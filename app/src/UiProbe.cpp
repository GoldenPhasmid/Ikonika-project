#include "UiProbe.h"

#include "core/FTLog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPointF>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWheelEvent>

#include <algorithm>
#include <numeric>

namespace
{
    /** A frame this long or longer is one the user sees as a stutter at 60 Hz. */
    constexpr double LongFrameMilliseconds = 20.0;

    double ToMilliseconds(qint64 Nanoseconds)
    {
        return static_cast<double>(Nanoseconds) / 1000000.0;
    }
}

UiProbe::UiProbe(QObject* Parent)
    : QObject(Parent)
{
}

bool UiProbe::benchmarkRequested() const
{
    return !qEnvironmentVariableIsEmpty("IKONIKA_READER_BENCHMARK");
}

QString UiProbe::benchmarkPath() const
{
    return qEnvironmentVariable("IKONIKA_READER_BENCHMARK");
}

QString UiProbe::environmentVariable(const QString& Name) const
{
    return qEnvironmentVariable(Name.toUtf8().constData());
}

void UiProbe::begin(const QString& Label)
{
    SpanLabel = Label;
    SpanTimer.start();
    LastMarkNanoseconds = 0;

    qCInfo(LogApp).noquote() << "PROBE" << Label << "begin";
}

void UiProbe::mark(const QString& Label)
{
    if (!SpanTimer.isValid())
    {
        return;
    }

    const qint64 Now = SpanTimer.nsecsElapsed();
    const double SinceBegin = ToMilliseconds(Now);
    const double SinceLast = ToMilliseconds(Now - LastMarkNanoseconds);
    LastMarkNanoseconds = Now;

    qCInfo(LogApp).noquote()
        << "PROBE" << SpanLabel << "|" << Label
        << "| +" << QString::number(SinceLast, 'f', 1) << "ms"
        << "| total" << QString::number(SinceBegin, 'f', 1) << "ms";
}

void UiProbe::stamp(const QString& Label)
{
    static QElapsedTimer ProcessTimer = []
    {
        QElapsedTimer Timer;
        Timer.start();

        return Timer;
    }();

    qCInfo(LogApp).noquote()
        << "PROBE stamp" << QString::number(ToMilliseconds(ProcessTimer.nsecsElapsed()), 'f', 1)
        << "|" << Label;
}

void UiProbe::startFrames(QQuickItem* Item, const QString& Label)
{
    stopFrames();

    if (!Item || !Item->window())
    {
        qCWarning(LogApp) << "PROBE: no window to record frames from";

        return;
    }

    FrameWindow = Item->window();
    FrameLabel = Label;

    {
        QMutexLocker Locker(&FrameMutex);
        FrameIntervals.clear();
        FrameCosts.clear();
        PreviousFrameNanoseconds = 0;
        FrameBeginNanoseconds = 0;
        FrameTimer.start();
    }

    // Direct, so the timestamps are taken on the render thread as the frame happens. Queued, they
    // would be taken whenever the interface thread got round to it, which is the very thing under
    // measurement.
    connect(FrameWindow, &QQuickWindow::frameSwapped,
            this, &UiProbe::OnFrameSwapped, Qt::DirectConnection);
    connect(FrameWindow, &QQuickWindow::beforeSynchronizing,
            this, &UiProbe::OnFrameBegin, Qt::DirectConnection);
    connect(FrameWindow, &QQuickWindow::afterRendering,
            this, &UiProbe::OnFrameEnd, Qt::DirectConnection);

    HeartbeatGaps.clear();
    PreviousHeartbeatNanoseconds = 0;
    HeartbeatTimer.start();

    if (!Heartbeat)
    {
        Heartbeat = new QTimer(this);
        Heartbeat->setTimerType(Qt::PreciseTimer);
        Heartbeat->setInterval(4);
        connect(Heartbeat, &QTimer::timeout, this, &UiProbe::OnHeartbeat);
    }

    Heartbeat->start();
}

