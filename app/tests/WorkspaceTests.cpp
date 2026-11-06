#include <catch2/catch_test_macros.hpp>

#include "AppWorkspace.h"

#include "core/FTCard.h"
#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTReaderRepository.h"
#include "core/FTSettings.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace
{
    AppWorkspaceFolder MakeFolder(const QTemporaryDir& Root, const QString& Name)
    {
        return AppWorkspaceFolder{Root.path() + QLatin1Char('/') + Name};
    }

    void AddCard(AppWorkspace& Workspace, const std::string& Word, const std::string& Translation)
    {

        FTCard Card;
        Card.TargetWord = Word;
        Card.Translation = Translation;

        REQUIRE(Workspace.GetCardRepository().AddCard(Card).has_value());
    }

    bool FileHasTable(const QString& Path, const std::string& Table)
    {
        std::optional<FTDatabase> Opened{FTDatabase::OpenReadOnly(Path.toStdString())};
        REQUIRE(Opened.has_value());

        FTStatement Query{Opened->Prepare("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?1")};
        REQUIRE(Query.IsValid());

        Query.BindText(1, Table);
        REQUIRE(Query.Step());

        return Query.GetInt(0) > 0;
    }
}

TEST_CASE("Opening a new workspace gives an empty database inside its folder", "[workspace]")
{
    QTemporaryDir Root;
    const AppWorkspaceFolder Folder{MakeFolder(Root, "alice")};

    const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Folder)};

    REQUIRE(Workspace != nullptr);
    CHECK(QFile::exists(Folder.GetDatabaseFilePath()));
    CHECK(Workspace->GetCardRepository().CountCards() == 0);
}

TEST_CASE("Cards added in one workspace stay out of another and are there on reopening",
          "[workspace]")
{
    QTemporaryDir Root;
    const AppWorkspaceFolder Alice{MakeFolder(Root, "alice")};
    const AppWorkspaceFolder Bob{MakeFolder(Root, "bob")};

    {
        const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Alice)};
        REQUIRE(Workspace != nullptr);
        AddCard(*Workspace, "chien", "dog");
    }

    {
        const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Bob)};
        REQUIRE(Workspace != nullptr);
        CHECK(Workspace->GetCardRepository().CountCards() == 0);
    }

    const std::unique_ptr<AppWorkspace> Reopened{AppWorkspace::Open(Alice)};
    REQUIRE(Reopened != nullptr);
    CHECK(Reopened->GetCardRepository().CountCards() == 1);
}

TEST_CASE("Closing a workspace releases its database file", "[workspace]")
{
    QTemporaryDir Root;
    const AppWorkspaceFolder Folder{MakeFolder(Root, "alice")};

    {
        const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Folder)};
        REQUIRE(Workspace != nullptr);
        AddCard(*Workspace, "chat", "cat");
    }

    CHECK(QFile::remove(Folder.GetDatabaseFilePath()));
}

TEST_CASE("The reader library lives in the device file, not the synced database", "[workspace][reader]")
{
    QTemporaryDir Root;
    const AppWorkspaceFolder Folder{MakeFolder(Root, "alice")};

    const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Folder)};
    REQUIRE(Workspace != nullptr);

    REQUIRE(QFile::exists(Folder.GetDeviceDatabaseFilePath()));
    CHECK(FileHasTable(Folder.GetDeviceDatabaseFilePath(), "ReaderDocuments"));
    CHECK_FALSE(FileHasTable(Folder.GetDatabaseFilePath(), "ReaderDocuments"));
}

TEST_CASE("A document added to the reader is there on reopening", "[workspace][reader]")
{
    QTemporaryDir Root;
    const AppWorkspaceFolder Folder{MakeFolder(Root, "alice")};

    {
        const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Folder)};
        REQUIRE(Workspace != nullptr);
        REQUIRE(Workspace->GetReaderLibrary().AddDocument("C:/books/x.pdf", "X", "pdf").has_value());
    }

    const std::unique_ptr<AppWorkspace> Reopened{AppWorkspace::Open(Folder)};
    REQUIRE(Reopened != nullptr);
    CHECK(Reopened->GetReaderLibrary().ListDocuments().size() == 1);
}

TEST_CASE("Closing a workspace releases its device file", "[workspace][reader]")
{
    QTemporaryDir Root;
    const AppWorkspaceFolder Folder{MakeFolder(Root, "alice")};

    {
        const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Folder)};
        REQUIRE(Workspace != nullptr);
    }

    CHECK(QFile::remove(Folder.GetDeviceDatabaseFilePath()));
}

TEST_CASE("Settings changed in one workspace stay out of another and persist", "[workspace]")
{
    QTemporaryDir Root;
    const AppWorkspaceFolder Alice{MakeFolder(Root, "alice")};
    const AppWorkspaceFolder Bob{MakeFolder(Root, "bob")};

    {
        const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Alice)};
        REQUIRE(Workspace != nullptr);
        Workspace->GetSettings().SetReminderMessage("Bonjour Alice");
    }

    CHECK(QFile::exists(Alice.GetSettingsFilePath()));

    {
        const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Bob)};
        REQUIRE(Workspace != nullptr);
        CHECK(Workspace->GetSettings().GetReminderRules().Message != "Bonjour Alice");
    }

    const std::unique_ptr<AppWorkspace> Reopened{AppWorkspace::Open(Alice)};
    REQUIRE(Reopened != nullptr);
    CHECK(Reopened->GetSettings().GetReminderRules().Message == "Bonjour Alice");
}

