#include "AppStartup.h"

#include "core/FTLog.h"

#include <QElapsedTimer>
#include <QQuickWindow>
#include <QString>
#include <QStringList>

#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#if defined(Q_OS_ANDROID)
#include <QtCore/qjniobject.h>
#endif

namespace
{
    struct Stage
    {
        const char* Name = nullptr;
        qint64 Microseconds = 0;
    };

    QElapsedTimer Clock;
    std::vector<Stage> Stages;
    qint64 PreviousMicroseconds = 0;

    /** Milliseconds the process spent in the loader before main ran, or -1 where that cannot be
        asked. Mapping Qt, the ONNX runtime and the speech libraries is real time the app is
        responsible for and nothing inside main can see it. */
    qint64 LoaderMilliseconds()
    {
#ifdef Q_OS_WIN
        FILETIME Creation;
        FILETIME Exit;
        FILETIME Kernel;
        FILETIME User;

        if (GetProcessTimes(GetCurrentProcess(), &Creation, &Exit, &Kernel, &User) == 0)
        {
            return -1;
        }

        FILETIME Now;
        GetSystemTimeAsFileTime(&Now);

        const auto ToTicks = [](const FILETIME& Value)
        {
            return (static_cast<quint64>(Value.dwHighDateTime) << 32) | Value.dwLowDateTime;
        };

        // FILETIME counts 100-nanosecond intervals.
        return static_cast<qint64>((ToTicks(Now) - ToTicks(Creation)) / 10000);
#else
        return -1;
#endif
    }

    qint64 LoaderAtStart = -1;

    QString Breakdown()
    {
        QStringList Parts;

        if (LoaderAtStart >= 0)
        {
            Parts.append(QStringLiteral("loader %1 ms").arg(LoaderAtStart));
        }

        for (const Stage& Entry : Stages)
        {
            Parts.append(QStringLiteral("%1 %2 ms")
                             .arg(QString::fromUtf8(Entry.Name))
                             .arg(Entry.Microseconds / 1000));
        }

        return Parts.join(QStringLiteral(", "));
    }

    qint64 TotalMilliseconds()
    {
        return (LoaderAtStart > 0 ? LoaderAtStart : 0) + Clock.elapsed();
    }
}

namespace AppStartup
{
    void Begin()
    {
        LoaderAtStart = LoaderMilliseconds();

        Clock.start();
        PreviousMicroseconds = 0;
        Stages.clear();
    }

    void Mark(const char* Stage)
    {
        if (!Clock.isValid())
        {
            return;
        }

        const qint64 Elapsed = Clock.nsecsElapsed() / 1000;

        Stages.push_back({Stage, Elapsed - PreviousMicroseconds});
        PreviousMicroseconds = Elapsed;
    }

    void ReportReady()
    {
        if (!Clock.isValid())
        {
            return;
        }

        qCInfo(LogApp) << "Startup to QML ready:" << TotalMilliseconds() << "ms |"
                       << Breakdown();
    }

    void ReportFirstFrame(QQuickWindow* Window)
    {
        if (Window == nullptr || !Clock.isValid())
        {
            return;
        }

        // Held by shared pointer so the lambda can disconnect itself: the signal fires on every
        // frame for the life of the window and only the first one is a startup measurement.
        auto Connection = std::make_shared<QMetaObject::Connection>();

        *Connection = QObject::connect(Window, &QQuickWindow::frameSwapped, Window,
            [Connection]()
            {
                QObject::disconnect(*Connection);

                qCInfo(LogApp) << "Startup to first frame:" << TotalMilliseconds() << "ms |"
                               << Breakdown();

#if defined(Q_OS_ANDROID)
                QJniObject::callStaticMethod<void>("com/goldenphasmid/ikonika/SplashGate",
                    "markReady", "()V");
#endif
            });
    }
}