void UiProbe::OnFrameBegin()
{
    QMutexLocker Locker(&FrameMutex);

    if (FrameTimer.isValid())
    {
        FrameBeginNanoseconds = FrameTimer.nsecsElapsed();
    }
}

void UiProbe::OnFrameEnd()
{
    QMutexLocker Locker(&FrameMutex);

    if (FrameTimer.isValid() && FrameBeginNanoseconds > 0)
    {
        FrameCosts.push_back(FrameTimer.nsecsElapsed() - FrameBeginNanoseconds);
        FrameBeginNanoseconds = 0;
    }
}

void UiProbe::OnHeartbeat()
{
    if (!HeartbeatTimer.isValid())
    {
        return;
    }

    const qint64 Now = HeartbeatTimer.nsecsElapsed();

    if (PreviousHeartbeatNanoseconds > 0)
    {
        HeartbeatGaps.push_back(Now - PreviousHeartbeatNanoseconds);
    }

    PreviousHeartbeatNanoseconds = Now;
}

void UiProbe::OnFrameSwapped()
{
    QMutexLocker Locker(&FrameMutex);

    if (!FrameTimer.isValid())
    {
        return;
    }

    const qint64 Now = FrameTimer.nsecsElapsed();

    if (PreviousFrameNanoseconds > 0)
    {
        FrameIntervals.push_back(Now - PreviousFrameNanoseconds);
    }

    PreviousFrameNanoseconds = Now;
}

void UiProbe::stopFrames()
{
    if (!FrameWindow)
    {
        return;
    }

    disconnect(FrameWindow, nullptr, this, nullptr);
    FrameWindow = nullptr;

    if (Heartbeat)
    {
        Heartbeat->stop();
    }

    std::vector<qint64> Intervals;
    std::vector<qint64> Costs;

    {
        QMutexLocker Locker(&FrameMutex);
        Intervals.swap(FrameIntervals);
        Costs.swap(FrameCosts);
        FrameTimer.invalidate();
    }

    HeartbeatTimer.invalidate();

    LogSamples(QStringLiteral("gap between frames"), Intervals);
    LogSamples(QStringLiteral("cost of a frame"), Costs);
    LogSamples(QStringLiteral("interface thread stall"), HeartbeatGaps);

    HeartbeatGaps.clear();
}

void UiProbe::LogSamples(const QString& What, std::vector<qint64>& Samples)
{
    if (Samples.empty())
    {
        qCInfo(LogApp).noquote() << "PROBE" << FrameLabel << "|" << What << "| nothing recorded";

        return;
    }

    std::sort(Samples.begin(), Samples.end());

    const qint64 Total = std::accumulate(Samples.begin(), Samples.end(), qint64{0});
    const double Mean = ToMilliseconds(Total / static_cast<qint64>(Samples.size()));
    const double Median = ToMilliseconds(Samples[Samples.size() / 2]);
    const double Worst = ToMilliseconds(Samples.back());
    const double NinetyFifth = ToMilliseconds(Samples[(Samples.size() * 95) / 100]);

    const auto LongOnes = std::count_if(Samples.begin(), Samples.end(), [](qint64 Sample)
    {
        return ToMilliseconds(Sample) >= LongFrameMilliseconds;
    });

    qCInfo(LogApp).noquote()
        << "PROBE" << FrameLabel << "|" << What
        << "| count" << Samples.size()
        << "| mean" << QString::number(Mean, 'f', 1)
        << "| median" << QString::number(Median, 'f', 1)
        << "| p95" << QString::number(NinetyFifth, 'f', 1)
        << "| worst" << QString::number(Worst, 'f', 1)
        << "| over 20ms" << LongOnes;
}

