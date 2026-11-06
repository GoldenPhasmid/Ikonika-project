#include "SyncTestSupport.h"

#include "AppCredentialStore.h"
#include "AppDatabaseService.h"
#include "AppWorkspace.h"
#include "AppWorkspaceHost.h"
#include "AppWorkspaceMerger.h"
#include "AppWorkspaceSync.h"

#include "core/FTCard.h"
#include "core/FTCardRepository.h"
#include "core/FTDatabase.h"
#include "core/FTSettings.h"

#include <QFile>
#include <QTemporaryDir>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace
{
    AppAccount MakeAccount(const QString& Id, const QString& Email)
    {
        AppAccount Account;
        Account.Id = Id;
        Account.Email = Email;

        return Account;
    }

    void AddCard(FTCardRepository& CardRepository, const std::string& Word, const std::string& Translation)
    {

        FTCard Card;
        Card.TargetWord = Word;
        Card.Translation = Translation;

        REQUIRE(CardRepository.AddCard(Card).has_value());
    }

    AppSyncTimings MakeTimings()
    {
        AppSyncTimings Timings;
        Timings.QuietPauseMilliseconds = 60000;
        Timings.ForegroundCooldownMilliseconds = 300000;
        Timings.RetryBaseMilliseconds = 30000;

        return Timings;
    }

    QString FolderName(const AppWorkspaceHost& Host)
    {
        return Host.GetActive()->GetFolder().Directory.section(QLatin1Char('/'), -1);
    }

    struct FSyncedHost
    {
        FSyncedHost()
        {
            Host.SetCloudStore(&Store, MakeTimings());
        }

        QTemporaryDir Root;
        AppMemoryCredentialStore Credentials;
        FFakeCloudStore Store;
        AppWorkspaceHost Host{Root.path(), Credentials};
    };
}

TEST_CASE("The database service opens the active workspace on first use", "[workspace]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppWorkspaceHost Host(Root.path(), Credentials);
    AppDatabaseService Service(Host);

    REQUIRE(Service.IsReady());
    REQUIRE(Host.GetActive() != nullptr);
    CHECK(Host.GetActive()->GetFolder().Directory
          == Host.GetRegistry().GetActiveFolder().Directory);
}

TEST_CASE("The database service hands back the open workspace's cards", "[workspace]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppWorkspaceHost Host(Root.path(), Credentials);
    AppDatabaseService Service(Host);

    REQUIRE(Host.OpenActive());
    AddCard(Service.GetCardRepository(), "chien", "dog");

    CHECK(Host.GetActive()->GetCardRepository().CountCards() == 1);
}

TEST_CASE("Once another workspace opens, the database service hands back its data",
          "[workspace]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppWorkspaceHost Host(Root.path(), Credentials);
    AppDatabaseService Service(Host);

    REQUIRE(Host.OpenActive());
    AddCard(Service.GetCardRepository(), "chien", "dog");

    REQUIRE(Host.GetRegistry().SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));
    REQUIRE(Host.OpenActive());

    CHECK(Service.GetCardRepository().CountCards() == 0);

    Host.GetRegistry().SignOut();
    REQUIRE(Host.OpenActive());

    CHECK(Service.GetCardRepository().CountCards() == 1);
}

TEST_CASE("The settings entry point hands back the open workspace's settings", "[workspace]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppWorkspaceHost Host(Root.path(), Credentials);

    REQUIRE(Host.OpenActive());

    FTSettings::Get().SetReminderMessage("Bonjour depuis l'espace");

    CHECK(Host.GetActive()->GetSettings().GetReminderRules().Message
          == "Bonjour depuis l'espace");
}

TEST_CASE("After a workspace closes, the settings entry point no longer hands back its settings",
          "[workspace]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppWorkspaceHost Host(Root.path(), Credentials);

    REQUIRE(Host.OpenActive());
    const FTSettings* WorkspaceSettings{&Host.GetActive()->GetSettings()};

    Host.Close();

    CHECK(Host.GetActive() == nullptr);
    CHECK(&FTSettings::Get() != WorkspaceSettings);
}

