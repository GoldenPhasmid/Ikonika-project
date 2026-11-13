#include <catch2/catch_test_macros.hpp>

#include "core/FTDatabase.h"
#include "core/FTJournal.h"
#include "core/FTSchema.h"
#include "core/FTUuid.h"

#include <QDate>
#include <QDateTime>
#include <QTime>

#include <set>
#include <string>

namespace
{
    FTDatabase MakeDatabase()
    {
        std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
        REQUIRE(Opened.has_value());

        return std::move(*Opened);
    }

    bool TableExists(FTDatabase& Target, const std::string& Name)
    {
        FTStatement Query = Target.Prepare("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?1");
        REQUIRE(Query.IsValid());

        Query.BindText(1, Name);
        REQUIRE(Query.Step());

        return Query.GetInt(0) > 0;
    }

    bool ColumnExists(FTDatabase& Target, const std::string& Table, const std::string& Column)
    {
        FTStatement Query = Target.Prepare("PRAGMA table_info(" + Table + ")");
        REQUIRE(Query.IsValid());

        while (Query.Step())
        {
            if (Query.GetText(1) == Column)
            {
                return true;
            }
        }

        return false;
    }

    bool IndexIsNoCase(FTDatabase& Target, const std::string& IndexName)
    {
        FTStatement Query = Target.Prepare(
            "SELECT sql FROM sqlite_master WHERE type = 'index' AND name = ?1");
        REQUIRE(Query.IsValid());

        Query.BindText(1, IndexName);
        REQUIRE(Query.Step());

        return Query.GetText(0).find("COLLATE NOCASE") != std::string::npos;
    }

    void RecreateDeckTables(FTDatabase& Target)
    {
        REQUIRE(Target.Execute(
            "DROP INDEX IF EXISTS IdxCardsLanguages;"
            "ALTER TABLE Cards DROP COLUMN TargetLanguage;"
            "ALTER TABLE Cards DROP COLUMN NativeLanguage;"
            "CREATE TABLE Decks"
            "("
            "    Id                     TEXT PRIMARY KEY NOT NULL,"
            "    Name                   TEXT NOT NULL,"
            "    Description            TEXT NOT NULL DEFAULT '',"
            "    CreatedDate            INTEGER NOT NULL,"
            "    LastUpdatedDate        INTEGER NOT NULL,"
            "    Deleted                INTEGER NOT NULL DEFAULT 0,"
            "    ColourIndex            INTEGER NOT NULL DEFAULT 0,"
            "    IntensiveCompletedDate INTEGER,"
            "    TargetLanguage         TEXT NOT NULL DEFAULT '',"
            "    NativeLanguage         TEXT NOT NULL DEFAULT '',"
            "    Role                   TEXT NOT NULL DEFAULT '',"
            "    SourceKey              TEXT NOT NULL DEFAULT ''"
            ");"
            "CREATE TABLE DeckCards"
            "("
            "    DeckId          TEXT NOT NULL REFERENCES Decks(Id) ON DELETE CASCADE,"
            "    CardId          TEXT NOT NULL REFERENCES Cards(Id) ON DELETE CASCADE,"
            "    Position        INTEGER NOT NULL DEFAULT 0,"
            "    LastUpdatedDate INTEGER NOT NULL,"
            "    Deleted         INTEGER NOT NULL DEFAULT 0,"
            "    PRIMARY KEY (DeckId, CardId)"
            ");"));
    }

    std::string ReadCardState(FTDatabase& Target, const std::string& CardId)
    {
        FTStatement Query = Target.Prepare(
            "SELECT TargetLanguage, NativeLanguage, Deleted, LastUpdatedDate, LeitnerStage "
            "FROM Cards WHERE Id = ?1");
        REQUIRE(Query.IsValid());

        Query.BindText(1, CardId);
        REQUIRE(Query.Step());

        return Query.GetText(0) + "-" + Query.GetText(1) + " deleted " + std::to_string(Query.GetInt(2))
               + " time " + std::to_string(Query.GetInt(3)) + " stage " + std::to_string(Query.GetInt(4));
    }
}

