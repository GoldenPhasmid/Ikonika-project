#include "AppIosStorage.h"

#include "core/FTLog.h"

#include <QDir>

#import <Foundation/Foundation.h>

void AppIosStorage::ExcludeFromBackup(const QString& Directory)
{
    if (!QDir().mkpath(Directory))
    {
        qCWarning(LogApp) << "Could not create" << Directory << "to keep it out of the iCloud backup";

        return;
    }

    NSURL* Url = [NSURL fileURLWithPath:Directory.toNSString() isDirectory:YES];
    NSError* Error = nil;

    if (![Url setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:&Error])
    {
        qCWarning(LogApp) << "Could not keep" << Directory << "out of the iCloud backup:"
                          << QString::fromNSString(Error.localizedDescription);

        return;
    }

    qCInfo(LogApp) << "Kept out of the iCloud backup:" << Directory;
}
