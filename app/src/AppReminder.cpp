#include "AppReminder.h"

#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppLocalization.h"
#include "AppPaths.h"

#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QMenu>
#include <QSettings>
#include <QTime>

#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>
#endif

#if defined(Q_OS_WIN)
#include "AppStartupTask.h"
#endif

namespace
{
#if defined(Q_OS_ANDROID)
    constexpr const char* AndroidPlatformNotificationClass =
        "com/goldenphasmid/ikonika/PlatformNotification";

    void AndroidRequestNotificationPermission()
    {
        QJniObject Activity = QNativeInterface::QAndroidApplication::context();

        QJniObject::callStaticMethod<void>(AndroidPlatformNotificationClass, "requestPermission",
            "(Landroid/content/Context;)V", Activity.object<jobject>());
    }

    bool AndroidHasNotificationPermission()
    {
        QJniObject Activity = QNativeInterface::QAndroidApplication::context();

        return QJniObject::callStaticMethod<jboolean>(AndroidPlatformNotificationClass, "hasPermission",
            "(Landroid/content/Context;)Z", Activity.object<jobject>());
    }

    void AndroidNotify(const QString& Title, const QString& Text)
    {
        QJniObject Activity = QNativeInterface::QAndroidApplication::context();
        QJniObject JniTitle = QJniObject::fromString(Title);
        QJniObject JniText = QJniObject::fromString(Text);

        QJniObject::callStaticMethod<void>(AndroidPlatformNotificationClass, "notify",
            "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)V",
            Activity.object<jobject>(), JniTitle.object<jstring>(), JniText.object<jstring>());
    }

    constexpr const char* AndroidReminderSchedulerClass =
        "com/goldenphasmid/ikonika/ReminderScheduler";

    void AndroidScheduleReminder(int Hour, int Minute, const QString& Message)
    {
        QJniObject Activity = QNativeInterface::QAndroidApplication::context();
        QJniObject JniMessage = QJniObject::fromString(Message);

        QJniObject::callStaticMethod<void>(AndroidReminderSchedulerClass, "schedule",
            "(Landroid/content/Context;IILjava/lang/String;)V",
            Activity.object<jobject>(), Hour, Minute, JniMessage.object<jstring>());
    }

    void AndroidCancelReminder()
    {
        QJniObject Activity = QNativeInterface::QAndroidApplication::context();

        QJniObject::callStaticMethod<void>(AndroidReminderSchedulerClass, "cancel",
            "(Landroid/content/Context;)V", Activity.object<jobject>());
    }

    void AndroidMarkStudiedToday(int DayNumber)
    {
        QJniObject Activity = QNativeInterface::QAndroidApplication::context();

        QJniObject::callStaticMethod<void>(AndroidReminderSchedulerClass, "markStudiedToday",
            "(Landroid/content/Context;I)V", Activity.object<jobject>(), DayNumber);
    }
#endif

    /** Every minute. The exact second does not matter for a daily nudge, and polling survives the
        machine sleeping through a scheduled one-shot. */
    constexpr int ClockIntervalMilliseconds = 60 * 1000;

    /** Where Windows looks for programs to start at sign-in. Per-user, so it needs no admin
        rights, and removing the value is all it takes to undo. */
    constexpr const char* RunKeyPath =
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    constexpr const char* RunValueName = "Ikonika";

    /** Drawn rather than shipped as a file: the app has no artwork yet, and a tray with no icon
        does not appear at all. Replace this the moment there is a real one. */
    QIcon MakePlaceholderIcon()
    {
        QPixmap Canvas(32, 32);
        Canvas.fill(Qt::transparent);

        QPainter Painter(&Canvas);
        Painter.setRenderHint(QPainter::Antialiasing);
        Painter.setPen(Qt::NoPen);
        Painter.setBrush(QColor("#5b9cf8"));
        Painter.drawRoundedRect(2, 2, 28, 28, 6, 6);

        Painter.setPen(QColor("#000000"));

        QFont Font = Painter.font();
        Font.setPixelSize(20);
        Font.setBold(true);
        Painter.setFont(Font);

        Painter.drawText(QRect(0, 0, 32, 32), Qt::AlignCenter, QStringLiteral("i"));

        return QIcon(Canvas);
    }
}

