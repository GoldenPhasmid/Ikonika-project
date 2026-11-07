#include "AppIosBackgroundTask.h"

#include "core/FTLog.h"

#import <UIKit/UIKit.h>

unsigned long AppIosBackgroundTask::Begin(const QString& Name)
{
    __block UIBackgroundTaskIdentifier Task = UIBackgroundTaskInvalid;
    const QString Label{Name};

    Task = [UIApplication.sharedApplication beginBackgroundTaskWithName:Name.toNSString() expirationHandler:^
    {
        qCWarning(LogApp) << "iOS ended the background time of" << Label << "before it finished";

        [UIApplication.sharedApplication endBackgroundTask:Task];
        Task = UIBackgroundTaskInvalid;
    }];

    qCInfo(LogApp) << "Background time requested for" << Name;

    return static_cast<unsigned long>(Task);
}

void AppIosBackgroundTask::End(unsigned long Task)
{
    if (Task == static_cast<unsigned long>(UIBackgroundTaskInvalid))
    {
        return;
    }

    [UIApplication.sharedApplication endBackgroundTask:static_cast<UIBackgroundTaskIdentifier>(Task)];
}