TEST_CASE("The signed-out workspace opens without sync", "[workspace][sync]")
{
    FSyncedHost Fixture;

    REQUIRE(Fixture.Host.OpenActive());

    CHECK(Fixture.Host.GetSync() == nullptr);
    CHECK(Fixture.Store.FindCount == 0);
}

TEST_CASE("A signed-in workspace opens with sync and pulls the cloud copy at once", "[workspace][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.GetRegistry().SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));

    REQUIRE(Fixture.Host.OpenActive());

    REQUIRE(Fixture.Host.GetSync() != nullptr);
    CHECK(Fixture.Store.FindCount == 1);
}

TEST_CASE("Without a cloud store a signed-in workspace opens without sync", "[workspace][sync]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppWorkspaceHost Host(Root.path(), Credentials);
    REQUIRE(Host.GetRegistry().SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));

    REQUIRE(Host.OpenActive());

    CHECK(Host.GetSync() == nullptr);
}

TEST_CASE("Signing in stores the credential and opens the account's own empty workspace with sync",
          "[workspace][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.OpenActive());
    AddCard(Fixture.Host.GetActive()->GetCardRepository(), "chien", "dog");

    REQUIRE(Fixture.Host.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));

    CHECK(Fixture.Credentials.Read("1001") == "alice-token");
    REQUIRE(Fixture.Host.GetActive() != nullptr);
    CHECK(FolderName(Fixture.Host) == "google-1001");
    CHECK(Fixture.Host.GetActive()->GetCardRepository().CountCards() == 0);
    REQUIRE(Fixture.Host.GetSync() != nullptr);
    CHECK(Fixture.Store.FindCount == 1);
}

TEST_CASE("Switching workspaces announces the close before it happens and the open after it",
          "[workspace][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.OpenActive());

    std::vector<QString> Events;

    QObject::connect(&Fixture.Host, &AppWorkspaceHost::aboutToSwitch, [&]
                     {
                         Events.push_back(QStringLiteral("close ") + FolderName(Fixture.Host));
                     });
    QObject::connect(&Fixture.Host, &AppWorkspaceHost::switched, [&]
                     {
                         Events.push_back(QStringLiteral("open ") + FolderName(Fixture.Host));
                     });

    REQUIRE(Fixture.Host.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));
    Fixture.Host.SignOut();

    CHECK(Events == std::vector<QString>{"close signed-out", "open google-1001", "close google-1001",
                                         "open signed-out"});
}

TEST_CASE("Every opened workspace is announced, the first one at launch included", "[workspace][sync]")
{
    FSyncedHost Fixture;

    int Opened{0};
    QObject::connect(&Fixture.Host, &AppWorkspaceHost::workspaceOpened, [&Opened] { ++Opened; });

    REQUIRE(Fixture.Host.OpenActive());
    REQUIRE(Fixture.Host.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));

    CHECK(Opened == 2);
}

TEST_CASE("Signing out forgets the credential, stops sync and reopens the signed-out workspace",
          "[workspace][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.OpenActive());
    AddCard(Fixture.Host.GetActive()->GetCardRepository(), "chien", "dog");
    REQUIRE(Fixture.Host.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));
    const int CancelsBefore{Fixture.Store.CancelCount};

    Fixture.Host.SignOut();

    CHECK(Fixture.Credentials.Read("1001").isEmpty());
    CHECK(Fixture.Host.GetSync() == nullptr);
    CHECK(Fixture.Store.CancelCount > CancelsBefore);
    REQUIRE(Fixture.Host.GetActive() != nullptr);
    CHECK(FolderName(Fixture.Host) == "signed-out");
    CHECK(Fixture.Host.GetActive()->GetCardRepository().CountCards() == 1);
}

TEST_CASE("Signing out while signed out changes nothing", "[workspace][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.OpenActive());
    const AppWorkspace* Before{Fixture.Host.GetActive()};

    int Switches{0};
    QObject::connect(&Fixture.Host, &AppWorkspaceHost::switched, [&Switches] { ++Switches; });

    Fixture.Host.SignOut();

    CHECK(Fixture.Host.GetActive() == Before);
    CHECK(Switches == 0);
}

