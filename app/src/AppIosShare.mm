#include "AppIosShare.h"

#include "AppIosPresenter.h"

#include "core/FTLog.h"

bool AppIosShare::ShareFile(const QString& Path)
{
    UIViewController* Presenter = AppIosPresenter::FindTopViewController();

    if (Presenter == nil)
    {
        qCWarning(LogApp) << "No window to present the share sheet from";

        return false;
    }

    NSURL* Url = [NSURL fileURLWithPath:Path.toNSString()];
    UIActivityViewController* Sheet = [[[UIActivityViewController alloc] initWithActivityItems:@[Url]
                                                                         applicationActivities:nil] autorelease];

    Sheet.popoverPresentationController.sourceView = Presenter.view;
    Sheet.popoverPresentationController.sourceRect = CGRectMake(CGRectGetMidX(Presenter.view.bounds),
                                                                CGRectGetMidY(Presenter.view.bounds), 0, 0);

    Sheet.completionWithItemsHandler = ^(UIActivityType Activity, BOOL bCompleted, NSArray* Items, NSError* Error)
    {
        qCInfo(LogApp) << "Share sheet closed, completed =" << static_cast<bool>(bCompleted)
                       << "activity" << QString::fromNSString(Activity);
    };

    [Presenter presentViewController:Sheet animated:YES completion:nil];

    qCInfo(LogApp) << "Opened the share sheet for" << Path;

    return true;
}