TEST_CASE("A session checkpoint stays out of another workspace and survives reopening",
          "[workspace]")
{
    QTemporaryDir Root;
    const AppWorkspaceFolder Alice{MakeFolder(Root, "alice")};
    const AppWorkspaceFolder Bob{MakeFolder(Root, "bob")};

    {
        const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Alice)};
        REQUIRE(Workspace != nullptr);
        Workspace->GetCheckpoints().Save("Intensive", "vocabulary", R"({"stageIndex":2})");
    }

    {
        const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Bob)};
        REQUIRE(Workspace != nullptr);
        CHECK(Workspace->GetCheckpoints().LoadAll("Intensive").empty());
    }

    const std::unique_ptr<AppWorkspace> Reopened{AppWorkspace::Open(Alice)};
    REQUIRE(Reopened != nullptr);

    const std::vector<std::pair<std::string, std::string>> Saved{
        Reopened->GetCheckpoints().LoadAll("Intensive")};

    REQUIRE(Saved.size() == 1);
    CHECK(Saved.front().first == "vocabulary");
    CHECK(Saved.front().second == R"({"stageIndex":2})");
}

TEST_CASE("A folder that cannot be created does not open", "[workspace]")
{
    QTemporaryDir Root;
    const AppWorkspaceFolder Blocked{MakeFolder(Root, "blocked")};

    QFile File(Blocked.Directory);
    REQUIRE(File.open(QIODevice::WriteOnly));
    File.close();

    CHECK(AppWorkspace::Open(Blocked) == nullptr);
}

namespace
{
    struct FSettingRow
    {
        QString Value;
        int64_t LastUpdatedDate = 0;
    };

    std::optional<FSettingRow> ReadSettingRow(AppWorkspace& Workspace, const std::string& Key)
    {
        FTStatement Query{Workspace.GetConnection().Prepare(
            "SELECT Value, LastUpdatedDate FROM SyncedSettings WHERE Key = ?1")};
        REQUIRE(Query.IsValid());

        Query.BindText(1, Key);

        if (!Query.Step())
        {
            return std::nullopt;
        }

        return FSettingRow{QString::fromStdString(Query.GetText(0)), Query.GetInt64(1)};
    }
}

TEST_CASE("Changing a synced setting records it in the workspace database", "[workspace][sync]")
{
    QTemporaryDir Root;
    const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(MakeFolder(Root, "alice"))};
    REQUIRE(Workspace != nullptr);

    Workspace->GetSettings().SetTrainingWordCount(15);

    const std::optional<FSettingRow> Row = ReadSettingRow(*Workspace, "intensiveLearning/trainingWordCount");
    REQUIRE(Row.has_value());
    CHECK(ParseSettingValue(Row->Value.toStdString()).toInt() == 15);
    CHECK(Row->LastUpdatedDate > 0);
}

TEST_CASE("A device setting is never recorded in the workspace database", "[workspace][sync]")
{
    QTemporaryDir Root;
    const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(MakeFolder(Root, "alice"))};
    REQUIRE(Workspace != nullptr);

    Workspace->GetSettings().SetLaunchAtStartup(true);

    CHECK_FALSE(ReadSettingRow(*Workspace, "startup/launchAtStartup").has_value());
}

TEST_CASE("Opening a workspace records settings changed before sync knew of them, at time zero",
          "[workspace][sync]")
{
    QTemporaryDir Root;
    const AppWorkspaceFolder Folder{MakeFolder(Root, "alice")};

    REQUIRE(QDir().mkpath(Folder.Directory));
    FTSettings{Folder.GetSettingsFilePath()}.SetTrainingWordCount(10);

    const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Folder)};
    REQUIRE(Workspace != nullptr);

    const std::optional<FSettingRow> Row = ReadSettingRow(*Workspace, "intensiveLearning/trainingWordCount");
    REQUIRE(Row.has_value());
    CHECK(ParseSettingValue(Row->Value.toStdString()).toInt() == 10);
    CHECK(Row->LastUpdatedDate == 0);
}

TEST_CASE("Opening a workspace with no chosen language adopts the language of the cards studied last",
          "[workspace][sync]")
{
    QTemporaryDir Root;
    const AppWorkspaceFolder Folder{MakeFolder(Root, "alice")};

    {
        const std::unique_ptr<AppWorkspace> Workspace{AppWorkspace::Open(Folder)};
        REQUIRE(Workspace != nullptr);
        CHECK_FALSE(Workspace->GetSettings().HasChosenLanguage());

        Workspace->GetCardRepository().SetActiveLanguages("es", "uk");
        AddCard(*Workspace, "perro", "пес");
    }

    const std::unique_ptr<AppWorkspace> Reopened{AppWorkspace::Open(Folder)};
    REQUIRE(Reopened != nullptr);

    CHECK(Reopened->GetSettings().GetLanguageRules().TargetLanguage == "es");
    CHECK(Reopened->GetSettings().GetLanguageRules().NativeLanguage == "uk");
    CHECK(Reopened->GetCardRepository().CountCards() == 1);
}