TEST_CASE("Reconnecting the account keeps its workspace open and syncs again", "[workspace][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.OpenActive());
    REQUIRE(Fixture.Host.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-old"));
    Fixture.Store.FinishFind(MakeRefused(), std::nullopt);
    REQUIRE(Fixture.Host.GetRegistry().GetActiveAccount()->bNeedsReconnect);

    const AppWorkspace* Before{Fixture.Host.GetActive()};

    int Switches{0};
    QObject::connect(&Fixture.Host, &AppWorkspaceHost::switched, [&Switches] { ++Switches; });

    REQUIRE(Fixture.Host.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-new"));

    CHECK(Fixture.Host.GetActive() == Before);
    CHECK(Switches == 0);
    CHECK_FALSE(Fixture.Host.GetRegistry().GetActiveAccount()->bNeedsReconnect);
    CHECK(Fixture.Credentials.Read("1001") == "alice-new");
    CHECK(Fixture.Store.FindCount == 2);
}

TEST_CASE("Signing in as another account opens that account's workspace and keeps both credentials",
          "[workspace][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.OpenActive());
    REQUIRE(Fixture.Host.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));
    AddCard(Fixture.Host.GetActive()->GetCardRepository(), "chien", "dog");

    REQUIRE(Fixture.Host.SignIn(MakeAccount("2002", "bob@gmail.com"), "bob-token"));

    CHECK(FolderName(Fixture.Host) == "google-2002");
    CHECK(Fixture.Host.GetActive()->GetCardRepository().CountCards() == 0);
    CHECK(Fixture.Credentials.Read("1001") == "alice-token");
    CHECK(Fixture.Credentials.Read("2002") == "bob-token");
}

TEST_CASE("A sign-in the registry refuses leaves the current workspace open", "[workspace][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.OpenActive());
    const AppWorkspace* Before{Fixture.Host.GetActive()};

    int Announced{0};
    QObject::connect(&Fixture.Host, &AppWorkspaceHost::aboutToSwitch, [&Announced] { ++Announced; });

    CHECK_FALSE(Fixture.Host.SignIn(MakeAccount("../1001", "alice@gmail.com"), "alice-token"));

    CHECK(Fixture.Host.GetActive() == Before);
    CHECK(Announced == 0);
    CHECK(Fixture.Host.GetSync() == nullptr);
}

TEST_CASE("Closing the host stops sync before its workspace closes", "[workspace][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.OpenActive());
    REQUIRE(Fixture.Host.SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));

    Fixture.Host.Close();

    CHECK(Fixture.Host.GetSync() == nullptr);
    CHECK(Fixture.Host.GetActive() == nullptr);
}

namespace
{
    QString MakeOtherDatabase(const QTemporaryDir& Root, const std::string& Language)
    {
        const AppWorkspaceFolder Other{Root.path() + QStringLiteral("/other")};
        const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Other)};
        REQUIRE(Workspace != nullptr);

        Workspace->GetCardRepository().SetActiveLanguages(Language, "en");
        AddCard(Workspace->GetCardRepository(), "perro", "dog");

        return Other.GetDatabaseFilePath();
    }

    int CountAllCards(AppWorkspace& Workspace)
    {
        FTStatement Query{Workspace.GetConnection().Prepare("SELECT COUNT(*) FROM Cards WHERE Deleted = 0")};
        REQUIRE(Query.IsValid());
        REQUIRE(Query.Step());

        return Query.GetInt(0);
    }
}

TEST_CASE("Importing a database merges its cards into the open workspace and leaves the file alone",
          "[workspace][import]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppWorkspaceHost Host(Root.path(), Credentials);
    REQUIRE(Host.OpenActive());

    const QString Source{MakeOtherDatabase(Root, "es")};
    const QByteArray Before{[&Source] { QFile File(Source); REQUIRE(File.open(QIODevice::ReadOnly)); return File.readAll(); }()};

    CHECK_FALSE(Host.ImportDatabase(Source).has_value());

    CHECK(CountAllCards(*Host.GetActive()) == 1);

    QFile After(Source);
    REQUIRE(After.open(QIODevice::ReadOnly));
    CHECK(After.readAll() == Before);
}

