#include "AppIosReader.h"

#include "AppFreeBooks.h"
#include "AppIosPresenter.h"

#include "core/FTLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

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

QString AppIosReader::KeepBook(const QString& SourcePath, bool bMove)
{
    AppFreeBooks* Free = AppFreeBooks::GetInstance();

    if (Free == nullptr)
    {
        qCWarning(LogApp) << "Reader: no workspace to keep the book in";

        return {};
    }

    const QDir Directory(Free->GetBooksDirectory());
    const QFileInfo Source(SourcePath);

    if (!Directory.mkpath(QStringLiteral(".")))
    {
        qCWarning(LogApp) << "Reader: could not create" << Directory.absolutePath();

        return {};
    }

    QString Destination = Directory.filePath(Source.fileName());

    for (int Copy = 2; QFileInfo::exists(Destination); ++Copy)
    {
        Destination = Directory.filePath(QStringLiteral("%1 (%2).%3")
            .arg(Source.completeBaseName()).arg(Copy).arg(Source.suffix()));
    }

    const bool bKept = (bMove && QFile::rename(SourcePath, Destination)) || QFile::copy(SourcePath, Destination);

    if (!bKept)
    {
        qCWarning(LogApp) << "Reader: could not keep the book" << SourcePath << "at" << Destination;

        return {};
    }

    qCInfo(LogApp) << "Reader: kept the book at" << Destination;

    return Destination;
}

QString AppIosReader::ImportOpenedUrl(const QUrl& Url)
{
    NSURL* Native = Url.toNSURL();
    const bool bScoped = [Native startAccessingSecurityScopedResource];

    NSFileCoordinator* Coordinator = [[[NSFileCoordinator alloc] initWithFilePresenter:nil] autorelease];
    NSError* Error = nil;
    __block QString Kept;

    [Coordinator coordinateReadingItemAtURL:Native options:NSFileCoordinatorReadingWithoutChanges error:&Error
                                 byAccessor:^(NSURL* ReadableUrl)
    {
        Kept = KeepBook(QString::fromNSString(ReadableUrl.path), false);
    }];

    if (bScoped)
    {
        [Native stopAccessingSecurityScopedResource];
    }

    if (Error != nil)
    {
        qCWarning(LogApp) << "Reader: could not read the book handed over by another app:"
                          << QString::fromNSString(Error.localizedDescription);
    }

    return Kept;
}
