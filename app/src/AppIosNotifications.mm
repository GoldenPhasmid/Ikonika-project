#include "AppIosNotifications.h"

#include "core/FTLog.h"

#include <QDateTime>

#include <atomic>

#import <UserNotifications/UserNotifications.h>

@interface IkonikaNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation IkonikaNotificationDelegate

- (void)userNotificationCenter:(UNUserNotificationCenter*)Center
       willPresentNotification:(UNNotification*)Notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))CompletionHandler
{
    CompletionHandler(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionList
                      | UNNotificationPresentationOptionSound);
}

@end

namespace
{
    constexpr int ScheduledDays = 14;

    std::atomic<bool> bAuthorized{false};

    NSString* ReminderIdentifier(int DayOffset)
    {
        return [NSString stringWithFormat:@"ikonika-reminder-%d", DayOffset];
    }

    UNMutableNotificationContent* MakeContent(const QString& Title, const QString& Body)
    {
        UNMutableNotificationContent* Content = [[[UNMutableNotificationContent alloc] init] autorelease];
        Content.title = Title.toNSString();
        Content.body = Body.toNSString();
        Content.sound = [UNNotificationSound defaultSound];

        return Content;
    }

    void Add(NSString* Identifier, UNMutableNotificationContent* Content, UNNotificationTrigger* Trigger)
    {
        UNNotificationRequest* Request = [UNNotificationRequest requestWithIdentifier:Identifier
                                                                              content:Content
                                                                              trigger:Trigger];

        [[UNUserNotificationCenter currentNotificationCenter]
            addNotificationRequest:Request
             withCompletionHandler:^(NSError* Error)
        {
            if (Error != nil)
            {
                qCWarning(LogApp) << "Could not schedule a notification:"
                                  << QString::fromNSString(Error.localizedDescription);
            }
        }];
    }

    void RefreshPermission()
    {
        [[UNUserNotificationCenter currentNotificationCenter]
            getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings* Settings)
        {
            const UNAuthorizationStatus Status = Settings.authorizationStatus;

            bAuthorized = Status == UNAuthorizationStatusAuthorized || Status == UNAuthorizationStatusProvisional;

            qCInfo(LogApp) << "Notification permission status:" << static_cast<int>(Status);
        }];
    }
}

void AppIosNotifications::Install()
{
    static IkonikaNotificationDelegate* Delegate = [[IkonikaNotificationDelegate alloc] init];

    [UNUserNotificationCenter currentNotificationCenter].delegate = Delegate;

    RefreshPermission();
}

void AppIosNotifications::RequestPermission()
{
    const UNAuthorizationOptions Options = UNAuthorizationOptionAlert | UNAuthorizationOptionSound;

    [[UNUserNotificationCenter currentNotificationCenter]
        requestAuthorizationWithOptions:Options
                      completionHandler:^(BOOL bGranted, NSError* Error)
    {
        bAuthorized = bGranted;

        if (Error != nil)
        {
            qCWarning(LogApp) << "Notification permission request failed:"
                              << QString::fromNSString(Error.localizedDescription);

            return;
        }

        qCInfo(LogApp) << "Notification permission granted =" << static_cast<bool>(bGranted);
    }];
}

bool AppIosNotifications::HasPermission()
{
    return bAuthorized;
}

void AppIosNotifications::CancelScheduled()
{
    NSMutableArray<NSString*>* Identifiers = [NSMutableArray arrayWithCapacity:ScheduledDays];

    for (int DayOffset = 0; DayOffset < ScheduledDays; ++DayOffset)
    {
        [Identifiers addObject:ReminderIdentifier(DayOffset)];
    }

    [[UNUserNotificationCenter currentNotificationCenter] removePendingNotificationRequestsWithIdentifiers:Identifiers];
}

void AppIosNotifications::ScheduleDaily(int Hour, int Minute, const QString& Title, const QString& Body,
                                        bool bSkipToday)
{
    CancelScheduled();

    const QDateTime Now = QDateTime::currentDateTime();
    int Scheduled = 0;

    for (int DayOffset = 0; DayOffset < ScheduledDays; ++DayOffset)
    {
        const QDate Day = Now.date().addDays(DayOffset);
        const QDateTime When(Day, QTime(Hour, Minute));

        if (When <= Now || (DayOffset == 0 && bSkipToday))
        {
            continue;
        }

        NSDateComponents* Components = [[[NSDateComponents alloc] init] autorelease];
        Components.year = Day.year();
        Components.month = Day.month();
        Components.day = Day.day();
        Components.hour = Hour;
        Components.minute = Minute;

        UNCalendarNotificationTrigger* Trigger =
            [UNCalendarNotificationTrigger triggerWithDateMatchingComponents:Components repeats:NO];

        Add(ReminderIdentifier(DayOffset), MakeContent(Title, Body), Trigger);
        ++Scheduled;
    }

    qCInfo(LogApp) << "Scheduled" << Scheduled << "daily reminders at" << Hour << ":" << Minute
                   << ", today skipped =" << bSkipToday;
}

void AppIosNotifications::NotifyNow(const QString& Title, const QString& Body)
{
    UNTimeIntervalNotificationTrigger* Trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:1
                                                                                                    repeats:NO];

    Add(@"ikonika-reminder-now", MakeContent(Title, Body), Trigger);
}
