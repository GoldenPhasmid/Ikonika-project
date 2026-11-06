#pragma once

#include "AppCloudStore.h"

#include <QString>
#include <QUrl>

#include <memory>
#include <vector>

class AppAccountRegistry;
class AppCredentialStore;
class QNetworkAccessManager;

class AppGoogleTokenSource : public AppAccessTokenSource
{
public:
    AppGoogleTokenSource(AppAccountRegistry& Registry, AppCredentialStore& Credentials, const QString& ClientId,
                         const QString& ClientSecret, const QUrl& TokenEndpoint, int TimeoutMilliseconds);
    ~AppGoogleTokenSource() override;

    void RequestToken(TokenCallback Done) override;
    void InvalidateToken() override;
    void Revoke(const QString& AccountId, const QUrl& RevokeEndpoint);

private:
    void Refresh(const QString& AccountId, const QString& RefreshToken);
    void OnRefreshed(const QString& AccountId, const std::optional<AppCloudFailure>& Failure,
                     const QByteArray& Answer);
    void Deliver(const std::optional<AppCloudFailure>& Failure, const QString& Token);
    QString GetActiveAccountId() const;

    AppAccountRegistry* Registry = nullptr;
    AppCredentialStore* Credentials = nullptr;
    QString ClientId;
    QString ClientSecret;
    QUrl TokenEndpoint;
    int TimeoutMilliseconds = 0;

    QString TokenAccountId;
    QString AccessToken;
    qint64 ExpiresAt = 0;
    std::vector<TokenCallback> Waiting;
    bool bRefreshing = false;

    std::unique_ptr<QNetworkAccessManager> Network;
    std::shared_ptr<char> Lifetime{std::make_shared<char>()};
};