AppReminder& AppReminder::Get()
{
    static AppReminder Instance;

    return Instance;
}

AppReminder::AppReminder(QObject* Parent)
    : QObject(Parent)
{
    Clock.setInterval(ClockIntervalMilliseconds);

    connect(&Clock, &QTimer::timeout, this, &AppReminder::CheckDue);
    connect(&AppLocalization::Get(), &AppLocalization::languageChanged,
            this, &AppReminder::SyncAndroidSchedule);
    connect(&AppDictionary::Notifier(), &AppDictionaryNotifier::languagesChanged,
            this, &AppReminder::OnLearningLanguageChanged);
}

void AppReminder::Start()
{
#if defined(Q_OS_ANDROID)
    if (IsEnabled() && !AndroidHasNotificationPermission())
    {
        AndroidRequestNotificationPermission();
    }

    SyncAndroidSchedule();

    qCInfo(LogApp) << "Reminder scheduled natively, enabled =" << IsEnabled();

    return;
#elif defined(Q_OS_IOS)
    qCInfo(LogApp) << "Reminders are not part of the iOS build yet, nothing is scheduled";
#else
    if (!IsTrayAvailable())
    {
        qCWarning(LogApp) << "No system tray on this desktop: the window will close normally and "
                             "the reminder only works while the app is open";

        Clock.start();

        return;
    }

    Tray = new QSystemTrayIcon(this);

    // The window's icon if the app ever gets one, otherwise a drawn placeholder: the tray refuses
    // to appear at all without an icon, so there has to be something.
    Tray->setIcon(QGuiApplication::windowIcon().isNull()
        ? MakePlaceholderIcon()
        : QGuiApplication::windowIcon());

    Tray->setToolTip(QStringLiteral("Ikonika"));

    QMenu* Menu = new QMenu();

    QAction* Open = Menu->addAction(tr("Open Ikonika"));
    connect(Open, &QAction::triggered, this, [this]() { emit showWindowRequested(); });

    Menu->addSeparator();

    // The only way out once closing hides the window, so this is not optional chrome.
    QAction* Quit = Menu->addAction(tr("Quit"));
    connect(Quit, &QAction::triggered, this, [this]() { emit quitRequested(); });

    Tray->setContextMenu(Menu);

    // Clicking the icon itself brings the window back, since that is what a tray icon does.
    connect(Tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason Reason)
    {
        if (Reason == QSystemTrayIcon::Trigger || Reason == QSystemTrayIcon::DoubleClick)
        {
            emit showWindowRequested();
        }
    });

    // Clicking the notification itself is the fastest path to actually studying.
    connect(Tray, &QSystemTrayIcon::messageClicked, this,
            [this]() { emit showWindowRequested(); });

    Tray->show();

    Clock.start();

    // Written on every launch, so the entry exists from the first run and follows the executable
    // when an update moves it.
    ApplyLaunchAtStartup(IsLaunchAtStartup());

    qCInfo(LogApp) << "Tray icon started, reminder enabled =" << IsEnabled();
#endif
}

bool AppReminder::ShouldHideOnClose() const
{
    // Without a tray there would be no way to get the window back, so closing has to mean closing.
    return IsTrayAvailable();
}

bool AppReminder::IsTrayAvailable() const
{
#if defined(Q_OS_ANDROID)
    return true;
#elif defined(Q_OS_IOS)
    return false;
#else
    return QSystemTrayIcon::isSystemTrayAvailable();
#endif
}

bool AppReminder::CanNotify() const
{
#if defined(Q_OS_ANDROID)
    return true;
#else
    return Tray != nullptr;
#endif
}