TEST_CASE("The six-box migration reopens completed third-box cards once", "[schema][journal][laddermigration]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));
    REQUIRE(Target.Execute(
        "INSERT INTO Cards (Id, TargetWord, Translation, CreatedDate, LastUpdatedDate, LeitnerStage, CompletedDate) "
        "VALUES ('old', 'chat', 'cat', 1, 1, 3, 1000), ('finished', 'chien', 'dog', 1, 1, 6, 1000);"
        "PRAGMA user_version = 35"));
    REQUIRE(SeedStartingEvents(Target));
    const FTDayNumber Today{GetTodayDayNumber()};
    REQUIRE(MigrateSchema(Target));
    {
        FTStatement Migrated{Target.Prepare("SELECT LeitnerStage, CompletedDate, DueDate FROM Cards WHERE Id = 'old'")};
        REQUIRE(Migrated.Step());
        CHECK(Migrated.GetInt(0) == 4);
        CHECK(Migrated.IsNull(1));
        CHECK(Migrated.GetInt(2) == Today + 14);
    }
    REQUIRE(RebuildCardProgress(Target, Today));
    FTStatement Query{Target.Prepare("SELECT LeitnerStage, CompletedDate, DueDate, LastUpdatedDate FROM Cards WHERE Id = 'old'")};
    REQUIRE(Query.Step());
    CHECK(Query.GetInt(0) == 4);
    CHECK(Query.IsNull(1));
    CHECK(Query.GetInt(2) == Today + 14);
    CHECK(Query.GetInt(3) == 1);
    Query = Target.Prepare("SELECT CompletedDate FROM Cards WHERE Id = 'finished'");
    REQUIRE(Query.Step());
    CHECK_FALSE(Query.IsNull(0));
    const auto Events{LoadJournal(Target, "old")};
    REQUIRE(Events.size() == 2);
    CHECK(Events.back().EventId == JournalPlaced);
    CHECK(Events.back().EventValue == 4);
    CHECK(Events.back().DueDate == Today + 14);
    REQUIRE(MigrateSchema(Target));
    CHECK(LoadJournal(Target, "old").size() == 2);
}

TEST_CASE("A fresh database reports version zero", "[schema]")
{
    FTDatabase Target = MakeDatabase();

    CHECK(ReadSchemaVersion(Target) == 0);
}

TEST_CASE("Migrating brings a fresh database to the current version", "[schema]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
}

TEST_CASE("Migrating creates the card table and no deck tables", "[schema]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateSchema(Target));

    CHECK(TableExists(Target, "Cards"));
    CHECK(ColumnExists(Target, "Cards", "TargetLanguage"));
    CHECK(ColumnExists(Target, "Cards", "NativeLanguage"));
    CHECK_FALSE(TableExists(Target, "Decks"));
    CHECK_FALSE(TableExists(Target, "DeckCards"));
}

TEST_CASE("A fresh database has no reader library and no YouTube table", "[schema]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateSchema(Target));

    CHECK_FALSE(TableExists(Target, "ReaderDocuments"));
    CHECK_FALSE(TableExists(Target, "YoutubeVideos"));
}

TEST_CASE("Migrating creates the journal", "[schema][journal]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateSchema(Target));

    REQUIRE(TableExists(Target, "Journal"));
    CHECK(ColumnExists(Target, "Journal", "Id"));
    CHECK(ColumnExists(Target, "Journal", "EventId"));
    CHECK(ColumnExists(Target, "Journal", "CardId"));
    CHECK(ColumnExists(Target, "Journal", "EventValue"));
    CHECK(ColumnExists(Target, "Journal", "Day"));
    CHECK(ColumnExists(Target, "Journal", "DueDate"));
}

