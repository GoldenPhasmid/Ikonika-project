#include "AppGoogleTokenSource.h"

#include "AppAccountRegistry.h"
#include "AppCredentialStore.h"

#include "core/FTLog.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <utility>

namespace
{
    constexpr qint64 ExpiryMarginMilliseconds{60000};
    constexpr int HttpFirstError{400};

    AppCloudFailure MakeFailure(const QString& Message, bool bCredentialRefused = false)
    {
        AppCloudFailure Failure;
        Failure.Message = Message;
        Failure.bCredentialRefused = bCredentialRefused;

        return Failure;
    }

    qint64 GetNow()
    {
        return QDateTime::currentMSecsSinceEpoch();
    }
}

AppGoogleTokenSource::AppGoogleTokenSource(AppAccountRegistry& Registry, AppCredentialStore& Credentials,
                                           const QString& ClientId, const QString& ClientSecret,
                                           const QUrl& TokenEndpoint, int TimeoutMilliseconds)
    : Registry{&Registry}
    , Credentials{&Credentials}
    , ClientId{ClientId}
    , ClientSecret{ClientSecret}
    , TokenEndpoint{TokenEndpoint}
    , TimeoutMilliseconds{TimeoutMilliseconds}
    , Network{std::make_unique<QNetworkAccessManager>()}
{
}

AppGoogleTokenSource::~AppGoogleTokenSource()
{
    Lifetime.reset();
    Waiting.clear();
}

void AppGoogleTokenSource::RequestToken(TokenCallback Done)
{
    const QString AccountId{GetActiveAccountId()};

    if (AccountId.isEmpty())
    {
        Done(MakeFailure(QStringLiteral("no account is signed in")), QString());

        return;
    }

    if (AccountId != TokenAccountId)
    {
        InvalidateToken();
    }

    if (!AccessToken.isEmpty() && GetNow() + ExpiryMarginMilliseconds < ExpiresAt)
    {
        Done(std::nullopt, AccessToken);

        return;
    }

    Waiting.push_back(std::move(Done));

    if (bRefreshing)
    {
        return;
    }

    const QString RefreshToken{Credentials->Read(AccountId)};

    if (RefreshToken.isEmpty())
    {
        qCWarning(LogSync) << "No stored refresh token for the account" << AccountId;

        Deliver(MakeFailure(QStringLiteral("no refresh token is stored"), true), QString());

        return;
    }

    Refresh(AccountId, RefreshToken);
}

void AppGoogleTokenSource::InvalidateToken()
{
    TokenAccountId.clear();
    AccessToken.clear();
    ExpiresAt = 0;
}

void AppGoogleTokenSource::Revoke(const QString& AccountId, const QUrl& RevokeEndpoint)
{
    const QString RefreshToken{Credentials->Read(AccountId)};

    if (RefreshToken.isEmpty())
    {
        qCInfo(LogSync) << "No stored refresh token to revoke for the account" << AccountId;

        return;
    }

    if (AccountId == TokenAccountId)
    {
        InvalidateToken();
    }

    QUrlQuery Form;
    Form.addQueryItem(QStringLiteral("token"), RefreshToken);

    QNetworkRequest Request(RevokeEndpoint);
    Request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    Request.setTransferTimeout(TimeoutMilliseconds);

    qCInfo(LogSync) << "Revoking the Google grant of the account" << AccountId;

    QNetworkReply* Reply{Network->post(Request, Form.toString(QUrl::FullyEncoded).toUtf8())};

    QObject::connect(Reply, &QNetworkReply::finished, Reply, [Reply, AccountId]
                     {
                         Reply->deleteLater();

                         if (Reply->error() != QNetworkReply::NoError)
                         {
                             qCWarning(LogSync) << "Google did not revoke the grant of the account" << AccountId
                                                << ":" << Reply->errorString();

                             return;
                         }

                         qCInfo(LogSync) << "Google revoked the grant of the account" << AccountId;
                     });
}

void AppGoogleTokenSource::Refresh(const QString& AccountId, const QString& RefreshToken)
{
    bRefreshing = true;

    QUrlQuery Form;
    Form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    Form.addQueryItem(QStringLiteral("refresh_token"), RefreshToken);
    Form.addQueryItem(QStringLiteral("client_id"), ClientId);
    Form.addQueryItem(QStringLiteral("client_secret"), ClientSecret);

    QNetworkRequest Request(TokenEndpoint);
    Request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    Request.setTransferTimeout(TimeoutMilliseconds);

    qCInfo(LogSync) << "Refreshing the access token of the account" << AccountId;

    QNetworkReply* Reply{Network->post(Request, Form.toString(QUrl::FullyEncoded).toUtf8())};

    QObject::connect(Reply, &QNetworkReply::finished, Reply,
                     [this, Alive = std::weak_ptr<char>(Lifetime), Reply, AccountId]
                     {
                         Reply->deleteLater();

                         if (Alive.expired())
                         {
                             return;
                         }

                         const QByteArray Answer{Reply->readAll()};
                         const int Status{Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()};

                         if (Status >= HttpFirstError)
                         {
                             const QString Error{QJsonDocument::fromJson(Answer).object()
                                                     .value(QStringLiteral("error")).toString()};

                             OnRefreshed(AccountId,
                                         MakeFailure(QStringLiteral("Google answered %1 %2").arg(Status).arg(Error),
                                                     Error == QStringLiteral("invalid_grant")),
                                         {});

                             return;
                         }

                         if (Reply->error() != QNetworkReply::NoError)
                         {
                             OnRefreshed(AccountId, MakeFailure(Reply->errorString()), {});

                             return;
                         }

                         OnRefreshed(AccountId, std::nullopt, Answer);
                     });
}

void AppGoogleTokenSource::OnRefreshed(const QString& AccountId, const std::optional<AppCloudFailure>& Failure,
                                       const QByteArray& Answer)
{
    bRefreshing = false;

    if (AccountId != GetActiveAccountId())
    {
        qCInfo(LogSync) << "The account changed while its token was refreshed, the token is dropped";

        Deliver(MakeFailure(QStringLiteral("the account changed during the refresh")), QString());

        return;
    }

    if (Failure.has_value())
    {
        qCWarning(LogSync) << "Could not refresh the access token:" << Failure->Message;

        Deliver(Failure, QString());

        return;
    }

    const QJsonObject Grant{QJsonDocument::fromJson(Answer).object()};
    const QString Token{Grant.value(QStringLiteral("access_token")).toString()};

    if (Token.isEmpty())
    {
        Deliver(MakeFailure(QStringLiteral("Google answered without an access token")), QString());

        return;
    }

    const QString RotatedRefreshToken{Grant.value(QStringLiteral("refresh_token")).toString()};

    if (!RotatedRefreshToken.isEmpty() && !Credentials->Write(AccountId, RotatedRefreshToken))
    {
        qCWarning(LogSync) << "Could not store the new refresh token of the account" << AccountId;
    }

    TokenAccountId = AccountId;
    AccessToken = Token;
    ExpiresAt = GetNow() + Grant.value(QStringLiteral("expires_in")).toInteger() * 1000;

    Deliver(std::nullopt, AccessToken);
}

void AppGoogleTokenSource::Deliver(const std::optional<AppCloudFailure>& Failure, const QString& Token)
{
    for (const TokenCallback& Done : std::exchange(Waiting, {}))
    {
        Done(Failure, Token);
    }
}

QString AppGoogleTokenSource::GetActiveAccountId() const
{
    const std::optional<AppAccount> Account{Registry->GetActiveAccount()};

    return Account.has_value() ? Account->Id : QString();
}