void AppReminder::CheckDue()
{
    if (!IsEnabled() || !CanNotify())
    {
        return;
    }

    const int Today = static_cast<int>(GetTodayDayNumber());

    if (LastNotifiedDay == Today)
    {
        return;
    }

    const QTime Now = QTime::currentTime();
    const int MinutesNow = (Now.hour() * 60) + Now.minute();

    if (MinutesNow < GetTimeMinutes())
    {
        return;
    }

    if (AppDatabase::Get().IsReady())
    {
        const int Due = AppDatabase::Get().GetCardRepository().CountDueCards(GetTodayDayNumber());
        const int Pool = AppDatabase::Get().GetCardRepository().CountUnlearnedCards();

        if (Due == 0 && Pool == 0)
        {
            return;
        }

        if (DayRequirementMet(Due))
        {
            return;
        }
    }

    LastNotifiedDay = Today;

    Notify(GetMessageOrDefault());
}

bool AppReminder::DayRequirementMet(int DueCount) const
{
    if (DueCount == 0)
    {
        return true;
    }

    const FTDayNumber Today = GetTodayDayNumber();
    const std::vector<FTDailyStats> TodayStats = AppDatabase::Get().GetCardRepository().LoadDailyStats(Today, Today);
    const int LearnedToday = TodayStats.empty() ? 0 : TodayStats.front().LearnedCount;
    const int DailyGoal = FTSettings::Get().GetIntensiveLearningRules().GetDailyGoal();

    return LearnedToday >= DailyGoal;
}

void AppReminder::Notify(const QString& Text)
{
    if (!CanNotify())
    {
        return;
    }

    qCInfo(LogApp) << "Reminder shown:" << Text;

#if defined(Q_OS_ANDROID)
    AndroidNotify(tr("Ikonika"), Text);
#else
    Tray->showMessage(tr("Ikonika"), Text, QSystemTrayIcon::Information, 10000);
#endif
}

QString AppReminder::debugTriggerCheck()
{
    if (!CanNotify())
    {
        return tr("Notifications are unavailable, so nothing can be shown.");
    }

    if (!IsEnabled())
    {
        return tr("The reminder is off.");
    }

    const QTime Now = QTime::currentTime();
    const int MinutesNow = (Now.hour() * 60) + Now.minute();

    if (MinutesNow < GetTimeMinutes())
    {
        return tr("Too early: set for %1, it is %2.")
            .arg(QTime(0, 0).addSecs(GetTimeMinutes() * 60).toString(QStringLiteral("HH:mm")),
                 Now.toString(QStringLiteral("HH:mm")));
    }

    if (AppDatabase::Get().IsReady())
    {
        const int Due = AppDatabase::Get().GetCardRepository().CountDueCards(GetTodayDayNumber());
        const int Pool = AppDatabase::Get().GetCardRepository().CountUnlearnedCards();

        if (Due == 0 && Pool == 0)
        {
            return tr("Nothing due and nothing waiting, so there is nothing to nag about.");
        }

        if (DayRequirementMet(Due))
        {
            return tr("Today's goal is already met, so it would stay quiet.");
        }
    }

    Notify(GetMessageOrDefault());

    return tr("Shown.");
}

QString AppReminder::showSample()
{
    if (!CanNotify())
    {
        qCWarning(LogApp) << "Sample reminder asked for with notifications unavailable";

        return tr("Notifications are unavailable, so nothing can be shown.");
    }

    const QString Text = GetMessageOrDefault();

    Notify(Text);

    return tr("Sent: \"%1\". If nothing appeared, check the system notification settings.")
        .arg(Text);
}

void AppReminder::syncDailyProgress()
{
#if defined(Q_OS_ANDROID)
    if (!AppDatabase::Get().IsReady())
    {
        return;
    }

    const int Due = AppDatabase::Get().GetCardRepository().CountDueCards(GetTodayDayNumber());

    if (!DayRequirementMet(Due))
    {
        return;
    }

    AndroidMarkStudiedToday(static_cast<int>(GetTodayDayNumber()));

    qCInfo(LogApp) << "Today's goal met, so the reminder will stay quiet";
#endif
}

void AppReminder::SyncAndroidSchedule()
{
#if defined(Q_OS_ANDROID)
    if (!IsEnabled())
    {
        AndroidCancelReminder();

        return;
    }

    const int Minutes = GetTimeMinutes();

    AndroidScheduleReminder(Minutes / 60, Minutes % 60, GetMessageOrDefault());
#endif
}

