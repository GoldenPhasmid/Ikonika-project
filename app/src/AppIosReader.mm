#include "AppIosReader.h"

#include "AppIosPresenter.h"

#include "core/FTLog.h"

#include <utility>

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace
{
    std::function<void(const QString&)> PendingHandler;

    NSArray<UTType*>* BookTypes()
    {
        NSMutableArray<UTType*>* Types = [NSMutableArray arrayWithObjects:UTTypePDF, UTTypePlainText, nil];

        for (NSString* Extension in @[@"epub", @"fb2"])
        {
            UTType* Type = [UTType typeWithFilenameExtension:Extension];

            if (Type != nil)
            {
                [Types addObject:Type];
            }
        }

        return Types;
    }
}

@interface IkonikaDocumentPickerDelegate : NSObject <UIDocumentPickerDelegate>
@end

@implementation IkonikaDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)Controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)Urls
{
    std::function<void(const QString&)> Handler = std::exchange(PendingHandler, nullptr);

    if (Urls.count == 0 || !Handler)
    {
        return;
    }

    Handler(QString::fromNSString(Urls.firstObject.path));
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)Controller
{
    PendingHandler = nullptr;

    qCInfo(LogApp) << "Reader: the document picker was cancelled";
}

@end

void AppIosReader::PickDocument(std::function<void(const QString&)> OnPicked)
{
    static IkonikaDocumentPickerDelegate* Delegate = [[IkonikaDocumentPickerDelegate alloc] init];

    UIViewController* Presenter = AppIosPresenter::FindTopViewController();

    if (Presenter == nil)
    {
        qCWarning(LogApp) << "Reader: no window to present the document picker from";

        return;
    }

    PendingHandler = std::move(OnPicked);

    UIDocumentPickerViewController* Picker =
        [[[UIDocumentPickerViewController alloc] initForOpeningContentTypes:BookTypes() asCopy:YES] autorelease];

    Picker.delegate = Delegate;
    Picker.allowsMultipleSelection = NO;

    [Presenter presentViewController:Picker animated:YES completion:nil];

    qCInfo(LogApp) << "Reader: opened the document picker";
}

void AppIosReader::SetKeepScreenOn(bool bEnabled)
{
    UIApplication.sharedApplication.idleTimerDisabled = bEnabled;
}
