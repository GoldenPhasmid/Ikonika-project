#include <catch2/catch_test_macros.hpp>

#include "core/FTAnkiImporter.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QUuid>

#include <string>
#include <vector>

namespace
{
    const char* AnkiTextSample = "#separator:Tab\n"
                                 "#html:false\n"
                                 "#tags column:3\n"
                                 "le chien\tdog\n"
                                 "la maison\thouse\n"
                                 "un chat\tcat\n";

    /** Writes Content to a temp file and returns its path, or an empty string on failure. */
    std::string WriteTempText(const char* Content)
    {
        const QString Path = QStringLiteral(SAVED_TEST_DATA_DIRECTORY) + QStringLiteral("/AnkiImportTest.txt");

        QFile File(Path);

        if (!File.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return {};
        }

        File.write(Content);
        File.close();

        return Path.toStdString();
    }

    /** Creates a collection.anki2-shaped SQLite file with the given notes, returns its path. */
    std::string WriteTempAnkiDatabase(const std::vector<std::string>& FldsRows)
    {
        const QString Path = QStringLiteral(SAVED_TEST_DATA_DIRECTORY) + QStringLiteral("/AnkiImportTest.anki2");
        QFile::remove(Path);

        const QString ConnectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QSqlDatabase Database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), ConnectionName);
        Database.setDatabaseName(Path);

        if (!Database.open())
        {
            QSqlDatabase::removeDatabase(ConnectionName);
            return {};
        }

        QSqlQuery Query(Database);

        if (!Query.exec(QStringLiteral("CREATE TABLE notes (id INTEGER PRIMARY KEY, guid TEXT, "
                                       "mid INTEGER, mod INTEGER, usn INTEGER, tags TEXT, flds TEXT, "
                                       "sfld INTEGER, csum INTEGER, flags INTEGER, data TEXT)")))
        {
            Database.close();
            QSqlDatabase::removeDatabase(ConnectionName);
            return {};
        }

        for (const std::string& Flds : FldsRows)
        {
            Query.prepare(QStringLiteral("INSERT INTO notes (guid, flds) VALUES (?, ?)"));
            Query.addBindValue(QStringLiteral("abc"));
            Query.addBindValue(QString::fromStdString(Flds));

            if (!Query.exec())
            {
                Database.close();
                QSqlDatabase::removeDatabase(ConnectionName);
                return {};
            }
        }

        Database.close();
        QSqlDatabase::removeDatabase(ConnectionName);

        return Path.toStdString();
    }
}

TEST_CASE("An Anki text file yields word-translation pairs", "[import]")
{
    const std::string Path = WriteTempText(AnkiTextSample);
    REQUIRE_FALSE(Path.empty());

    FTAnkiImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 3);
    CHECK(Importer.GetRows()[0].TargetWord == "le chien");
    CHECK(Importer.GetRows()[0].Translation == "dog");
    CHECK(Importer.GetRows()[2].TargetWord == "un chat");
    CHECK(Importer.GetRows()[2].Translation == "cat");
}

TEST_CASE("An Anki text file without headers defaults to a tab separator", "[import]")
{
    const std::string Path = WriteTempText("chien\tdog\nchat\tcat\n");
    REQUIRE_FALSE(Path.empty());

    FTAnkiImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 2);
    CHECK(Importer.GetRows()[1].TargetWord == "chat");
}

TEST_CASE("A semicolon-separated Anki file honours its separator header", "[import]")
{
    const std::string Path = WriteTempText("#separator:Semicolon\nchien;dog\nchat;cat\n");
    REQUIRE_FALSE(Path.empty());

    FTAnkiImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 2);
    CHECK(Importer.GetRows()[0].Translation == "dog");
}

TEST_CASE("A quoted field keeps its separator inside the word", "[import]")
{
    const std::string Path = WriteTempText("chien\t\"dog, hound\"\nchat\tcat\n");
    REQUIRE_FALSE(Path.empty());

    FTAnkiImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 2);
    CHECK(Importer.GetRows()[0].Translation == "dog, hound");
}

TEST_CASE("Blank and comment lines are skipped", "[import]")
{
    const std::string Path = WriteTempText("\n# a comment\nchien\tdog\n\nchat\tcat\n");
    REQUIRE_FALSE(Path.empty());

    FTAnkiImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 2);
}

TEST_CASE("A row missing a translation is skipped", "[import]")
{
    const std::string Path = WriteTempText("chien\tdog\nchat\n");
    REQUIRE_FALSE(Path.empty());

    FTAnkiImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 1);
    CHECK(Importer.GetRows()[0].TargetWord == "chien");
}

TEST_CASE("An .anki2 database yields word-translation pairs from its notes", "[import]")
{
    const std::string Path = WriteTempAnkiDatabase({
        "le chien\x1f" "dog\x1f",
        "la maison\x1f" "house\x1f",
    });
    REQUIRE_FALSE(Path.empty());

    FTAnkiImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 2);
    CHECK(Importer.GetRows()[0].TargetWord == "le chien");
    CHECK(Importer.GetRows()[0].Translation == "dog");
    CHECK(Importer.GetRows()[1].TargetWord == "la maison");
    CHECK(Importer.GetRows()[1].Translation == "house");
}

TEST_CASE("A missing file fails with an error rather than an empty import", "[import]")
{
    FTAnkiImporter Importer;

    REQUIRE_FALSE(Importer.Parse("D:/no/such/file.txt"));
    CHECK_FALSE(Importer.GetLastError().empty());
    CHECK(Importer.GetRows().empty());
}

TEST_CASE("An .apkg archive is refused with a pointer at the right format", "[import]")
{
    FTAnkiImporter Importer;

    REQUIRE_FALSE(Importer.Parse("D:/no/such/deck.apkg"));
    CHECK_FALSE(Importer.GetLastError().empty());
    CHECK(Importer.GetLastError().find("zip") != std::string::npos);
}

TEST_CASE("An empty file yields an error", "[import]")
{
    const std::string Path = WriteTempText("#separator:Tab\n");
    REQUIRE_FALSE(Path.empty());

    FTAnkiImporter Importer;

    REQUIRE_FALSE(Importer.Parse(Path));
    CHECK_FALSE(Importer.GetLastError().empty());
}