void AppReminder::OnLearningLanguageChanged()
{
    qCInfo(LogApp) << "Reminder text follows the new learning language:" << GetMessageOrDefault();

    SyncAndroidSchedule();

    emit settingsChanged();
}

void AppReminder::OnSettingsApplied()
{
    qCInfo(LogApp) << "Reminder re-reads its settings:" << IsEnabled() << GetTimeMinutes();

    LastNotifiedDay = -1;
    SyncAndroidSchedule();

    emit settingsChanged();
}

bool AppReminder::hasNotificationPermission() const
{
#if defined(Q_OS_ANDROID)
    return AndroidHasNotificationPermission();
#else
    return true;
#endif
}

void AppReminder::moveToBackground()
{
#if defined(Q_OS_ANDROID)
    QJniObject Activity = QNativeInterface::QAndroidApplication::context();
    Activity.callMethod<jboolean>("moveTaskToBack", "(Z)Z", true);
#endif
}

bool AppReminder::IsEnabled() const
{
    return FTSettings::Get().GetReminderRules().bEnabled;
}

void AppReminder::SetEnabled(bool bValue)
{
    if (IsEnabled() == bValue)
    {
        return;
    }

    FTSettings::Get().SetReminderEnabled(bValue);

#if defined(Q_OS_ANDROID)
    if (bValue)
    {
        AndroidRequestNotificationPermission();
    }
#endif

    SyncAndroidSchedule();

    emit settingsChanged();
}

int AppReminder::GetTimeMinutes() const
{
    return FTSettings::Get().GetReminderRules().TimeOfDayMinutes;
}

void AppReminder::SetTimeMinutes(int Minutes)
{
    if (GetTimeMinutes() == Minutes)
    {
        return;
    }

    FTSettings::Get().SetReminderTimeMinutes(Minutes);

    LastNotifiedDay = -1;
    SyncAndroidSchedule();

    emit settingsChanged();
}

QString AppReminder::GetMessage() const
{
    return QString::fromStdString(FTSettings::Get().GetReminderRules().Message);
}

void AppReminder::SetMessage(const QString& Text)
{
    if (GetMessage() == Text)
    {
        return;
    }

    FTSettings::Get().SetReminderMessage(Text.toStdString());

    SyncAndroidSchedule();

    emit settingsChanged();
}

QString AppReminder::GetMessageOrDefault() const
{
    const QString Message = GetMessage();

    if (!Message.isEmpty())
    {
        return Message;
    }

    return tr("Your %1 is waiting.")
        .arg(LanguageDisplayName(FTSettings::Get().GetLanguageRules().TargetLanguage));
}

bool AppReminder::IsLaunchAtStartup() const
{
    return FTSettings::Get().GetLaunchAtStartup();
}

void AppReminder::SetLaunchAtStartup(bool bValue)
{
    if (IsLaunchAtStartup() == bValue)
    {
        return;
    }

    FTSettings::Get().SetLaunchAtStartup(bValue);

    ApplyLaunchAtStartup(bValue);

    emit settingsChanged();
}

void AppReminder::ApplyLaunchAtStartup(bool bValue)
{
#if defined(Q_OS_ANDROID)
    Q_UNUSED(bValue);

    return;
#else
    // A sandboxed test run must not change what the user's own sign-in starts.
    if (AppPaths::IsHeadless())
    {
        qCInfo(LogApp) << "Headless run, leaving Windows startup as it is";

        return;
    }

#if defined(Q_OS_WIN)
    if (AppPaths::IsStorePackage())
    {
        AppStartupTask::Apply(bValue);

        return;
    }
#endif

    QSettings Run(QString::fromUtf8(RunKeyPath), QSettings::NativeFormat);

    if (!bValue)
    {
        Run.remove(QString::fromUtf8(RunValueName));

        qCInfo(LogApp) << "Removed from Windows startup";

        return;
    }

    // Quoted, since the path contains spaces on most installs, and with the flag that starts the
    // app hidden: launching at sign-in should not put a window in front of anyone.
    const QString Command = QStringLiteral("\"%1\" --minimised")
        .arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));

    Run.setValue(QString::fromUtf8(RunValueName), Command);

    qCInfo(LogApp) << "Added to Windows startup:" << Command;
#endif
}
