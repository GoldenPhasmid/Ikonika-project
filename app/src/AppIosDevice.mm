#include "AppIosDevice.h"

#import <Foundation/Foundation.h>

bool AppIosDevice::IsRunningOnMac()
{
    return NSProcessInfo.processInfo.isiOSAppOnMac;
}
