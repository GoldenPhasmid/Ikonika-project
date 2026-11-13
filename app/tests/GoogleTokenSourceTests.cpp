#include "SyncTestSupport.h"

#include "AppAccountRegistry.h"
#include "AppCredentialStore.h"
#include "AppGoogleTokenSource.h"

#include <QTemporaryDir>
#include <QUrlQuery>

#include <memory>

namespace
{
    AppAccount MakeAccount(const QString& Id, const QString& Email)
    {
        AppAccount Account;
        Account.Id = Id;
        Account.Email = Email;

        return Account;
    }

    QUrl MakeEndpoint(const FDriveServer& Server)
    {
        QUrl Endpoint{Server.GetRoot()};
        Endpoint.setPath(QStringLiteral("/token"));

        return Endpoint;
    }

    QByteArray MakeGrant(const QString& Token, int ExpiresInSeconds)
    {
        return QStringLiteral(R"({"access_token":"%1","expires_in":%2,"token_type":"Bearer"})")
            .arg(Token)
            .arg(ExpiresInSeconds)
            .toUtf8();
    }

    struct FTokenHarness
    {
        explicit FTokenHarness(int TimeoutMilliseconds = 5000)
            : Tokens{Registry, Credentials, QStringLiteral("client-id"), QStringLiteral("client-secret"),
                     MakeEndpoint(Server), TimeoutMilliseconds}
        {
        }

        QTemporaryDir Root;
        AppMemoryCredentialStore Credentials;
        AppAccountRegistry Registry{Root.path(), Credentials};
        FDriveServer Server;
        AppGoogleTokenSource Tokens;
    };

    struct FTokenOutcome
    {
        bool bFinished = false;
        std::optional<AppCloudFailure> Failure;
        QString Token;
    };

    void Request(AppAccessTokenSource& Tokens, FTokenOutcome& Outcome)
    {
        Tokens.RequestToken([&Outcome](const std::optional<AppCloudFailure>& Failure, const QString& Token)
                            {
                                Outcome.bFinished = true;
                                Outcome.Failure = Failure;
                                Outcome.Token = Token;
                            });
    }

    FTokenOutcome RequestAndWait(FTokenHarness& Harness)
    {
        FTokenOutcome Outcome;
        Request(Harness.Tokens, Outcome);

        REQUIRE(WaitUntil([&Outcome] { return Outcome.bFinished; }));

        return Outcome;
    }

    QUrlQuery ReadForm(const FReceivedRequest& Received)
    {
        return QUrlQuery(QString::fromUtf8(Received.Body));
    }
}

TEST_CASE("A token request sends the account's refresh token and hands back the access token", "[sync][token]")
{
    FTokenHarness Harness;
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));
    Harness.Server.Answer(200, MakeGrant("access-1", 3600));

    const FTokenOutcome Outcome{RequestAndWait(Harness)};

    CHECK_FALSE(Outcome.Failure.has_value());
    CHECK(Outcome.Token == "access-1");

    REQUIRE(Harness.Server.Requests.size() == 1);
    const FReceivedRequest& Sent{Harness.Server.Requests.front()};
    CHECK(Sent.Method == "POST");
    CHECK(Sent.Target == "/token");

    const QUrlQuery Form{ReadForm(Sent)};
    CHECK(Form.queryItemValue("grant_type") == "refresh_token");
    CHECK(Form.queryItemValue("refresh_token") == "alice-refresh");
    CHECK(Form.queryItemValue("client_id") == "client-id");
    CHECK(Form.queryItemValue("client_secret") == "client-secret");
}

TEST_CASE("A token still inside its lifetime is handed back without asking Google again", "[sync][token]")
{
    FTokenHarness Harness;
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));
    Harness.Server.Answer(200, MakeGrant("access-1", 3600));

    RequestAndWait(Harness);
    const FTokenOutcome Second{RequestAndWait(Harness)};

    CHECK(Second.Token == "access-1");
    CHECK(Harness.Server.Requests.size() == 1);
}

TEST_CASE("A token about to expire is refreshed before it is handed back", "[sync][token]")
{
    FTokenHarness Harness;
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));
    Harness.Server.Answer(200, MakeGrant("access-1", 30));
    Harness.Server.Answer(200, MakeGrant("access-2", 3600));

    RequestAndWait(Harness);
    const FTokenOutcome Second{RequestAndWait(Harness)};

    CHECK(Second.Token == "access-2");
    CHECK(Harness.Server.Requests.size() == 2);
}

TEST_CASE("Requests made while a refresh is under way share its answer", "[sync][token]")
{
    FTokenHarness Harness;
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));
    Harness.Server.Answer(200, MakeGrant("access-1", 3600));

    FTokenOutcome First;
    FTokenOutcome Second;
    Request(Harness.Tokens, First);
    Request(Harness.Tokens, Second);

    REQUIRE(WaitUntil([&] { return First.bFinished && Second.bFinished; }));
    CHECK(First.Token == "access-1");
    CHECK(Second.Token == "access-1");
    CHECK(Harness.Server.Requests.size() == 1);
}

TEST_CASE("Google refusing the refresh token fails as a refused credential", "[sync][token]")
{
    FTokenHarness Harness;
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));
    Harness.Server.Answer(400, R"({"error":"invalid_grant","error_description":"Token has been expired or revoked."})");

    const FTokenOutcome Outcome{RequestAndWait(Harness)};

    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Credential);
    CHECK(Outcome.Token.isEmpty());
}