TEST_CASE("Migrating from the version before recorded due dates adds the column", "[schema][journal]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));

    REQUIRE(Target.Execute("ALTER TABLE Journal DROP COLUMN DueDate"));
    REQUIRE(Target.Execute("PRAGMA user_version = 30"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
    CHECK(ColumnExists(Target, "Journal", "DueDate"));
}

TEST_CASE("Migrating a journal made before due dates still seeds the starting events", "[schema][journal]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));

    REQUIRE(Target.Execute("ALTER TABLE Journal DROP COLUMN DueDate"));
    REQUIRE(Target.Execute(
        "INSERT INTO Cards (Id, TargetWord, Translation, LeitnerStage, DueDate, BoxChangedDate, CreatedDate, "
        "                   LastUpdatedDate) "
        "VALUES ('placed', 'chien', 'dog', 2, 20010, 20005, 1, 1)"));
    REQUIRE(Target.Execute("PRAGMA user_version = 29"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);

    FTStatement Seeded = Target.Prepare("SELECT EventId, EventValue, DueDate FROM Journal WHERE CardId = 'placed'");
    REQUIRE(Seeded.Step());
    CHECK(Seeded.GetText(0) == "placed");
    CHECK(Seeded.GetInt(1) == 2);
    CHECK(Seeded.GetInt(2) == 20010);
}

TEST_CASE("Migrating creates the erasures table", "[schema][erase]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateSchema(Target));

    REQUIRE(TableExists(Target, "Erasures"));
    CHECK(ColumnExists(Target, "Erasures", "Scope"));
    CHECK(ColumnExists(Target, "Erasures", "ErasedDate"));
}

TEST_CASE("Migrating from the version before erasures adds their table", "[schema][erase]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));

    Target.Execute("DROP TABLE IF EXISTS Erasures");
    REQUIRE(Target.Execute("PRAGMA user_version = 28"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
    CHECK(TableExists(Target, "Erasures"));
}

TEST_CASE("The journal refuses a second event with the same id", "[schema][journal]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));

    const std::string Insert{
        "INSERT INTO Journal (Id, EventId, CardId, EventValue, Day) "
        "VALUES ('event-1', 'learned', 'card-1', 0, 20000)"};

    REQUIRE(Target.Execute(Insert));
    CHECK_FALSE(Target.Execute(Insert));
}

TEST_CASE("Migrating from the version before the journal adds it and drops the device tables", "[schema][journal]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));

    REQUIRE(Target.Execute("DROP TABLE Journal"));
    REQUIRE(Target.Execute(
        "CREATE TABLE ReaderDocuments (Id TEXT PRIMARY KEY NOT NULL, FilePath TEXT NOT NULL)"));
    REQUIRE(Target.Execute(
        "CREATE TABLE YoutubeVideos (Id TEXT PRIMARY KEY NOT NULL, VideoId TEXT NOT NULL)"));
    REQUIRE(Target.Execute("PRAGMA user_version = 27"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
    CHECK(TableExists(Target, "Journal"));
    CHECK_FALSE(TableExists(Target, "ReaderDocuments"));
    CHECK_FALSE(TableExists(Target, "YoutubeVideos"));
}

TEST_CASE("Migrating from the version with decks moves every card off them", "[schema][decks]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));
    RecreateDeckTables(Target);

    REQUIRE(Target.Execute(
        "INSERT INTO Decks (Id, Name, CreatedDate, LastUpdatedDate, Deleted, TargetLanguage, NativeLanguage, Role) "
        "VALUES ('learning-pool-fr-en', '', 1, 1, 0, 'fr', 'en', 'pool'),"
        "       ('kitchen', 'Kitchen', 1, 1, 0, 'fr', 'en', ''),"
        "       ('cuisine', '', 1, 1, 0, 'es', 'en', 'category'),"
        "       ('drill-numbers-fr-en', '', 1, 1, 0, 'fr', 'en', 'drill'),"
        "       ('gone', 'Gone', 1, 1, 1, 'fr', 'en', '');"
        "INSERT INTO Cards (Id, TargetWord, Translation, LeitnerStage, CreatedDate, LastUpdatedDate, Deleted) "
        "VALUES ('in-pool', 'chien', 'dog', 0, 1, 1, 0),"
        "       ('in-kitchen', 'pain', 'bread', 3, 1, 1, 0),"
        "       ('in-both', 'beurre', 'butter', 2, 1, 1, 0),"
        "       ('in-category', 'perro', 'dog', 1, 1, 1, 0),"
        "       ('in-drill', 'quatre', 'four', 1, 1, 1, 0),"
        "       ('in-gone-deck', 'lune', 'moon', 1, 1, 1, 0),"
        "       ('left-deck', 'soleil', 'sun', 0, 1, 1, 0),"
        "       ('tombstone', 'mer', 'sea', 0, 1, 1, 1);"
        "INSERT INTO DeckCards (DeckId, CardId, LastUpdatedDate, Deleted) "
        "VALUES ('learning-pool-fr-en', 'in-pool', 1, 0),"
        "       ('kitchen', 'in-kitchen', 1, 0),"
        "       ('kitchen', 'in-both', 1, 0),"
        "       ('learning-pool-fr-en', 'in-both', 1, 0),"
        "       ('cuisine', 'in-category', 1, 0),"
        "       ('drill-numbers-fr-en', 'in-drill', 1, 0),"
        "       ('gone', 'in-gone-deck', 1, 0),"
        "       ('kitchen', 'left-deck', 1, 1),"
        "       ('learning-pool-fr-en', 'tombstone', 1, 0);"));
    REQUIRE(Target.Execute("PRAGMA user_version = 31"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
    CHECK_FALSE(TableExists(Target, "Decks"));
    CHECK_FALSE(TableExists(Target, "DeckCards"));

    CHECK(ReadCardState(Target, "in-pool") == "fr-en deleted 0 time 1 stage 0");
    CHECK(ReadCardState(Target, "in-kitchen") == "fr-en deleted 0 time 1 stage 3");
    CHECK(ReadCardState(Target, "in-both") == "fr-en deleted 0 time 1 stage 2");
    CHECK(ReadCardState(Target, "in-category") == "es-en deleted 0 time 1 stage 1");
    CHECK(ReadCardState(Target, "in-drill") == "fr-en deleted 1 time 0 stage 1");
    CHECK(ReadCardState(Target, "in-gone-deck") == "fr-en deleted 1 time 0 stage 1");
    CHECK(ReadCardState(Target, "left-deck") == "fr-en deleted 1 time 0 stage 0");
    CHECK(ReadCardState(Target, "tombstone") == "fr-en deleted 1 time 1 stage 0");
}

TEST_CASE("Migrating upgrades a database several versions behind", "[schema]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(Target.Execute("PRAGMA user_version = 1"));

    REQUIRE(MigrateSchema(Target));
    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
    CHECK(TableExists(Target, "Journal"));
}

TEST_CASE("The device schema creates the reader library", "[schema][reader]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateDeviceSchema(Target));

    CHECK(TableExists(Target, "ReaderDocuments"));
    CHECK(ColumnExists(Target, "ReaderDocuments", "LastScrollX"));
    CHECK(ColumnExists(Target, "ReaderDocuments", "LastScrollY"));
}

TEST_CASE("The device schema holds no cards and no journal", "[schema][reader]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateDeviceSchema(Target));

    CHECK_FALSE(TableExists(Target, "Cards"));
    CHECK_FALSE(TableExists(Target, "Journal"));
}

TEST_CASE("Migrating the device schema again keeps the reader library", "[schema][reader]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateDeviceSchema(Target));

    FTStatement Add = Target.Prepare(
        "INSERT INTO ReaderDocuments (Id, FilePath, Title, Format, AddedDate, LastOpenedDate) "
        "VALUES ('doc-1', 'C:/books/x.pdf', 'X', 'pdf', 1, 1)");
    REQUIRE(Add.Run());

    REQUIRE(MigrateDeviceSchema(Target));

    FTStatement Count = Target.Prepare("SELECT COUNT(*) FROM ReaderDocuments");
    REQUIRE(Count.Step());
    CHECK(Count.GetInt(0) == 1);
}

TEST_CASE("An older reader library gains the read aloud place", "[schema][reader]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute(
        "CREATE TABLE ReaderDocuments (Id TEXT PRIMARY KEY NOT NULL, FilePath TEXT NOT NULL, "
        "Title TEXT NOT NULL DEFAULT '', Format TEXT NOT NULL DEFAULT '', AddedDate INTEGER NOT NULL, "
        "LastOpenedDate INTEGER NOT NULL, LastPosition INTEGER NOT NULL DEFAULT 0, LastZoom REAL NOT NULL DEFAULT 0, "
        "LastScrollX REAL NOT NULL DEFAULT 0, LastScrollY REAL NOT NULL DEFAULT 0, CloudId TEXT NOT NULL DEFAULT '', "
        "PositionCount INTEGER NOT NULL DEFAULT 0, Deleted INTEGER NOT NULL DEFAULT 0)"));
    REQUIRE(Target.Execute(
        "INSERT INTO ReaderDocuments (Id, FilePath, AddedDate, LastOpenedDate) VALUES ('doc-1', 'C:/books/x.epub', 1, 1)"));

    REQUIRE(MigrateDeviceSchema(Target));

    FTStatement Place = Target.Prepare("SELECT ReadAloudParagraph, ReadAloudCharacter FROM ReaderDocuments");
    REQUIRE(Place.Step());
    CHECK(Place.GetInt(0) == -1);
    CHECK(Place.GetInt(1) == 0);
}

TEST_CASE("A fresh database has no Skipped column", "[schema]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));

    CHECK(TableExists(Target, "TriagedWords"));
    CHECK_FALSE(ColumnExists(Target, "TriagedWords", "Skipped"));
}

TEST_CASE("Migrating drops the Skipped column and keeps the rows", "[schema]")
{
    FTDatabase Target = MakeDatabase();

    // The v12 shape of the table, since the current schema no longer produces one to drop from.
    REQUIRE(Target.Execute(
        "CREATE TABLE TriagedWords"
        "("
        "    TargetWord     TEXT NOT NULL COLLATE NOCASE,"
        "    TargetLanguage TEXT NOT NULL DEFAULT '',"
        "    PartOfSpeech   TEXT NOT NULL,"
        "    Gender         INTEGER NOT NULL DEFAULT -1,"
        "    MarkedDate     INTEGER NOT NULL,"
        "    Skipped        INTEGER NOT NULL DEFAULT 0,"
        "    PRIMARY KEY (TargetWord, TargetLanguage, PartOfSpeech, Gender)"
        ");"
        "INSERT INTO TriagedWords VALUES ('chien', 'fr', 'noun', 0, 1, 0);"));
    REQUIRE(Target.Execute("PRAGMA user_version = 12"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
    CHECK_FALSE(ColumnExists(Target, "TriagedWords", "Skipped"));

    // What the user answered is kept; only which of the two ways they answered is gone.
    FTStatement Count = Target.Prepare("SELECT COUNT(*) FROM TriagedWords WHERE TargetWord = 'chien'");
    REQUIRE(Count.Step());
    CHECK(Count.GetInt(0) == 1);
}

TEST_CASE("Migrating tolerates a database that never had the Skipped column", "[schema]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute(
        "CREATE TABLE TriagedWords"
        "("
        "    TargetWord     TEXT NOT NULL COLLATE NOCASE,"
        "    TargetLanguage TEXT NOT NULL DEFAULT '',"
        "    PartOfSpeech   TEXT NOT NULL,"
        "    Gender         INTEGER NOT NULL DEFAULT -1,"
        "    MarkedDate     INTEGER NOT NULL,"
        "    PRIMARY KEY (TargetWord, TargetLanguage, PartOfSpeech, Gender)"
        ");"));
    REQUIRE(Target.Execute("PRAGMA user_version = 12"));

    // Dropping a column that is not there would fail the migration outright, which leaves the
    // database refusing to open rather than merely skipping a step.
    REQUIRE(MigrateSchema(Target));
    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
}

TEST_CASE("Migrating retires the three forms decks with their cards and adds the forms table", "[schema]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateSchema(Target));
    RecreateDeckTables(Target);
    REQUIRE(Target.Execute(
        "DROP TABLE WordForms;"
        "INSERT INTO Decks (Id, Name, CreatedDate, LastUpdatedDate, Role) VALUES "
        "  ('conjugation-fr-en', '', 1, 1, 'conjugation'),"
        "  ('forms-adj-fr-en', '', 1, 1, 'forms-adj'),"
        "  ('forms-noun-fr-en', '', 1, 1, 'forms-noun'),"
        "  ('user-deck', 'Kitchen', 1, 1, '');"
        "INSERT INTO Cards (Id, TargetWord, Translation, CreatedDate, LastUpdatedDate) VALUES "
        "  ('conjugation-fr-vouloir-present', 'vouloir', 'want', 1, 1),"
        "  ('forms-adj-fr-beau', 'beau', 'beautiful', 1, 1),"
        "  ('forms-noun-fr-cheval', 'cheval', 'horse', 1, 1),"
        "  ('word-card', 'vouloir', 'want', 1, 1);"
        "INSERT INTO DeckCards (DeckId, CardId, LastUpdatedDate) VALUES "
        "  ('conjugation-fr-en', 'conjugation-fr-vouloir-present', 1),"
        "  ('forms-adj-fr-en', 'forms-adj-fr-beau', 1),"
        "  ('forms-noun-fr-en', 'forms-noun-fr-cheval', 1),"
        "  ('user-deck', 'word-card', 1);"));
    REQUIRE(Target.Execute("PRAGMA user_version = 20"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
    CHECK(TableExists(Target, "WordForms"));

    FTStatement Cards = Target.Prepare("SELECT Id FROM Cards WHERE Deleted = 0");
    REQUIRE(Cards.Step());
    CHECK(Cards.GetText(0) == "word-card");
    CHECK_FALSE(Cards.Step());

    FTStatement OlderThanTheirEdit = Target.Prepare(
        "SELECT COUNT(*) FROM Cards WHERE Deleted = 1 AND LastUpdatedDate = 0");
    REQUIRE(OlderThanTheirEdit.Step());
    CHECK(OlderThanTheirEdit.GetInt(0) == 3);
}

TEST_CASE("Migrating an already-current database does not touch existing data", "[schema]")
{
    // ReadSchemaVersion(Target) >= CurrentSchemaVersion short-circuits before the DROP/CREATE
    // block runs; this is what makes that early return actually matter.
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));

    FTStatement Add = Target.Prepare(
        "INSERT INTO Cards (Id, TargetWord, Translation, CreatedDate, LastUpdatedDate, Deleted) "
        "VALUES (?1, 'chien', 'dog', 1, 1, 0)");
    Add.BindText(1, MakeUuid());
    REQUIRE(Add.Run());

    REQUIRE(MigrateSchema(Target));

    FTStatement Count = Target.Prepare("SELECT COUNT(*) FROM Cards");
    REQUIRE(Count.Step());
    CHECK(Count.GetInt(0) == 1);
}

TEST_CASE("A fresh database's word index uses case-insensitive collation", "[schema]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));

    CHECK(IndexIsNoCase(Target, "IdxCardsWord"));
}

TEST_CASE("Migrating repairs a case-sensitive word index on an already-current database", "[schema]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));

    REQUIRE(Target.Execute("DROP INDEX IdxCardsWord"));
    REQUIRE(Target.Execute("CREATE INDEX IdxCardsWord ON Cards (TargetWord)"));
    REQUIRE_FALSE(IndexIsNoCase(Target, "IdxCardsWord"));

    REQUIRE(MigrateSchema(Target));

    CHECK(IndexIsNoCase(Target, "IdxCardsWord"));
}

TEST_CASE("A card carries a Leitner stage and its dates", "[schema]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));

    FTStatement Add = Target.Prepare(
        "INSERT INTO Cards (Id, TargetWord, Translation, Definition, UserEditedFields, "
        "                   LeitnerStage, DueDate, BoxChangedDate, CompletedDate, "
        "                   CreatedDate, LastUpdatedDate, Deleted) "
        "VALUES (?1, 'le chien', 'dog', '', 0, 2, 20500, 20497, NULL, 1, 1, 0)");
    REQUIRE(Add.IsValid());
    Add.BindText(1, MakeUuid());
    REQUIRE(Add.Run());

    FTStatement Read = Target.Prepare(
        "SELECT LeitnerStage, DueDate, CompletedDate FROM Cards");
    REQUIRE(Read.Step());

    CHECK(Read.GetInt(0) == 2);
    CHECK(Read.GetInt(1) == 20500);
    CHECK(Read.IsNull(2));
}

TEST_CASE("The system clock returns a plausible epoch time", "[schema]")
{
    // Sanity only. Milliseconds since 1970 passed 1.7 trillion in 2023, so anything below that is
    // seconds mistaken for milliseconds.
    CHECK(GetSystemTimeMilliseconds() > 1700000000000LL);
}

TEST_CASE("A generated UUID has the right shape", "[uuid]")
{
    const std::string Value = MakeUuid();

    REQUIRE(Value.size() == 36);
    CHECK(Value[8] == '-');
    CHECK(Value[13] == '-');
    CHECK(Value[18] == '-');
    CHECK(Value[23] == '-');

    // Version 4 and the RFC variant bits.
    CHECK(Value[14] == '4');
    CHECK((Value[19] == '8' || Value[19] == '9' || Value[19] == 'a' || Value[19] == 'b'));

    CHECK(IsValidUuid(Value));
}

TEST_CASE("Generated UUIDs do not collide", "[uuid]")
{
    std::set<std::string> Seen;

    for (int Index = 0; Index < 2000; ++Index)
    {
        Seen.insert(MakeUuid());
    }

    CHECK(Seen.size() == 2000);
}

TEST_CASE("Junk is not a valid UUID", "[uuid]")
{
    CHECK_FALSE(IsValidUuid(""));
    CHECK_FALSE(IsValidUuid("nonsense"));
    CHECK_FALSE(IsValidUuid("8f14e45f-ea8b-4c3d-9a72"));
    CHECK_FALSE(IsValidUuid("8f14e45fXea8bX4c3dX9a72Xb1e0d7c8a501"));
    CHECK_FALSE(IsValidUuid("8F14E45F-EA8B-4C3D-9A72-B1E0D7C8A501"));
    CHECK_FALSE(IsValidUuid("8f14e45f-ea8b-4c3d-9a72-b1e0d7c8a501-extra"));
}


namespace
{
    /** An instant on a given local date and local clock time, as the epoch milliseconds the app
        reads from the system. */
    int64_t LocalInstant(const QDate& Day, int Hour, int Minute)
    {
        return QDateTime(Day, QTime(Hour, Minute)).toMSecsSinceEpoch();
    }
}

TEST_CASE("The day a card belongs to is the local one", "[schema]")
{
    const QDate Day(2026, 8, 21);

    CHECK(DayNumberForMilliseconds(LocalInstant(Day, 9, 0))
          == DayNumberForMilliseconds(LocalInstant(Day, 23, 30)));
}

TEST_CASE("The day turns at two in the morning, not at midnight", "[schema]")
{
    const QDate Day(2026, 8, 21);
    const QDate Next = Day.addDays(1);

    const FTDayNumber Evening = DayNumberForMilliseconds(LocalInstant(Day, 22, 0));

    CHECK(DayNumberForMilliseconds(LocalInstant(Next, 0, 30)) == Evening);
    CHECK(DayNumberForMilliseconds(LocalInstant(Next, 1, 59)) == Evening);
    CHECK(DayNumberForMilliseconds(LocalInstant(Next, 2, 0)) == Evening + 1);
}

TEST_CASE("Consecutive days are consecutive numbers", "[schema]")
{
    const QDate Day(2026, 8, 21);

    for (int Ahead = 0; Ahead < 400; ++Ahead)
    {
        CHECK(DayNumberForMilliseconds(LocalInstant(Day.addDays(Ahead), 12, 0))
              == DayNumberForMilliseconds(LocalInstant(Day, 12, 0)) + Ahead);
    }
}

TEST_CASE("Migrating from the version before placements adds their table", "[schema][placement]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateSchema(Target));
    REQUIRE(Target.Execute("DROP TABLE Placements"));
    REQUIRE(Target.Execute("PRAGMA user_version = 22"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
    CHECK(TableExists(Target, "Placements"));
}

TEST_CASE("Migrating from the version before the week strip keeps the days already recorded", "[schema]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateSchema(Target));
    REQUIRE(Target.Execute("ALTER TABLE DailyStats DROP COLUMN DueCount"));
    REQUIRE(Target.Execute("ALTER TABLE DailyStats DROP COLUMN GoalCount"));
    REQUIRE(Target.Execute("INSERT INTO DailyStats (Day, TargetLanguage, NativeLanguage, LearnedCount, "
                           "ReviewedCount, LastUpdatedDate) VALUES (100, 'fr', 'en', 6, 12, 1)"));
    REQUIRE(Target.Execute("PRAGMA user_version = 23"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);

    FTStatement Query = Target.Prepare("SELECT LearnedCount, ReviewedCount, DueCount, GoalCount FROM DailyStats");

    REQUIRE(Query.Step());
    CHECK(Query.GetInt(0) == 6);
    CHECK(Query.GetInt(1) == 12);
    CHECK(Query.GetInt(2) == 0);
    CHECK(Query.GetInt(3) == 0);
}

TEST_CASE("Migrating from the version before devices keeps the days and gives them this device",
          "[schema][sync]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateSchema(Target));

    const std::string DeviceId = ReadLocalDeviceId(Target);
    REQUIRE_FALSE(DeviceId.empty());

    REQUIRE(Target.Execute("DROP TABLE DailyStats"));
    REQUIRE(Target.Execute("CREATE TABLE DailyStats (Day INTEGER NOT NULL, "
                           "TargetLanguage TEXT NOT NULL DEFAULT '', "
                           "NativeLanguage TEXT NOT NULL DEFAULT '', "
                           "LearnedCount INTEGER NOT NULL DEFAULT 0, "
                           "ReviewedCount INTEGER NOT NULL DEFAULT 0, "
                           "DueCount INTEGER NOT NULL DEFAULT 0, "
                           "GoalCount INTEGER NOT NULL DEFAULT 0, "
                           "LastUpdatedDate INTEGER NOT NULL, "
                           "PRIMARY KEY (Day, TargetLanguage, NativeLanguage))"));
    REQUIRE(Target.Execute("INSERT INTO DailyStats VALUES (100, 'fr', 'en', 6, 12, 3, 15, 1)"));
    REQUIRE(Target.Execute("PRAGMA user_version = 25"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
    CHECK(ReadLocalDeviceId(Target) == DeviceId);

    FTStatement Query = Target.Prepare(
        "SELECT DeviceId, LearnedCount, ReviewedCount, DueCount, GoalCount FROM DailyStats");

    REQUIRE(Query.Step());
    CHECK(Query.GetText(0) == DeviceId);
    CHECK(Query.GetInt(1) == 6);
    CHECK(Query.GetInt(2) == 12);
    CHECK(Query.GetInt(3) == 3);
    CHECK(Query.GetInt(4) == 15);
    CHECK_FALSE(Query.Step());
}

TEST_CASE("Two databases get different device ids", "[schema][sync]")
{
    FTDatabase First = MakeDatabase();
    FTDatabase Second = MakeDatabase();

    REQUIRE(MigrateSchema(First));
    REQUIRE(MigrateSchema(Second));

    CHECK(ReadLocalDeviceId(First) != ReadLocalDeviceId(Second));
}

TEST_CASE("Migrating from the version before synced settings adds their table", "[schema][sync]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(MigrateSchema(Target));
    REQUIRE(Target.Execute("DROP TABLE SyncedSettings"));
    REQUIRE(Target.Execute("PRAGMA user_version = 26"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);
    CHECK(TableExists(Target, "SyncedSettings"));
}

TEST_CASE("Card hints migrate without changing existing cards", "[schema][hint]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));
    REQUIRE(ColumnExists(Target, "Cards", "Hint"));
    REQUIRE(Target.Execute("INSERT INTO Cards (Id, TargetWord, Translation, CreatedDate, LastUpdatedDate) VALUES ('hint-card', 'chat', 'cat', 1, 1)"));
    REQUIRE(Target.Execute("ALTER TABLE Cards DROP COLUMN Hint"));
    REQUIRE(Target.Execute("PRAGMA user_version = 34"));
    REQUIRE(MigrateSchema(Target));
    FTStatement Query = Target.Prepare("SELECT TargetWord, Hint FROM Cards WHERE Id = 'hint-card'");
    REQUIRE(Query.IsValid());
    REQUIRE(Query.Step());
    CHECK(Query.GetText(0) == "chat");
    CHECK(Query.GetText(1).empty());
}

TEST_CASE("Known marks migrate to whole-reading marks with an empty meaning", "[schema][triage]")
{
    FTDatabase Target = MakeDatabase();
    REQUIRE(MigrateSchema(Target));
    REQUIRE(Target.Execute("DROP TABLE TriagedWords"));
    REQUIRE(Target.Execute("CREATE TABLE TriagedWords (TargetWord TEXT NOT NULL COLLATE NOCASE, "
                           "TargetLanguage TEXT NOT NULL DEFAULT '', PartOfSpeech TEXT NOT NULL, "
                           "Gender INTEGER NOT NULL DEFAULT -1, MarkedDate INTEGER NOT NULL, "
                           "PRIMARY KEY (TargetWord, TargetLanguage, PartOfSpeech, Gender))"));
    REQUIRE(Target.Execute("INSERT INTO TriagedWords VALUES ('pêche', 'fr', 'noun', 1, 5)"));
    REQUIRE(Target.Execute("PRAGMA user_version = 36"));

    REQUIRE(MigrateSchema(Target));

    CHECK(ReadSchemaVersion(Target) == CurrentSchemaVersion);

    FTStatement Query = Target.Prepare("SELECT TargetWord, MeaningGist, MarkedDate FROM TriagedWords");
    REQUIRE(Query.IsValid());
    REQUIRE(Query.Step());
    CHECK(Query.GetText(0) == "pêche");
    CHECK(Query.GetText(1).empty());
    CHECK(Query.GetInt(2) == 5);

    REQUIRE(Target.Execute("INSERT INTO TriagedWords (TargetWord, TargetLanguage, PartOfSpeech, Gender, MeaningGist, MarkedDate) "
                           "VALUES ('pêche', 'fr', 'noun', 1, 'fruit', 6)"));
}
