#include <catch2/catch_test_macros.hpp>

#include "AppCredentialStore.h"
#include "AppAccountRegistry.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>

namespace
{
    AppAccount MakeAccount(const QString& Id, const QString& Email, const QString& Name)
    {
        AppAccount Account;
        Account.Id = Id;
        Account.Email = Email;
        Account.Name = Name;

        return Account;
    }

    void WriteFile(const QString& Path, const QByteArray& Contents)
    {
        QDir().mkpath(QFileInfo(Path).absolutePath());

        QFile File(Path);
        REQUIRE(File.open(QIODevice::WriteOnly));
        File.write(Contents);
    }

    bool IsInside(const QString& Path, const QString& Directory)
    {
        const QString Parent = QDir::cleanPath(Directory) + QLatin1Char('/');

        return QDir::cleanPath(Path).startsWith(Parent);
    }

    bool AnyFileContains(const QString& Root, const QByteArray& Text)
    {
        QDirIterator Iterator(Root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);

        while (Iterator.hasNext())
        {
            QFile File(Iterator.next());

            if (File.open(QIODevice::ReadOnly) && File.readAll().contains(Text))
            {
                return true;
            }
        }

        return false;
    }

    QStringList WorkspaceFiles(const AppWorkspaceFolder& Workspace)
    {
        return {Workspace.GetSettingsFilePath(), Workspace.GetDatabaseFilePath(),
                Workspace.GetBooksDirectory(), Workspace.GetCheckpointsDirectory()};
    }

    const AppAccount Alice{MakeAccount("1001", "alice@gmail.com", "Alice Martin")};
    const AppAccount Bob{MakeAccount("2002", "bob@gmail.com", "Bob Durand")};
}

TEST_CASE("A fresh device opens the signed-out workspace", "[accounts]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;

    const AppAccountRegistry Registry(Root.path(), Credentials);

    CHECK_FALSE(Registry.GetActiveAccount().has_value());
    CHECK(IsInside(Registry.GetActiveFolder().Directory, Root.path()));
    CHECK(QDir(Registry.GetActiveFolder().Directory).exists());
}

TEST_CASE("Signing in a new account opens an empty workspace of its own", "[accounts]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppAccountRegistry Registry(Root.path(), Credentials);

    const AppWorkspaceFolder SignedOut{Registry.GetActiveFolder()};
    WriteFile(SignedOut.GetDatabaseFilePath(), "signed-out cards");

    REQUIRE(Registry.SignIn(Alice, "alice-token"));

    REQUIRE(Registry.GetActiveAccount().has_value());
    CHECK(Registry.GetActiveAccount()->Id == "1001");
    CHECK(Registry.GetActiveFolder().Directory != SignedOut.Directory);
    CHECK(QDir(Registry.GetActiveFolder().Directory).exists());
    CHECK(QDir(Registry.GetActiveFolder().Directory).isEmpty());
}

TEST_CASE("A relaunch reopens the signed-in account's workspace from disk alone", "[accounts]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    QString AliceDirectory;

    {
        AppAccountRegistry Registry(Root.path(), Credentials);
        REQUIRE(Registry.SignIn(Alice, "alice-token"));
        AliceDirectory = Registry.GetActiveFolder().Directory;
    }

    const AppAccountRegistry Relaunched(Root.path(), Credentials);

    REQUIRE(Relaunched.GetActiveAccount().has_value());
    CHECK(Relaunched.GetActiveAccount()->Id == "1001");
    CHECK(Relaunched.GetActiveAccount()->Email == "alice@gmail.com");
    CHECK(Relaunched.GetActiveAccount()->Name == "Alice Martin");
    CHECK(Relaunched.GetActiveFolder().Directory == AliceDirectory);
}

TEST_CASE("Signing in to a known account reopens its workspace under a changed email", "[accounts]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppAccountRegistry Registry(Root.path(), Credentials);

    REQUIRE(Registry.SignIn(Alice, "alice-token"));
    const AppWorkspaceFolder AliceWorkspace{Registry.GetActiveFolder()};
    WriteFile(AliceWorkspace.GetDatabaseFilePath(), "alice cards");

    Registry.SignOut();
    REQUIRE(Registry.SignIn(MakeAccount("1001", "alice.martin@gmail.com", "Alice M."),
                              "alice-token-2"));

    CHECK(Registry.GetActiveFolder().Directory == AliceWorkspace.Directory);
    CHECK(QFile::exists(AliceWorkspace.GetDatabaseFilePath()));
    CHECK(Registry.GetActiveAccount()->Email == "alice.martin@gmail.com");
    CHECK(Registry.GetActiveAccount()->Name == "Alice M.");
}

TEST_CASE("Signing out opens the signed-out workspace and leaves the account's files", "[accounts]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppAccountRegistry Registry(Root.path(), Credentials);

    const QString SignedOutDirectory{Registry.GetActiveFolder().Directory};

    REQUIRE(Registry.SignIn(Alice, "alice-token"));
    const AppWorkspaceFolder AliceWorkspace{Registry.GetActiveFolder()};
    WriteFile(AliceWorkspace.GetDatabaseFilePath(), "alice cards");

    Registry.SignOut();

    CHECK_FALSE(Registry.GetActiveAccount().has_value());
    CHECK(Registry.GetActiveFolder().Directory == SignedOutDirectory);
    CHECK(QFile::exists(AliceWorkspace.GetDatabaseFilePath()));
}

