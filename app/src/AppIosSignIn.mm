#include "AppIosSignIn.h"

#include "AppIosPresenter.h"

#include "core/FTLog.h"

#include <QCoreApplication>

#import <AuthenticationServices/AuthenticationServices.h>

@interface IkonikaSignInPresenter : NSObject <ASWebAuthenticationPresentationContextProviding>
@end

@implementation IkonikaSignInPresenter

- (ASPresentationAnchor)presentationAnchorForWebAuthenticationSession:(ASWebAuthenticationSession*)Session
{
    return AppIosPresenter::FindTopViewController().view.window;
}

@end

namespace
{
    ASWebAuthenticationSession* ActiveSession = nil;
}

QString AppIosSignIn::ReadClientId()
{
    id Value = [NSBundle.mainBundle objectForInfoDictionaryKey:@"IkonikaGoogleClientId"];

    if (![Value isKindOfClass:[NSString class]])
    {
        return {};
    }

    return QString::fromNSString((NSString*)Value).trimmed();
}

bool AppIosSignIn::Start(const QUrl& AuthorizationUrl, const QString& CallbackScheme, FinishedCallback Done)
{
    static IkonikaSignInPresenter* Presenter = [[IkonikaSignInPresenter alloc] init];

    [ActiveSession cancel];
    [ActiveSession release];
    ActiveSession = nil;

    ActiveSession = [[ASWebAuthenticationSession alloc] initWithURL:AuthorizationUrl.toNSURL()
                                                  callbackURLScheme:CallbackScheme.toNSString()
                                                  completionHandler:^(NSURL* CallbackUrl, NSError* Error)
    {
        const QUrl Callback{CallbackUrl != nil ? QUrl::fromNSURL(CallbackUrl) : QUrl()};
        const bool bCancelled = Error != nil && [Error.domain isEqualToString:ASWebAuthenticationSessionErrorDomain]
                                && Error.code == ASWebAuthenticationSessionErrorCodeCanceledLogin;
        const QString Message{Error != nil ? QString::fromNSString(Error.localizedDescription) : QString()};

        QMetaObject::invokeMethod(QCoreApplication::instance(), [Done, Callback, Message, bCancelled]()
        {
            Done(Callback, Message, bCancelled);
        }, Qt::QueuedConnection);
    }];

    ActiveSession.presentationContextProvider = Presenter;

    if (![ActiveSession start])
    {
        qCWarning(LogSync) << "The system sign-in sheet did not open";

        return false;
    }

    qCInfo(LogSync) << "Opened the system sign-in sheet";

    return true;
}
