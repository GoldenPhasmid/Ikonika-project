#include "AppIosPresenter.h"

UIViewController* AppIosPresenter::FindTopViewController()
{
    for (UIScene* Scene in UIApplication.sharedApplication.connectedScenes)
    {
        if (![Scene isKindOfClass:[UIWindowScene class]])
        {
            continue;
        }

        for (UIWindow* Window in static_cast<UIWindowScene*>(Scene).windows)
        {
            if (!Window.isKeyWindow)
            {
                continue;
            }

            UIViewController* Top = Window.rootViewController;

            while (Top.presentedViewController != nil)
            {
                Top = Top.presentedViewController;
            }

            return Top;
        }
    }

    return nil;
}
