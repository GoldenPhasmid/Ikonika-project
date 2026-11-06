#pragma once

#include <QString>

class AppIosNotifications
{
public:
    static void Install();
    static void RequestPermission();
    static bool HasPermission();

    static void ScheduleDaily(int Hour, int Minute, const QString& Title, const QString& Body, bool bSkipToday);
    static void CancelScheduled();
    static void NotifyNow(const QString& Title, const QString& Body);
};