TEST_CASE("A token endpoint error other than a refused grant does not blame the credential", "[sync][token]")
{
    FTokenHarness Harness;
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));
    Harness.Server.Answer(503, R"({"error":"backend_error"})");

    const FTokenOutcome Outcome{RequestAndWait(Harness)};

    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Transient);
}

TEST_CASE("A token endpoint that never answers fails after the timeout without blaming the credential",
          "[sync][token]")
{
    FTokenHarness Harness{200};
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));

    FTokenOutcome Outcome;
    Request(Harness.Tokens, Outcome);

    REQUIRE(WaitUntil([&Outcome] { return Outcome.bFinished; }, 3000));
    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Transient);
}

TEST_CASE("With no account signed in, a token request fails without asking Google", "[sync][token]")
{
    FTokenHarness Harness;

    const FTokenOutcome Outcome{RequestAndWait(Harness)};

    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Transient);
    CHECK(Harness.Server.Requests.empty());
}

TEST_CASE("A dropped token is refreshed on the next request", "[sync][token]")
{
    FTokenHarness Harness;
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));
    Harness.Server.Answer(200, MakeGrant("access-1", 3600));
    Harness.Server.Answer(200, MakeGrant("access-2", 3600));

    RequestAndWait(Harness);
    Harness.Tokens.InvalidateToken();
    const FTokenOutcome Second{RequestAndWait(Harness)};

    CHECK(Second.Token == "access-2");
    CHECK(Harness.Server.Requests.size() == 2);
}

TEST_CASE("Another account signing in never gets the previous account's token", "[sync][token]")
{
    FTokenHarness Harness;
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));
    Harness.Server.Answer(200, MakeGrant("alice-access", 3600));
    Harness.Server.Answer(200, MakeGrant("bob-access", 3600));

    RequestAndWait(Harness);
    REQUIRE(Harness.Registry.SignIn(MakeAccount("2002", "bob@gmail.com"), "bob-refresh"));
    const FTokenOutcome Second{RequestAndWait(Harness)};

    CHECK(Second.Token == "bob-access");
    REQUIRE(Harness.Server.Requests.size() == 2);
    CHECK(ReadForm(Harness.Server.Requests.back()).queryItemValue("refresh_token") == "bob-refresh");
}

TEST_CASE("A token answered after the account changed is handed to nobody", "[sync][token]")
{
    FTokenHarness Harness;
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));
    Harness.Server.Answer(200, MakeGrant("alice-access", 3600));

    FTokenOutcome Outcome;
    Request(Harness.Tokens, Outcome);
    REQUIRE(Harness.Registry.SignIn(MakeAccount("2002", "bob@gmail.com"), "bob-refresh"));

    REQUIRE(WaitUntil([&Outcome] { return Outcome.bFinished; }));
    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Transient);
    CHECK(Outcome.Token.isEmpty());
}

TEST_CASE("A new refresh token in the answer replaces the stored one", "[sync][token]")
{
    FTokenHarness Harness;
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));
    Harness.Server.Answer(
        200, R"({"access_token":"access-1","expires_in":3600,"refresh_token":"alice-refresh-2"})");

    RequestAndWait(Harness);

    CHECK(Harness.Credentials.Read("1001") == "alice-refresh-2");
}

TEST_CASE("A token answer that arrives after the token source is gone touches nothing", "[sync][token]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppAccountRegistry Registry{Root.path(), Credentials};
    REQUIRE(Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));

    FDriveServer Server;
    Server.Answer(200, MakeGrant("access-1", 3600));

    bool bCalledBack{false};

    auto Tokens{std::make_unique<AppGoogleTokenSource>(Registry, Credentials, QStringLiteral("client-id"),
                                                        QStringLiteral("client-secret"), MakeEndpoint(Server),
                                                        5000)};

    Tokens->RequestToken([&bCalledBack](const std::optional<AppCloudFailure>&, const QString&)
                         {
                             bCalledBack = true;
                         });

    Tokens.reset();
    SpinFor(200);

    CHECK_FALSE(bCalledBack);
}

TEST_CASE("Revoking sends the account's refresh token to Google", "[sync][token]")
{
    FTokenHarness Harness;
    REQUIRE(Harness.Registry.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-refresh"));
    Harness.Server.Answer(200, "{}");

    QUrl Endpoint{Harness.Server.GetRoot()};
    Endpoint.setPath(QStringLiteral("/revoke"));

    Harness.Tokens.Revoke(QStringLiteral("1001"), Endpoint);

    REQUIRE(WaitUntil([&Harness] { return !Harness.Server.Requests.empty(); }));
    const FReceivedRequest& Sent{Harness.Server.Requests.front()};
    CHECK(Sent.Method == "POST");
    CHECK(Sent.Target == "/revoke");
    CHECK(ReadForm(Sent).queryItemValue("token") == "alice-refresh");
}

TEST_CASE("Revoking an account with no stored token sends nothing", "[sync][token]")
{
    FTokenHarness Harness;

    QUrl Endpoint{Harness.Server.GetRoot()};
    Endpoint.setPath(QStringLiteral("/revoke"));

    Harness.Tokens.Revoke(QStringLiteral("1001"), Endpoint);
    SpinFor(200);

    CHECK(Harness.Server.Requests.empty());
}
