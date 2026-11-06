#include "AppLog.h"

#include "AppPaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMutex>
#include <QTextStream>

namespace
{
    QFile& LogFile()
    {
        static QFile File;

        return File;
    }

    QMutex& LogMutex()
    {
        static QMutex Mutex;

        return Mutex;
    }

    // One line per message, timestamped and tagged with the category, the way UE_LOG reads. Errors
    // and warnings also go to stderr, so a console run still shows them without opening the file.
    void WriteToFile(QtMsgType Type, const QMessageLogContext& Context, const QString& Message)
    {
        static const char* TypeNames[] = {"debug", "warning", "critical", "fatal", "info"};

        const QString Line = QStringLiteral("[%1] %2 %3: %4")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODate),
                 QString::fromUtf8(TypeNames[static_cast<int>(Type)]),
                 QString::fromUtf8(Context.category != nullptr ? Context.category : "default"),
                 Message);

        if (LogFile().isOpen())
        {
            QMutexLocker Locker(&LogMutex());
            QTextStream Stream(&LogFile());
            Stream << Line << Qt::endl;
        }

        if (Type == QtWarningMsg || Type == QtCriticalMsg || Type == QtFatalMsg)
        {
            fprintf(stderr, "%s\n", qUtf8Printable(Line));
        }
    }
}

namespace AppLog
{
    void Install()
    {
        // Under the checkout in a debug build, under per-user local data in a release one. See
        // AppPaths for why the two are kept apart.
        const QDir Directory(AppPaths::LogsDirectory());
        QDir().mkpath(Directory.absolutePath());

        const QString CurrentPath = Directory.filePath(QStringLiteral("Ikonika.log"));

        // Same convention as UE_LOG: the previous run's log is kept, renamed with its own last-
        // modified time, rather than appended to or overwritten outright.
        if (QFile::exists(CurrentPath))
        {
            const QDateTime Modified = QFileInfo(CurrentPath).lastModified();
            const QString BackupPath = Directory.filePath(QStringLiteral("Ikonika-%1.log")
                .arg(Modified.toString(QStringLiteral("yyyy.MM.dd-HH.mm.ss"))));

            QFile::remove(BackupPath);
            QFile::rename(CurrentPath, BackupPath);
        }

        LogFile().setFileName(CurrentPath);
        LogFile().open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);

        qInstallMessageHandler(WriteToFile);

#if !defined(QT_DEBUG)
        QLoggingCategory::setFilterRules(QStringLiteral("ikonika.*.debug=false"));
#endif
    }
}