TEST_CASE("Switching accounts gives each account its own workspace", "[accounts]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppAccountRegistry Registry(Root.path(), Credentials);

    REQUIRE(Registry.SignIn(Alice, "alice-token"));
    const AppWorkspaceFolder AliceWorkspace{Registry.GetActiveFolder()};
    WriteFile(AliceWorkspace.GetDatabaseFilePath(), "alice cards");

    REQUIRE(Registry.SignIn(Bob, "bob-token"));
    const AppWorkspaceFolder BobWorkspace{Registry.GetActiveFolder()};

    CHECK(Registry.GetActiveAccount()->Id == "2002");
    CHECK(BobWorkspace.Directory != AliceWorkspace.Directory);
    CHECK_FALSE(QFile::exists(BobWorkspace.GetDatabaseFilePath()));

    REQUIRE(Registry.SignIn(Alice, "alice-token"));

    CHECK(Registry.GetActiveFolder().Directory == AliceWorkspace.Directory);
    CHECK(QFile::exists(AliceWorkspace.GetDatabaseFilePath()));
}

TEST_CASE("Every file a workspace owns sits inside its own folder", "[accounts]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppAccountRegistry Registry(Root.path(), Credentials);

    const AppWorkspaceFolder SignedOut{Registry.GetActiveFolder()};
    REQUIRE(Registry.SignIn(Alice, "alice-token"));
    const AppWorkspaceFolder AliceWorkspace{Registry.GetActiveFolder()};

    CHECK_FALSE(IsInside(SignedOut.Directory, AliceWorkspace.Directory));
    CHECK_FALSE(IsInside(AliceWorkspace.Directory, SignedOut.Directory));

    for (const QString& Path : WorkspaceFiles(SignedOut))
    {
        CHECK(IsInside(Path, SignedOut.Directory));
    }

    for (const QString& Path : WorkspaceFiles(AliceWorkspace))
    {
        CHECK(IsInside(Path, AliceWorkspace.Directory));
    }
}

TEST_CASE("A refused credential keeps the workspace open until the user signs in again",
          "[accounts]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    QString AliceDirectory;

    {
        AppAccountRegistry Registry(Root.path(), Credentials);
        REQUIRE(Registry.SignIn(Alice, "alice-token"));
        AliceDirectory = Registry.GetActiveFolder().Directory;

        Registry.MarkNeedsReconnect("1001");

        CHECK(Registry.GetActiveFolder().Directory == AliceDirectory);
        CHECK(Registry.GetActiveAccount()->bNeedsReconnect);
    }

    AppAccountRegistry Relaunched(Root.path(), Credentials);

    CHECK(Relaunched.GetActiveFolder().Directory == AliceDirectory);
    CHECK(Relaunched.GetActiveAccount()->bNeedsReconnect);

    REQUIRE(Relaunched.SignIn(Alice, "alice-token-2"));

    CHECK_FALSE(Relaunched.GetActiveAccount()->bNeedsReconnect);
}

TEST_CASE("The credential goes to the credential store and never into a file", "[accounts]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppAccountRegistry Registry(Root.path(), Credentials);

    REQUIRE(Registry.SignIn(Alice, "alice-secret-token"));

    CHECK(Credentials.Read("1001") == "alice-secret-token");
    CHECK_FALSE(AnyFileContains(Root.path(), "alice-secret-token"));
}

TEST_CASE("Signing out forgets the credential, switching keeps the previous account's",
          "[accounts]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppAccountRegistry Registry(Root.path(), Credentials);

    REQUIRE(Registry.SignIn(Alice, "alice-token"));
    REQUIRE(Registry.SignIn(Bob, "bob-token"));

    CHECK(Credentials.Read("1001") == "alice-token");

    Registry.SignOut();

    CHECK(Credentials.Read("2002").isEmpty());
    CHECK(Credentials.Read("1001") == "alice-token");
}

TEST_CASE("An account id that is not a plain folder name is refused", "[accounts]")
{
    QTemporaryDir Root;
    AppMemoryCredentialStore Credentials;
    AppAccountRegistry Registry(Root.path(), Credentials);

    const QString SignedOutDirectory{Registry.GetActiveFolder().Directory};

    CHECK_FALSE(Registry.SignIn(MakeAccount("", "empty@gmail.com", "Empty"), "token"));
    CHECK_FALSE(Registry.SignIn(MakeAccount("../1001", "escape@gmail.com", "Escape"), "token"));

    CHECK_FALSE(Registry.GetActiveAccount().has_value());
    CHECK(Registry.GetActiveFolder().Directory == SignedOutDirectory);
    CHECK_FALSE(QDir(Root.path() + "/1001").exists());
}