TEST_CASE("A file that is not an Ikonika database is refused and changes nothing", "[workspace][import]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppWorkspaceHost Host(Root.path(), Credentials);
    REQUIRE(Host.OpenActive());

    const QString Source{Root.filePath(QStringLiteral("notes.db"))};
    QFile File(Source);
    REQUIRE(File.open(QIODevice::WriteOnly));
    File.write("just some text");
    File.close();

    const std::optional<AppIncomingProblem> Problem{Host.ImportDatabase(Source)};

    REQUIRE(Problem.has_value());
    CHECK_FALSE(Problem->bFromNewerApp);
    CHECK(CountAllCards(*Host.GetActive()) == 0);
}

TEST_CASE("A database from a newer app is refused", "[workspace][import]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppWorkspaceHost Host(Root.path(), Credentials);
    REQUIRE(Host.OpenActive());

    const QString Source{MakeOtherDatabase(Root, "es")};

    {
        std::optional<FTDatabase> Opened{FTDatabase::Open(Source.toStdString())};
        REQUIRE(Opened.has_value());
        REQUIRE(Opened->Execute("PRAGMA user_version = 100000"));
    }

    const std::optional<AppIncomingProblem> Problem{Host.ImportDatabase(Source)};

    REQUIRE(Problem.has_value());
    CHECK(Problem->bFromNewerApp);
    CHECK(CountAllCards(*Host.GetActive()) == 0);
}

TEST_CASE("Importing while signed in starts a sync of the merged workspace", "[workspace][import][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.GetRegistry().SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));
    REQUIRE(Fixture.Host.OpenActive());

    Fixture.Store.FinishFind(std::nullopt, std::nullopt);
    Fixture.Store.FinishUpload(std::nullopt, AppCloudCopy{QStringLiteral("copy"), QStringLiteral("1")});
    REQUIRE_FALSE(Fixture.Host.GetSync()->IsRunning());

    CHECK_FALSE(Fixture.Host.ImportDatabase(MakeOtherDatabase(Fixture.Root, "es")).has_value());

    CHECK(Fixture.Store.FindCount == 2);
}

TEST_CASE("Importing while a sync runs is refused and changes nothing", "[workspace][import][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.GetRegistry().SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));
    REQUIRE(Fixture.Host.OpenActive());
    REQUIRE(Fixture.Host.GetSync()->IsRunning());

    const std::optional<AppIncomingProblem> Problem{Fixture.Host.ImportDatabase(MakeOtherDatabase(Fixture.Root, "es"))};

    REQUIRE(Problem.has_value());
    CHECK(Problem->bBusy);
    CHECK(CountAllCards(*Fixture.Host.GetActive()) == 0);
}

TEST_CASE("A switch waits for the gate, with the old sync stopped while it waits", "[workspace][sync]")
{
    FSyncedHost Fixture;
    REQUIRE(Fixture.Host.GetRegistry().SignIn(MakeAccount("1001", "alice@gmail.com"), "alice-token"));
    REQUIRE(Fixture.Host.OpenActive());
    const QString AliceFolder{FolderName(Fixture.Host)};

    std::function<void()> Proceed;
    Fixture.Host.SetSwitchGate([&Proceed](std::function<void()> Switch) { Proceed = std::move(Switch); });

    int Switched{0};
    QObject::connect(&Fixture.Host, &AppWorkspaceHost::switched, [&Switched] { ++Switched; });

    const int CancelsBefore{Fixture.Store.CancelCount};
    Fixture.Host.SignOut();

    REQUIRE(Proceed);
    CHECK(Switched == 0);
    CHECK(FolderName(Fixture.Host) == AliceFolder);
    CHECK(Fixture.Store.CancelCount > CancelsBefore);

    Proceed();

    CHECK(Switched == 1);
    CHECK(FolderName(Fixture.Host) != AliceFolder);
}