void UiProbe::wheel(QQuickItem* Item, int AngleDelta, bool bControl)
{
    if (!Item || !Item->window())
    {
        qCWarning(LogApp) << "PROBE: nothing to send a wheel event to";

        return;
    }

    QQuickWindow* Window = Item->window();
    const QPointF Centre(Item->width() / 2, Item->height() / 2);
    const QPointF ScenePosition = Item->mapToScene(Centre);
    const QPointF GlobalPosition = Window->mapToGlobal(ScenePosition);

    QWheelEvent Event(ScenePosition, GlobalPosition,
                      QPoint(0, 0), QPoint(0, AngleDelta),
                      Qt::NoButton,
                      bControl ? Qt::ControlModifier : Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    Event.setTimestamp(static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()));

    QCoreApplication::sendEvent(Window, &Event);
}

namespace
{
    std::atomic<int> ProbeWarningCount{0};
    QtMessageHandler PreviousMessageHandler = nullptr;

    void ProbeMessageHandler(QtMsgType Type, const QMessageLogContext& Context, const QString& Message)
    {
        if (Type == QtWarningMsg || Type == QtCriticalMsg)
        {
            ++ProbeWarningCount;
        }

        if (PreviousMessageHandler)
        {
            PreviousMessageHandler(Type, Context, Message);
        }
    }
}

int UiProbe::warningCount() const
{
    return ProbeWarningCount.load();
}

void UiProbe::resetWarnings()
{
    static bool bInstalled = false;

    if (!bInstalled)
    {
        bInstalled = true;
        PreviousMessageHandler = qInstallMessageHandler(&ProbeMessageHandler);
    }

    ProbeWarningCount.store(0);
}

