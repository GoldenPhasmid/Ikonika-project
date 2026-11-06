#include "AppGoogleAccount.h"

#include "AppCredentialStore.h"
#include "AppGoogleTokenSource.h"
#include "AppPaths.h"

#include "core/FTLog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QStringList>
#if !defined(Q_OS_IOS)
#include <QtNetworkAuth/qoauth2authorizationcodeflow.h>
#include <QtNetworkAuth/qoauthhttpserverreplyhandler.h>
#endif

#include <utility>

#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>
#endif

namespace
{
    constexpr int LoopbackPort = 59123;
    constexpr int TokenTimeoutMilliseconds = 30000;
    constexpr int SignInTimeoutMilliseconds = 5 * 60 * 1000;

    const QUrl AuthorizationEndpoint(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
    const QUrl TokenEndpoint(QStringLiteral("https://oauth2.googleapis.com/token"));
    const QUrl UserInfoEndpoint(QStringLiteral("https://openidconnect.googleapis.com/v1/userinfo"));

    const QString DriveScope = QStringLiteral("https://www.googleapis.com/auth/drive.appdata");
    const QString EmailScope = QStringLiteral("https://www.googleapis.com/auth/userinfo.email");
    const QString ProfileScope = QStringLiteral("https://www.googleapis.com/auth/userinfo.profile");

    const QString AndroidScopes = DriveScope + QStringLiteral(",") + EmailScope
        + QStringLiteral(",") + ProfileScope;

    const QString RetiredSettingsGroup = QStringLiteral("google");

    QStringList CredentialCandidates()
    {
        const QString DataRoot = AppPaths::DataRoot();
        const QString SettingsRoot = QFileInfo(AppPaths::DatabaseFilePath()).absolutePath();
        const QString ApplicationDirectory = QCoreApplication::applicationDirPath();

        QStringList Candidates;
        Candidates << DataRoot + QStringLiteral("/google-oauth.json")
                   << DataRoot + QStringLiteral("/client_secret_*.json")
                   << SettingsRoot + QStringLiteral("/google-oauth.json")
                   << SettingsRoot + QStringLiteral("/client_secret_*.json")
                   << ApplicationDirectory + QStringLiteral("/google-oauth.json")
                   << ApplicationDirectory + QStringLiteral("/client_secret_*.json");

#if defined(PROJECT_ROOT_DIRECTORY) && defined(QT_DEBUG)
        Candidates << QStringLiteral(PROJECT_ROOT_DIRECTORY "/data/google-oauth.json")
                   << QStringLiteral(PROJECT_ROOT_DIRECTORY "/data/client_secret_*.json");
#endif

        return Candidates;
    }

    AppCloudFailure MakeFailure(const QString& Message, bool bCredentialRefused = false)
    {
        AppCloudFailure Failure;
        Failure.Message = Message;
        Failure.bCredentialRefused = bCredentialRefused;

        return Failure;
    }

#if defined(Q_OS_ANDROID)
    constexpr qint64 AndroidTokenLifetimeMilliseconds = 30 * 60 * 1000;

    const char* AndroidBridgeClass = "com/goldenphasmid/ikonika/GoogleAuthorization";

    AppGoogleAccount* AndroidInstance = nullptr;

    void AndroidNativeOnAuthorized(JNIEnv*, jclass, jstring AccessToken)
    {
        if (AndroidInstance == nullptr)
        {
            return;
        }

        const QString Token = QJniObject(AccessToken).toString();
        AppGoogleAccount* Instance = AndroidInstance;

        QMetaObject::invokeMethod(Instance, [Instance, Token]()
        {
            Instance->HandleAndroidAuthorized(Token);
        }, Qt::QueuedConnection);
    }

    void AndroidNativeOnAuthorizationFailed(JNIEnv*, jclass, jstring Message, jboolean Refused)
    {
        if (AndroidInstance == nullptr)
        {
            return;
        }

        const QString Text = QJniObject(Message).toString();
        const bool bRefused = Refused == JNI_TRUE;
        AppGoogleAccount* Instance = AndroidInstance;

        QMetaObject::invokeMethod(Instance, [Instance, Text, bRefused]()
        {
            Instance->HandleAndroidAuthorizationFailed(Text, bRefused);
        }, Qt::QueuedConnection);
    }
#endif
}

AppGoogleAccount::AppGoogleAccount(AppAccountRegistry& Registry, AppCredentialStore& Credentials, QObject* Parent)
    : QObject(Parent)
    , Registry{&Registry}
    , Credentials{&Credentials}
{
    SignInTimeout.setSingleShot(true);
    SignInTimeout.setInterval(SignInTimeoutMilliseconds);

    connect(&SignInTimeout, &QTimer::timeout, this, [this]
            {
                if (bBusy)
                {
                    qCWarning(LogSync) << "The sign-in was never finished in the browser";

                    FailSignIn(tr("Google sign-in was not finished. Try again."));
                }
            });

    if (AppPaths::IsHeadless())
    {
        qCInfo(LogSync) << "Headless sandbox, Google sign-in is off";

        return;
    }

    Network = std::make_unique<QNetworkAccessManager>(this);

#if defined(Q_OS_ANDROID)
    AndroidInstance = this;

    static const JNINativeMethod Methods[] = {
        { "nativeOnAuthorized", "(Ljava/lang/String;)V",
          reinterpret_cast<void*>(&AndroidNativeOnAuthorized) },
        { "nativeOnAuthorizationFailed", "(Ljava/lang/String;Z)V",
          reinterpret_cast<void*>(&AndroidNativeOnAuthorizationFailed) },
    };

    QJniEnvironment Env;

    if (!Env.registerNativeMethods(AndroidBridgeClass, Methods, 2))
    {
        SetLastError(tr("Could not register the Android sign-in bridge."));

        return;
    }

    bConfigured = true;

    qCInfo(LogSync) << "Android sign-in bridge registered";
#elif defined(Q_OS_IOS)
    qCInfo(LogSync) << "Google sign-in is not part of the iOS build, sync stays off";
#else
    if (QSettings().childGroups().contains(RetiredSettingsGroup))
    {
        qCInfo(LogSync) << "Removing the sign-in kept in the shared settings file by an earlier version";

        QSettings().remove(RetiredSettingsGroup);
    }

    if (!LoadClient())
    {
        return;
    }

    Tokens = std::make_unique<AppGoogleTokenSource>(Registry, Credentials, ClientId, ClientSecret, TokenEndpoint,
                                                    TokenTimeoutMilliseconds);
    bConfigured = true;
#endif
}

AppGoogleAccount::~AppGoogleAccount()
{
#if defined(Q_OS_ANDROID)
    if (AndroidInstance == this)
    {
        AndroidInstance = nullptr;
    }
#endif
}

bool AppGoogleAccount::IsBusy() const
{
    return bBusy;
}

bool AppGoogleAccount::IsConfigured() const
{
    return bConfigured;
}

QString AppGoogleAccount::GetLastError() const
{
    return LastError;
}

void AppGoogleAccount::FetchAccount(const QString& AccessToken, AccountCallback Done)
{
    QNetworkRequest Request(UserInfoEndpoint);
    Request.setRawHeader("Authorization", "Bearer " + AccessToken.toUtf8());
    Request.setTransferTimeout(TokenTimeoutMilliseconds);

    QNetworkReply* Reply = Network->get(Request);

    connect(Reply, &QNetworkReply::finished, this, [Reply, Done]
            {
                Reply->deleteLater();

                if (Reply->error() != QNetworkReply::NoError)
                {
                    qCWarning(LogSync) << "Google did not say whose account this is:" << Reply->errorString();

                    Done(std::nullopt);

                    return;
                }

                const QJsonObject Details = QJsonDocument::fromJson(Reply->readAll()).object();

                AppAccount Account;
                Account.Id = Details.value(QStringLiteral("sub")).toString();
                Account.Email = Details.value(QStringLiteral("email")).toString();
                Account.Name = Details.value(QStringLiteral("name")).toString();

                if (Account.Id.isEmpty())
                {
                    qCWarning(LogSync) << "Google answered without the account id";

                    Done(std::nullopt);

                    return;
                }

                Done(Account);
            });
}

void AppGoogleAccount::FinishSignIn(const QString& AccessToken, const QString& Credential)
{
    FetchAccount(AccessToken, [this, Credential](const std::optional<AppAccount>& Account)
                 {
                     if (!Account.has_value())
                     {
                         FailSignIn(tr("Google did not say which account signed in. Try again."));

                         return;
                     }

                     qCInfo(LogSync) << "Google signed in" << Account->Email << ", account id" << Account->Id;

                     SignInTimeout.stop();
                     SetBusy(false);
                     SetLastError(QString());

                     emit signedIn(*Account, Credential);
                 });
}

void AppGoogleAccount::FailSignIn(const QString& Message)
{
    SignInTimeout.stop();
    SetBusy(false);
    SetLastError(Message);
}

void AppGoogleAccount::SetBusy(bool bValue)
{
    if (bBusy == bValue)
    {
        return;
    }

    bBusy = bValue;
    emit busyChanged();
}

void AppGoogleAccount::SetLastError(const QString& Message)
{
    if (LastError == Message)
    {
        return;
    }

    LastError = Message;

    if (!Message.isEmpty())
    {
        qCWarning(LogSync) << Message;
    }

    emit errorChanged();
}

#if defined(Q_OS_ANDROID)

void AppGoogleAccount::SignIn(const QString& AccountEmail)
{
    if (!bConfigured || bInteractivePending)
    {
        qCInfo(LogSync) << "Sign-in ignored, configured =" << bConfigured << "pending =" << bInteractivePending;

        return;
    }

    qCInfo(LogSync) << "The user asked to sign in, preselected account" << AccountEmail;

    SetBusy(true);
    SetLastError(QString());

    bInteractivePending = true;
    LoginHint = AccountEmail;

    if (!bSilentPending)
    {
        AndroidAuthorize(false, LoginHint);
    }
}

void AppGoogleAccount::ForgetToken()
{
    InvalidateToken();
    AndroidTokenAccountId.clear();
}

void AppGoogleAccount::RequestToken(TokenCallback Done)
{
    const std::optional<AppAccount> Account{Registry->GetActiveAccount()};

    if (!bConfigured || !Account.has_value())
    {
        Done(MakeFailure(QStringLiteral("no account is signed in")), QString());

        return;
    }

    if (!AndroidToken.isEmpty() && AndroidTokenAccountId == Account->Id
        && QDateTime::currentMSecsSinceEpoch() < AndroidTokenExpiresAt)
    {
        Done(std::nullopt, AndroidToken);

        return;
    }

    Waiting.push_back(std::move(Done));

    if (bSilentPending || bInteractivePending)
    {
        return;
    }

    bSilentPending = true;
    AndroidTokenAccountId = Account->Id;

    AndroidAuthorize(true, Account->Email);
}

void AppGoogleAccount::InvalidateToken()
{
    if (!AndroidToken.isEmpty())
    {
        ClearToken(AndroidToken);
    }

    AndroidToken.clear();
    AndroidTokenExpiresAt = 0;
}

void AppGoogleAccount::ClearToken(const QString& Token)
{
    if (!bConfigured)
    {
        return;
    }

    qCInfo(LogSync) << "Clearing the refused access token from Play services";

    QJniObject Activity = QNativeInterface::QAndroidApplication::context();
    QJniObject JniToken = QJniObject::fromString(Token);

    QJniObject::callStaticMethod<void>(AndroidBridgeClass, "clearToken",
        "(Landroid/app/Activity;Ljava/lang/String;)V",
        Activity.object<jobject>(), JniToken.object<jstring>());
}

void AppGoogleAccount::AndroidAuthorize(bool bSilent, const QString& AccountEmail)
{
    qCInfo(LogSync) << "Requesting Android authorization, silent =" << bSilent;

    QJniObject Activity = QNativeInterface::QAndroidApplication::context();
    QJniObject Scopes = QJniObject::fromString(AndroidScopes);
    QJniObject Email = QJniObject::fromString(AccountEmail);

    QJniObject::callStaticMethod<void>(AndroidBridgeClass, "authorize",
        "(Landroid/app/Activity;Ljava/lang/String;ZLjava/lang/String;)V",
        Activity.object<jobject>(), Scopes.object<jstring>(), jboolean(bSilent), Email.object<jstring>());
}

void AppGoogleAccount::HandleAndroidAuthorized(const QString& AccessToken)
{
    if (bSilentPending)
    {
        bSilentPending = false;

        const std::optional<AppAccount> Account{Registry->GetActiveAccount()};

        if (!Account.has_value() || Account->Id != AndroidTokenAccountId)
        {
            DeliverToken(MakeFailure(QStringLiteral("the account changed during the request")), QString());
        }
        else
        {
            AndroidToken = AccessToken;
            AndroidTokenExpiresAt = QDateTime::currentMSecsSinceEpoch() + AndroidTokenLifetimeMilliseconds;

            DeliverToken(std::nullopt, AndroidToken);
        }

        if (bInteractivePending)
        {
            AndroidAuthorize(false, LoginHint);
        }

        return;
    }

    if (!bInteractivePending)
    {
        return;
    }

    bInteractivePending = false;

    FinishSignIn(AccessToken, QString());
}

void AppGoogleAccount::HandleAndroidAuthorizationFailed(const QString& Message, bool bRefused)
{
    qCWarning(LogSync) << "Android authorization failed:" << Message << ", refused =" << bRefused;

    if (bSilentPending)
    {
        bSilentPending = false;

        DeliverToken(MakeFailure(Message, bRefused), QString());

        if (bInteractivePending)
        {
            AndroidAuthorize(false, LoginHint);
        }

        return;
    }

    if (!bInteractivePending)
    {
        return;
    }

    bInteractivePending = false;

    FailSignIn(tr("Google sign-in failed (%1).").arg(Message));
}

void AppGoogleAccount::DeliverToken(const std::optional<AppCloudFailure>& Failure, const QString& Token)
{
    for (const TokenCallback& Done : std::exchange(Waiting, {}))
    {
        Done(Failure, Token);
    }
}

bool AppGoogleAccount::LoadClient()
{
    return true;
}

#elif defined(IKONIKA_DESKTOP)

void AppGoogleAccount::SignIn(const QString& AccountEmail)
{
    if (!bConfigured)
    {
        SetLastError(tr("Sync is not configured. Add the Google client file."));

        return;
    }

    LoginHint = AccountEmail;

    PrepareFlow();

    if (ReplyHandler == nullptr || !ReplyHandler->isListening())
    {
        SetLastError(tr("Could not listen for Google's answer on port %1.").arg(LoopbackPort));

        return;
    }

    qCInfo(LogSync) << "The user asked to sign in, opening the browser, preselected account" << LoginHint;

    SetBusy(true);
    SetLastError(QString());
    SignInTimeout.start();

    Flow->grant();
}

void AppGoogleAccount::ForgetToken()
{
    InvalidateToken();
}

void AppGoogleAccount::ClearToken(const QString& Token)
{
    Q_UNUSED(Token);
}

void AppGoogleAccount::RequestToken(TokenCallback Done)
{
    if (Tokens == nullptr)
    {
        Done(MakeFailure(QStringLiteral("sync is not configured")), QString());

        return;
    }

    Tokens->RequestToken(std::move(Done));
}

void AppGoogleAccount::InvalidateToken()
{
    if (Tokens != nullptr)
    {
        Tokens->InvalidateToken();
    }
}

void AppGoogleAccount::PrepareFlow()
{
    if (Flow != nullptr)
    {
        return;
    }

    ReplyHandler = std::make_unique<QOAuthHttpServerReplyHandler>(LoopbackPort, this);

    Flow = std::make_unique<QOAuth2AuthorizationCodeFlow>(ClientId, AuthorizationEndpoint, TokenEndpoint,
                                                          Network.get(), this);

    Flow->setReplyHandler(ReplyHandler.get());
    Flow->setRequestedScopeTokens({"openid", "email", "profile", DriveScope.toUtf8()});
    Flow->setClientIdentifierSharedKey(ClientSecret);
    Flow->setModifyParametersFunction(
        [this](QAbstractOAuth::Stage Stage, QMultiMap<QString, QVariant>* Parameters)
        {
            if (Stage != QAbstractOAuth::Stage::RequestingAuthorization)
            {
                return;
            }

            if (LoginHint.isEmpty())
            {
                Parameters->insert(QStringLiteral("prompt"), QStringLiteral("select_account"));
                return;
            }

            Parameters->insert(QStringLiteral("login_hint"), LoginHint);
        });

    connect(Flow.get(), &QAbstractOAuth::granted, this, [this]
            {
                const QString RefreshToken{Flow->refreshToken()};

                if (RefreshToken.isEmpty())
                {
                    FailSignIn(tr("Google did not allow this device to stay signed in. Try again."));

                    return;
                }

                FinishSignIn(Flow->token(), RefreshToken);
            });

    connect(Flow.get(), &QAbstractOAuth2::serverReportedErrorOccurred, this,
            [this](const QString& Error, const QString& Description, const QUrl&)
            {
                LastServerError = Description.isEmpty() ? Error : Description;

                qCWarning(LogSync) << "Google refused the sign-in:" << Error << "-" << Description;
            });

    connect(Flow.get(), &QAbstractOAuth::requestFailed, this, [this](QAbstractOAuth::Error Error)
            {
                const QString Reason = LastServerError.isEmpty() ? QStringLiteral("error %1").arg(int(Error))
                                                                 : LastServerError;
                LastServerError.clear();

                FailSignIn(tr("Google sign-in failed (%1).").arg(Reason));
            });

    connect(Flow.get(), &QAbstractOAuth::authorizeWithBrowser, this, [](const QUrl& Url)
            {
                if (!QDesktopServices::openUrl(Url))
                {
                    qCWarning(LogSync) << "Could not open the system browser for sign-in";
                }
            });
}

bool AppGoogleAccount::LoadClient()
{
    for (const QString& Pattern : CredentialCandidates())
    {
        const QFileInfo PatternInfo(Pattern);
        const QDir Directory(PatternInfo.absolutePath());

        const QStringList Matches = Directory.entryList({PatternInfo.fileName()}, QDir::Files);

        if (Matches.isEmpty())
        {
            continue;
        }

        const QString Path = Directory.absoluteFilePath(Matches.first());
        QFile File(Path);

        if (!File.open(QIODevice::ReadOnly))
        {
            continue;
        }

        const QJsonObject Object = QJsonDocument::fromJson(File.readAll()).object();
        const QJsonObject Installed = Object.value(QStringLiteral("installed")).toObject();

        ClientId = Installed.value(QStringLiteral("client_id")).toString();
        ClientSecret = Installed.value(QStringLiteral("client_secret")).toString();

        if (ClientId.isEmpty())
        {
            ClientId = Object.value(QStringLiteral("client_id")).toString();
            ClientSecret = Object.value(QStringLiteral("client_secret")).toString();
        }

        if (ClientId.isEmpty())
        {
            continue;
        }

        qCInfo(LogSync) << "Google client loaded from" << Path;

        return true;
    }

    SetLastError(tr("No Google client file found. Expected google-oauth.json beside the application."));

    return false;
}

#endif

#if defined(Q_OS_IOS)

void AppGoogleAccount::SignIn(const QString& AccountEmail)
{
    qCWarning(LogSync) << "Sign-in was asked for, but the iOS build has none; account" << AccountEmail;

    SetLastError(tr("Sign-in is not available on this device yet."));
}

void AppGoogleAccount::ForgetToken()
{
}

void AppGoogleAccount::ClearToken(const QString& Token)
{
    Q_UNUSED(Token);
}

void AppGoogleAccount::RequestToken(TokenCallback Done)
{
    Done(MakeFailure(QStringLiteral("sync is not configured")), QString());
}

void AppGoogleAccount::InvalidateToken()
{
}

bool AppGoogleAccount::LoadClient()
{
    return false;
}

#endif
