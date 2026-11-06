#pragma once

#include <QObject>
#include <QString>
#include <QQmlEngine>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

/**
 * The tray icon and the daily nudge.
 *
 * Closing the window hides it here rather than quitting, so the reminder survives: an app that
 * exits cannot remind anyone of anything. Quit lives in the tray menu, which is then the only way
 * out and has to stay there.
 *
 * Nothing fires on a day already studied. A reminder for work already done is how a user learns to
 * ignore reminders.
 */
class AppReminder : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ IsEnabled WRITE SetEnabled NOTIFY settingsChanged)

    /** Minutes from midnight. Split into hours and minutes only for showing. */
    Q_PROPERTY(int timeMinutes READ GetTimeMinutes WRITE SetTimeMinutes NOTIFY settingsChanged)

    /** What the reminder says. Empty means the default, which `messageOrDefault` resolves. */
    Q_PROPERTY(QString message READ GetMessage WRITE SetMessage NOTIFY settingsChanged)
    Q_PROPERTY(QString messageOrDefault READ GetMessageOrDefault NOTIFY settingsChanged)


    /** Starts the desktop app hidden in the tray when the user signs in. Separate from the reminder. */
    Q_PROPERTY(bool launchAtStartup READ IsLaunchAtStartup WRITE SetLaunchAtStartup NOTIFY settingsChanged)

    /** False where the desktop has no tray, in which case hiding the window would lose it. */
    Q_PROPERTY(bool trayAvailable READ IsTrayAvailable CONSTANT)

public:
    explicit AppReminder(QObject* Parent = nullptr);

    /**
     * The one instance, owned here rather than by QML. Main has to reach it to wire the window's
     * close and the tray's Open together before any QML exists, and a QML-created singleton would
     * be a second object running its own clock.
     */
    static AppReminder& Get();

    /** Builds the tray icon and starts the clock. Called once the window exists, since the tray's
        menu has to be able to show it again. */
    void Start();

    /** True when the window should hide instead of closing. False without a tray, where hiding
        would leave no way back to it. */
    bool ShouldHideOnClose() const;

    /**
     * Shows the reminder now, whatever the time, for trying out a message. Returns what happened.
     *
     * Says so rather than returning nothing, because the failure it is most likely to hit is
     * invisible: a notification the desktop accepted and then chose not to draw. Without a word
     * back, pressing this and seeing nothing is indistinguishable from a broken button.
     */
    Q_INVOKABLE QString showSample();

    /**
     * Runs the real due check as the clock would, and says what happened. A sample notification
     * proves only that the tray works; this exercises the gates that decide whether one fires at
     * all, which is where the interesting failures are.
     *
     * The once-a-day guard is lifted for this call, so it can be pressed twice.
     */
    Q_INVOKABLE QString debugTriggerCheck();

    Q_INVOKABLE void syncDailyProgress();

    Q_INVOKABLE bool hasNotificationPermission() const;

    Q_INVOKABLE void moveToBackground();

    bool IsEnabled() const;
    void SetEnabled(bool bValue);

    int GetTimeMinutes() const;
    void SetTimeMinutes(int Minutes);

    QString GetMessage() const;
    void SetMessage(const QString& Text);
    QString GetMessageOrDefault() const;

    bool IsTrayAvailable() const;

signals:
    void settingsChanged();

    /** The tray asked for the window back. */
    void showWindowRequested();

    /** The tray's Quit was chosen, which is the only way out once close hides. */
    void quitRequested();

private:
    /** Fires the notification if today's time has passed and nothing was studied. */
    void CheckDue();

    bool DayRequirementMet(int DueCount) const;

    void Notify(const QString& Text);

    bool CanNotify() const;

    void ApplyLaunchAtStartup(bool bValue);

    bool IsLaunchAtStartup() const;
    void SetLaunchAtStartup(bool bValue);

    void SyncAndroidSchedule();
    void OnLearningLanguageChanged();
    void OnSettingsApplied();

    QSystemTrayIcon* Tray = nullptr;

    /** Checked on a coarse interval rather than scheduled for the exact minute: the machine can
        sleep through a one-shot timer, and a minute of lateness on a daily nudge is nothing. */
    QTimer Clock;

    /** The day a notification was last shown, so it fires once rather than every check. */
    int LastNotifiedDay = -1;
};

/**
 * Registers AppReminder::Get()'s instance as the QML singleton, rather than making AppReminder
 * itself QML_SINGLETON.
 *
 * AppReminder needs a public constructor so Get()'s function-local static can exist, but a
 * QML_SINGLETON QML_ELEMENT type with a public constructor is exactly the shape QML's engine uses
 * its own default-construction path for, on top of whatever create() returns - the one true
 * instance from Get() and a second, QML-owned one from that other path. QML_FOREIGN with an
 * explicit static instance and no public constructor in sight leaves the engine only one way to
 * get an AppReminder at all.
 */
struct AppReminderSingleton
{
    Q_GADGET
    QML_FOREIGN(AppReminder)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppReminder)

public:
    static AppReminder* create(QQmlEngine* Engine, QJSEngine*)
    {
        AppReminder& Instance = AppReminder::Get();

        // Owned by Get()'s static, so QML must not delete it.
        QJSEngine::setObjectOwnership(&Instance, QJSEngine::CppOwnership);

        return &Instance;
    }
};