void UiProbe::click(QQuickItem* Item)
{
    if (!Item || !Item->window())
    {
        qCWarning(LogApp) << "PROBE: nothing to click on";

        return;
    }

    QQuickWindow* Window = Item->window();
    const QPointF Centre(Item->width() / 2, Item->height() / 2);
    const QPointF ScenePosition = Item->mapToScene(Centre);
    const QPointF GlobalPosition = Window->mapToGlobal(ScenePosition);

    QMouseEvent Press(QEvent::MouseButtonPress, ScenePosition, ScenePosition, GlobalPosition,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(Window, &Press);

    QMouseEvent Release(QEvent::MouseButtonRelease, ScenePosition, ScenePosition,
                        GlobalPosition, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(Window, &Release);
}

QQuickItem* UiProbe::findChild(QQuickItem* Root, const QString& ObjectName) const
{
    if (!Root)
    {
        return nullptr;
    }

    for (QQuickItem* Child : Root->childItems())
    {
        if (Child->objectName() == ObjectName)
        {
            return Child;
        }

        if (QQuickItem* Found = findChild(Child, ObjectName))
        {
            return Found;
        }
    }

    return nullptr;
}

void UiProbe::check(bool bCondition, const QString& Label) const
{
    if (bCondition)
    {
        qCInfo(LogApp).noquote() << "NAV PASS" << Label;
    }
    else
    {
        qCWarning(LogApp).noquote() << "NAV FAIL" << Label;
    }
}

void UiProbe::doubleClick(QQuickItem* Item, QPointF ItemPoint)
{
    if (!Item || !Item->window())
    {
        qCWarning(LogApp) << "PROBE: nothing to click on";

        return;
    }

    QQuickWindow* Window = Item->window();
    const QPointF ScenePosition = Item->mapToScene(ItemPoint);
    const QPointF GlobalPosition = Window->mapToGlobal(ScenePosition);

    // Two press-release pairs rather than a synthesised double-click event, because that is what a
    // mouse sends and what the tap handler counts.
    for (int Click = 0; Click < 2; ++Click)
    {
        QMouseEvent Press(QEvent::MouseButtonPress, ScenePosition, ScenePosition, GlobalPosition,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(Window, &Press);

        QMouseEvent Release(QEvent::MouseButtonRelease, ScenePosition, ScenePosition,
                            GlobalPosition, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(Window, &Release);
    }
}

void UiProbe::serveSnapshots(QQuickItem* Item, const QString& Directory)
{
    if (!Item || !Item->window() || Directory.isEmpty())
    {
        qCWarning(LogApp) << "PROBE: snapshots need a window and a directory";

        return;
    }

    SnapshotWindow = Item->window();
    SnapshotDirectory = Directory;

    if (!SnapshotPoll)
    {
        SnapshotPoll = new QTimer(this);
        SnapshotPoll->setInterval(300);
        connect(SnapshotPoll, &QTimer::timeout, this, &UiProbe::OnSnapshotPoll);
    }

    SnapshotPoll->start();

    qCInfo(LogApp) << "PROBE: serving snapshots from" << Directory;
}

QQuickItem* UiProbe::FindVisibleChild(QQuickItem* Root, const QString& ObjectName) const
{
    if (!Root || !Root->isVisible())
    {
        return nullptr;
    }

    for (QQuickItem* Child : Root->childItems())
    {
        if (Child->isVisible() && Child->objectName() == ObjectName)
        {
            return Child;
        }

        if (QQuickItem* Found = FindVisibleChild(Child, ObjectName))
        {
            return Found;
        }
    }

    return nullptr;
}

void UiProbe::OnSnapshotPoll()
{
    QFile Request(QDir(SnapshotDirectory).filePath(QStringLiteral("request")));

    if (!SnapshotWindow || !Request.exists() || !Request.open(QIODevice::ReadOnly))
    {
        return;
    }

    const QStringList Lines = QString::fromUtf8(Request.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    Request.close();
    Request.remove();

    for (const QString& Line : Lines)
    {
        const QStringList Parts = Line.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);

        if (Parts.size() >= 2 && Parts[0] == QStringLiteral("window"))
        {
            const QString Title = Line.trimmed().mid(Parts[0].size() + 1);

            for (QWindow* Candidate : QGuiApplication::topLevelWindows())
            {
                QQuickWindow* QuickWindow = qobject_cast<QQuickWindow*>(Candidate);

                if (QuickWindow != nullptr && QuickWindow->isVisible() && QuickWindow->title() == Title)
                {
                    SnapshotWindow = QuickWindow;
                    qCInfo(LogApp) << "PROBE: snapshot window" << Title;

                    break;
                }
            }

            continue;
        }

        if (Parts.size() == 3 && Parts[0] == QStringLiteral("size"))
        {
            SnapshotWindow->resize(Parts[1].toInt(), Parts[2].toInt());

            continue;
        }

        if (Parts.size() == 2 && Parts[0] == QStringLiteral("click"))
        {
            click(FindVisibleChild(SnapshotWindow->contentItem(), Parts[1]));

            continue;
        }

        if (Parts.size() == 2 && Parts[0] == QStringLiteral("hover"))
        {
            QQuickItem* Target = FindVisibleChild(SnapshotWindow->contentItem(), Parts[1]);

            if (Target)
            {
                const QPointF ScenePosition = Target->mapToScene(QPointF(Target->width() / 2, Target->height() / 2));
                QMouseEvent Move(QEvent::MouseMove, ScenePosition, ScenePosition, SnapshotWindow->mapToGlobal(ScenePosition),
                                 Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(SnapshotWindow, &Move);
            }

            continue;
        }

        if (Parts.size() == 3 && Parts[0] == QStringLiteral("wheel"))
        {
            wheel(FindVisibleChild(SnapshotWindow->contentItem(), Parts[1]), Parts[2].toInt(), false);

            continue;
        }

        const bool bMiddle = Parts[0].startsWith(QStringLiteral("middle"));
        const bool bLeft = Parts[0].startsWith(QStringLiteral("left"));
        const bool bDown = (bMiddle || bLeft) && Parts[0].endsWith(QStringLiteral("down"));
        const bool bUp = (bMiddle || bLeft) && Parts[0].endsWith(QStringLiteral("up"));

        if (Parts.size() == 4 && (bDown || bUp || Parts[0] == QStringLiteral("moveat")))
        {
            QQuickItem* Target = FindVisibleChild(SnapshotWindow->contentItem(), Parts[1]);

            if (Target)
            {
                const QPointF ScenePosition = Target->mapToScene(
                    QPointF(Target->width() * Parts[2].toDouble(), Target->height() * Parts[3].toDouble()));
                const QEvent::Type Type = bDown ? QEvent::MouseButtonPress
                                        : bUp ? QEvent::MouseButtonRelease : QEvent::MouseMove;
                const Qt::MouseButton Button = bMiddle ? Qt::MiddleButton : bLeft ? Qt::LeftButton : Qt::NoButton;

                if (bDown)
                {
                    SnapshotButtons |= Button;
                }

                if (bUp)
                {
                    SnapshotButtons &= ~Qt::MouseButtons(Button);
                }

                QMouseEvent Event(Type, ScenePosition, ScenePosition, SnapshotWindow->mapToGlobal(ScenePosition),
                                  Button, SnapshotButtons, Qt::NoModifier);
                QCoreApplication::sendEvent(SnapshotWindow, &Event);
            }

            continue;
        }

        if (Parts.size() == 4 && Parts[0] == QStringLiteral("clickat"))
        {
            QQuickItem* Target = FindVisibleChild(SnapshotWindow->contentItem(), Parts[1]);

            if (Target)
            {
                doubleClick(Target, QPointF(Target->width() * Parts[2].toDouble(), Target->height() * Parts[3].toDouble()));
            }

            continue;
        }

        if (Parts.size() >= 2 && Parts[0] == QStringLiteral("key"))
        {
            const QKeyCombination Combination = QKeySequence::fromString(Parts[1])[0];
            const QString Text = Combination.key() == Qt::Key_Space ? QStringLiteral(" ") : QString();

            QKeyEvent Press(QEvent::KeyPress, Combination.key(), Combination.keyboardModifiers(), Text);
            QCoreApplication::sendEvent(SnapshotWindow, &Press);

            QKeyEvent Release(QEvent::KeyRelease, Combination.key(), Combination.keyboardModifiers(), Text);
            QCoreApplication::sendEvent(SnapshotWindow, &Release);

            continue;
        }

        if (Parts.size() >= 2 && (Parts[0] == QStringLiteral("keydown") || Parts[0] == QStringLiteral("keyup")))
        {
            const bool bControl = Parts[1] == QStringLiteral("Ctrl");
            const int Key = bControl ? Qt::Key_Control : QKeySequence::fromString(Parts[1])[0].key();
            const bool bDown = Parts[0] == QStringLiteral("keydown");
            const Qt::KeyboardModifiers Modifiers = bControl && bDown ? Qt::ControlModifier : Qt::NoModifier;

            QKeyEvent Event(bDown ? QEvent::KeyPress : QEvent::KeyRelease, Key, Modifiers);
            QCoreApplication::sendEvent(SnapshotWindow, &Event);

            continue;
        }

        if (Parts.size() >= 2 && Parts[0] == QStringLiteral("type"))
        {
            for (const QChar Character : Line.trimmed().mid(5))
            {
                QKeyEvent Press(QEvent::KeyPress, 0, Qt::NoModifier, QString(Character));
                QCoreApplication::sendEvent(SnapshotWindow, &Press);
            }

            continue;
        }

        if (Parts.size() != 1)
        {
            continue;
        }

        const QString Path = QDir(SnapshotDirectory).filePath(Parts[0]);
        const bool bSaved = SnapshotWindow->grabWindow().save(Path);

        qCInfo(LogApp) << "PROBE: snapshot" << Path << (bSaved ? "saved" : "failed");
    }
}
