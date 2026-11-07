#pragma once

#include "AppAccountRegistry.h"
#include "AppCloudStore.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class AppCredentialStore;
class AppGoogleTokenSource;
class QNetworkAccessManager;
class QOAuth2AuthorizationCodeFlow;
class QOAuthHttpServerReplyHandler;

class AppGoogleAccount : public QObject, public AppAccessTokenSource
{
    Q_OBJECT

public:
    AppGoogleAccount(AppAccountRegistry& Registry, AppCredentialStore& Credentials, QObject* Parent = nullptr);
    ~AppGoogleAccount() override;

    void SignIn(const QString& AccountEmail = QString());
    void ForgetToken();

    bool IsBusy() const;
    bool IsConfigured() const;
    QString GetLastError() const;

    void RequestToken(TokenCallback Done) override;
    void InvalidateToken() override;

#if defined(Q_OS_ANDROID)
    void HandleAndroidAuthorized(const QString& AccessToken);
    void HandleAndroidAuthorizationFailed(const QString& Message, bool bRefused);
#endif

signals:
    void busyChanged();
    void errorChanged();
    void signedIn(const AppAccount& Account, const QString& Credential);

private:
    using AccountCallback = std::function<void(const std::optional<AppAccount>&)>;

    bool LoadClient();
    void FetchAccount(const QString& AccessToken, AccountCallback Done);
    void FinishSignIn(const QString& AccessToken, const QString& Credential);
    void FailSignIn(const QString& Message);
    void ClearToken(const QString& Token);

    void SetBusy(bool bValue);
    void SetLastError(const QString& Message);

#if defined(Q_OS_ANDROID)
    void AndroidAuthorize(bool bSilent, const QString& AccountEmail);
    void DeliverToken(const std::optional<AppCloudFailure>& Failure, const QString& Token);
#elif defined(Q_OS_IOS)
    void ExchangeCode(const QString& Code);
#elif defined(IKONIKA_DESKTOP)
    void PrepareFlow();
#endif

    AppAccountRegistry* Registry = nullptr;
    AppCredentialStore* Credentials = nullptr;

    std::unique_ptr<QNetworkAccessManager> Network;
    std::unique_ptr<AppGoogleTokenSource> Tokens;
#if !defined(Q_OS_IOS)
    std::unique_ptr<QOAuthHttpServerReplyHandler> ReplyHandler;
    std::unique_ptr<QOAuth2AuthorizationCodeFlow> Flow;
#endif

    QString ClientId;
    QString ClientSecret;
    QString LastError;
    QString LastServerError;
    QString LoginHint;
    QTimer SignInTimeout;

#if defined(Q_OS_IOS)
    QString PkceVerifier;
    QString PkceState;
    QString RedirectUri;
#endif

#if defined(Q_OS_ANDROID)
    QString AndroidToken;
    QString AndroidTokenAccountId;
    qint64 AndroidTokenExpiresAt = 0;
    std::vector<TokenCallback> Waiting;
    bool bInteractivePending = false;
    bool bSilentPending = false;
#endif

    bool bBusy = false;
    bool bConfigured = false;
};
