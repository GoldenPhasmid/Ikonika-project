#pragma once

#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QQuickItem>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <vector>

class QQuickWindow;

/**
 * Measurement instrument for any screen, driven from QML. The reader was the first thing it
 * measured and the dictionary is the second.
 *
 * Two things it provides that a running application cannot otherwise show. Wall-clock marks
 * around an action, so the cost of a zoom step is a number rather than an impression. And the
 * cadence of the frames the scene graph actually produced while that action ran, which is what
 * makes a stall visible: a mean frame time says little, the longest frame says everything.
 *
 * It also posts real wheel events into the window, so a zoom or a scroll can be exercised without
 * a hand on the mouse. The event goes through the same delivery the mouse would, so whatever
 * handler chain the view has is what answers it.
 */
class UiProbe : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit UiProbe(QObject* Parent = nullptr);

    /** True when the run was asked to measure the reader instead of being used. */
    Q_INVOKABLE bool benchmarkRequested() const;

    /** The document the measurement run opens. */
    Q_INVOKABLE QString benchmarkPath() const;

    /** What an environment variable holds, empty when it is unset. Each measurement run is asked
        for from the environment, and QML cannot read one on its own. */
    Q_INVOKABLE QString environmentVariable(const QString& Name) const;

    /** Starts a measured span. Every later mark reports against this. */
    Q_INVOKABLE void begin(const QString& Label);

    /** Logs the time since the span began and since the previous mark. */
    Q_INVOKABLE void mark(const QString& Label);

    /** Logs one absolute timestamp, so marks from separate files line up on the same clock. */
    Q_INVOKABLE void stamp(const QString& Label);

    /** Begins recording how long each frame took. */
    Q_INVOKABLE void startFrames(QQuickItem* Item, const QString& Label);

    /** Stops recording and logs the frame count, mean, worst and how many ran long. */
    Q_INVOKABLE void stopFrames();

    /** Posts one wheel event over the centre of an item, optionally with Ctrl held. */
    Q_INVOKABLE void wheel(QQuickItem* Item, int AngleDelta, bool bControl);

    /** Posts two press-release pairs at a point in an item, as a double-click. */
    Q_INVOKABLE void doubleClick(QQuickItem* Item, QPointF ItemPoint);

    Q_INVOKABLE void click(QQuickItem* Item);

    Q_INVOKABLE QQuickItem* findChild(QQuickItem* Root, const QString& ObjectName) const;

    Q_INVOKABLE int warningCount() const;
    Q_INVOKABLE void resetWarnings();
    Q_INVOKABLE void check(bool bCondition, const QString& Label) const;

    /**
     * Answers snapshot requests from outside the process, for reviewing screens while the desktop
     * is locked and the window is never painted. A file named request in the directory lists one
     * command per line: "size W H" resizes the window, "click name" clicks the visible item with
     * that object name, "hover name" moves the pointer over it, "key Return" and "type text" send keys, and any other line is an image
     * file name the window is rendered into.
     */
    Q_INVOKABLE void serveSnapshots(QQuickItem* Item, const QString& Directory);

private:
    void OnSnapshotPoll();
    QQuickItem* FindVisibleChild(QQuickItem* Root, const QString& ObjectName) const;

    QQuickWindow* SnapshotWindow = nullptr;
    QString SnapshotDirectory;
    Qt::MouseButtons SnapshotButtons = Qt::NoButton;
    QTimer* SnapshotPoll = nullptr;

    void OnFrameSwapped();

    QElapsedTimer SpanTimer;
    QString SpanLabel;
    qint64 LastMarkNanoseconds = 0;

    void OnFrameBegin();
    void OnFrameEnd();
    void OnHeartbeat();
    void LogSamples(const QString& What, std::vector<qint64>& Samples);

    QQuickWindow* FrameWindow = nullptr;
    QString FrameLabel;
    QElapsedTimer FrameTimer;
    QMutex FrameMutex;
    qint64 PreviousFrameNanoseconds = 0;
    qint64 FrameBeginNanoseconds = 0;
    std::vector<qint64> FrameIntervals;
    std::vector<qint64> FrameCosts;

    // The interface thread's own responsiveness. A timer that asks to run every few milliseconds
    // and does not is a thread that was busy, which is what the user feels as a frozen window,
    // whether or not the scene graph happened to be drawing at the time.
    QTimer* Heartbeat = nullptr;
    QElapsedTimer HeartbeatTimer;
    qint64 PreviousHeartbeatNanoseconds = 0;
    std::vector<qint64> HeartbeatGaps;
};

