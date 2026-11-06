#include "core/FTSchema.h"

#include "core/FTDatabase.h"
#include "core/FTJournal.h"
#include "core/FTLog.h"

#include <QDateTime>
#include <QSettings>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <string>

// Generated from core/resources/schema.sql at configure time — the single source of truth for the
// trainer schema. See core/schema_sql.h.in and core/CMakeLists.txt.
#include "schema_sql.h"

namespace
{
    /** SQLite has no DROP COLUMN IF EXISTS, and a migration that fails leaves the database
        refusing to open, so a column being gone already has to be a question rather than a crash. */
    bool ColumnExists(FTDatabase& Target, const std::string& Table, const std::string& Column)
    {
        FTStatement Query = Target.Prepare("PRAGMA table_info(" + Table + ")");

        if (!Query.IsValid())
        {
            return false;
        }

        while (Query.Step())
        {
            if (Query.GetText(1) == Column)
            {
                return true;
            }
        }

        return false;
    }

    /** The same tolerance for a step that alters a table which may never have been created. */
    bool TableExists(FTDatabase& Target, const std::string& Table)
    {
        FTStatement Query = Target.Prepare(
            "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?1 LIMIT 1");
        Query.BindText(1, Table);

        return Query.IsValid() && Query.Step();
    }

    bool IndexIsNoCase(FTDatabase& Target, const std::string& IndexName)
    {
        FTStatement Query = Target.Prepare(
            "SELECT sql FROM sqlite_master WHERE type = 'index' AND name = ?1");
        Query.BindText(1, IndexName);

        if (!Query.IsValid() || !Query.Step())
        {
            return false;
        }

        return Query.GetText(0).find("COLLATE NOCASE") != std::string::npos;
    }

    constexpr const char* ReaderDocumentsDefinition =
        "CREATE TABLE IF NOT EXISTS ReaderDocuments"
        "("
        "    Id             TEXT PRIMARY KEY NOT NULL,"
        "    FilePath       TEXT NOT NULL,"
        "    Title          TEXT NOT NULL DEFAULT '',"
        "    Format         TEXT NOT NULL DEFAULT '',"
        "    AddedDate      INTEGER NOT NULL,"
        "    LastOpenedDate INTEGER NOT NULL,"
        "    Deleted        INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS IdxReaderDocumentsOpened ON ReaderDocuments (LastOpenedDate);";

    constexpr const char* YoutubeVideosDefinition =
        "CREATE TABLE IF NOT EXISTS YoutubeVideos"
        "("
        "    Id             TEXT PRIMARY KEY NOT NULL,"
        "    VideoId        TEXT NOT NULL,"
        "    Title          TEXT NOT NULL DEFAULT '',"
        "    AddedDate      INTEGER NOT NULL,"
        "    LastOpenedDate INTEGER NOT NULL,"
        "    Deleted        INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS IdxYoutubeVideosOpened ON YoutubeVideos (LastOpenedDate);";

    constexpr const char* DeviceReaderDocumentsDefinition =
        "CREATE TABLE IF NOT EXISTS ReaderDocuments"
        "("
        "    Id             TEXT PRIMARY KEY NOT NULL,"
        "    FilePath       TEXT NOT NULL,"
        "    Title          TEXT NOT NULL DEFAULT '',"
        "    Format         TEXT NOT NULL DEFAULT '',"
        "    AddedDate      INTEGER NOT NULL,"
        "    LastOpenedDate INTEGER NOT NULL,"
        "    LastPosition   INTEGER NOT NULL DEFAULT 0,"
        "    LastZoom       REAL NOT NULL DEFAULT 0,"
        "    LastScrollX    REAL NOT NULL DEFAULT 0,"
        "    LastScrollY    REAL NOT NULL DEFAULT 0,"
        "    CloudId        TEXT NOT NULL DEFAULT '',"
        "    PositionCount  INTEGER NOT NULL DEFAULT 0,"
        "    Deleted        INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS IdxReaderDocumentsOpened ON ReaderDocuments (LastOpenedDate);";

    constexpr const char* CloudBooksDefinition =
        "CREATE TABLE IF NOT EXISTS CloudBooks"
        "("
        "    Id              TEXT PRIMARY KEY NOT NULL,"
        "    Title           TEXT NOT NULL DEFAULT '',"
        "    Format          TEXT NOT NULL DEFAULT '',"
        "    FileSize        INTEGER NOT NULL DEFAULT 0,"
        "    DriveFileId     TEXT NOT NULL DEFAULT '',"
        "    AddedDate       INTEGER NOT NULL,"
        "    LastOpenedDate  INTEGER NOT NULL,"
        "    LastPosition    INTEGER NOT NULL DEFAULT 0,"
        "    LastZoom        REAL NOT NULL DEFAULT 0,"
        "    LastScrollX     REAL NOT NULL DEFAULT 0,"
        "    LastScrollY     REAL NOT NULL DEFAULT 0,"
        "    PositionCount   INTEGER NOT NULL DEFAULT 0,"
        "    LastUpdatedDate INTEGER NOT NULL,"
        "    Deleted         INTEGER NOT NULL DEFAULT 0"
        ");";

    constexpr const char* JournalDefinition =
        "CREATE TABLE IF NOT EXISTS Journal"
        "("
        "    Id         TEXT PRIMARY KEY NOT NULL,"
        "    EventId    TEXT NOT NULL,"
        "    CardId     TEXT NOT NULL,"
        "    EventValue INTEGER NOT NULL DEFAULT 0,"
        "    Day        INTEGER NOT NULL,"
        "    DueDate    INTEGER"
        ");"
        "CREATE INDEX IF NOT EXISTS IdxJournalCard ON Journal (CardId, Day);";

    constexpr const char* ErasuresDefinition =
        "CREATE TABLE IF NOT EXISTS Erasures"
        "("
        "    Scope      TEXT PRIMARY KEY NOT NULL,"
        "    ErasedDate INTEGER NOT NULL"
        ");";

    constexpr const char* WordFormsDefinition =
        "CREATE TABLE IF NOT EXISTS WordForms"
        "("
        "    Id              TEXT PRIMARY KEY NOT NULL,"
        "    TargetWord      TEXT NOT NULL,"
        "    Translation     TEXT NOT NULL DEFAULT '',"
        "    TargetLanguage  TEXT NOT NULL DEFAULT '',"
        "    NativeLanguage  TEXT NOT NULL DEFAULT '',"
        "    LearnedDate     INTEGER,"
        "    CreatedDate     INTEGER NOT NULL,"
        "    LastUpdatedDate INTEGER NOT NULL,"
        "    Deleted         INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS IdxWordFormsLanguages ON WordForms (TargetLanguage, NativeLanguage);";

    constexpr const char* PlacementsDefinition =
        "CREATE TABLE IF NOT EXISTS Placements"
        "("
        "    TargetLanguage  TEXT PRIMARY KEY NOT NULL,"
        "    CutoffRank      INTEGER NOT NULL DEFAULT 0,"
        "    LastUpdatedDate INTEGER NOT NULL"
        ");";

    constexpr const char* LocalStateDefinition =
        "CREATE TABLE IF NOT EXISTS LocalState"
        "("
        "    Key   TEXT PRIMARY KEY NOT NULL,"
        "    Value TEXT NOT NULL"
        ");";

    constexpr const char* SyncedSettingsDefinition =
        "CREATE TABLE IF NOT EXISTS SyncedSettings"
        "("
        "    Key             TEXT PRIMARY KEY NOT NULL,"
        "    Value           TEXT NOT NULL,"
        "    LastUpdatedDate INTEGER NOT NULL"
        ");";

    constexpr const char* DailyStatsWithDevicesDefinition =
        "CREATE TABLE DailyStats"
        "("
        "    Day             INTEGER NOT NULL,"
        "    TargetLanguage  TEXT NOT NULL DEFAULT '',"
        "    NativeLanguage  TEXT NOT NULL DEFAULT '',"
        "    DeviceId        TEXT NOT NULL DEFAULT '',"
        "    LearnedCount    INTEGER NOT NULL DEFAULT 0,"
        "    ReviewedCount   INTEGER NOT NULL DEFAULT 0,"
        "    DueCount        INTEGER NOT NULL DEFAULT 0,"
        "    GoalCount       INTEGER NOT NULL DEFAULT 0,"
        "    LastUpdatedDate INTEGER NOT NULL,"
        "    PRIMARY KEY (Day, TargetLanguage, NativeLanguage, DeviceId)"
        ");";

    bool EnsureDeviceId(FTDatabase& Target)
    {
        return Target.Execute(LocalStateDefinition)
            && Target.Execute("INSERT OR IGNORE INTO LocalState (Key, Value) "
                              "VALUES ('DeviceId', lower(hex(randomblob(16))))");
    }

    /**
     * A day's counts used to be one row, so two devices studying the same day kept one device's
     * counts and lost the other's. Now each device has a row of its own. Rows already here belong to
     * this device, whatever device they were first written on, because they were merged into this
     * copy under the old rule and are this copy's to carry.
     */
    bool GiveDailyStatsADevice(FTDatabase& Target)
    {
        if (!EnsureDeviceId(Target))
        {
            return false;
        }

        if (!TableExists(Target, "DailyStats") || ColumnExists(Target, "DailyStats", "DeviceId"))
        {
            return true;
        }

        return Target.Execute("ALTER TABLE DailyStats RENAME TO DailyStatsBeforeDevices")
            && Target.Execute(DailyStatsWithDevicesDefinition)
            && Target.Execute(
                "INSERT INTO DailyStats (Day, TargetLanguage, NativeLanguage, DeviceId, LearnedCount, "
                "                        ReviewedCount, DueCount, GoalCount, LastUpdatedDate) "
                "SELECT Day, TargetLanguage, NativeLanguage, "
                "       (SELECT Value FROM LocalState WHERE Key = 'DeviceId'), LearnedCount, "
                "       ReviewedCount, DueCount, GoalCount, LastUpdatedDate "
                "FROM DailyStatsBeforeDevices")
            && Target.Execute("DROP TABLE DailyStatsBeforeDevices");
    }

    bool RetireFormsDecks(FTDatabase& Target)
    {
        if (!TableExists(Target, "Decks") || !TableExists(Target, "Cards") || !TableExists(Target, "DeckCards"))
        {
            return true;
        }

        const std::string FormsDecks =
            "SELECT Id FROM Decks WHERE Role IN ('conjugation', 'forms-adj', 'forms-noun')";

        return Target.Execute("UPDATE Cards SET Deleted = 1, LastUpdatedDate = LastUpdatedDate - 1 "
                              "WHERE Deleted = 0 AND Id IN (SELECT CardId FROM DeckCards "
                              "                             WHERE DeckId IN (" + FormsDecks + "))")
            && Target.Execute("UPDATE DeckCards SET Deleted = 1, LastUpdatedDate = LastUpdatedDate - 1 "
                              "WHERE Deleted = 0 AND DeckId IN (" + FormsDecks + ")")
            && Target.Execute("UPDATE Decks SET Deleted = 1, LastUpdatedDate = LastUpdatedDate - 1 "
                              "WHERE Deleted = 0 AND Role IN ('conjugation', 'forms-adj', 'forms-noun')");
    }

    bool EnsureJournalDueDate(FTDatabase& Target)
    {
        return !TableExists(Target, "Journal") || ColumnExists(Target, "Journal", "DueDate")
            || Target.Execute("ALTER TABLE Journal ADD COLUMN DueDate INTEGER");
    }

    bool MoveCardsOffDecks(FTDatabase& Target)
    {
        for (const char* Column : {"TargetLanguage", "NativeLanguage"})
        {
            if (!ColumnExists(Target, "Cards", Column)
                && !Target.Execute(std::string("ALTER TABLE Cards ADD COLUMN ") + Column + " TEXT NOT NULL DEFAULT ''"))
            {
                return false;
            }
        }

        if (!Target.Execute("CREATE INDEX IF NOT EXISTS IdxCardsLanguages ON Cards (TargetLanguage, NativeLanguage)"))
        {
            return false;
        }

        if (!TableExists(Target, "Decks") || !TableExists(Target, "DeckCards"))
        {
            return Target.Execute("DROP TABLE IF EXISTS DeckCards") && Target.Execute("DROP TABLE IF EXISTS Decks");
        }

        const auto ReadFromDeck = [](const std::string& Column)
        {
            return "COALESCE((SELECT D." + Column + " FROM DeckCards M JOIN Decks D ON D.Id = M.DeckId "
                   "WHERE M.CardId = Cards.Id "
                   "ORDER BY M.Deleted = 0 AND D.Deleted = 0 DESC, D.Role = 'pool' DESC LIMIT 1), '')";
        };

        return Target.Execute("UPDATE Cards SET TargetLanguage = " + ReadFromDeck("TargetLanguage") + ", "
                              "NativeLanguage = " + ReadFromDeck("NativeLanguage"))
            && Target.Execute("UPDATE Cards SET Deleted = 1, LastUpdatedDate = LastUpdatedDate - 1 "
                              "WHERE Deleted = 0 AND NOT EXISTS ("
                              "  SELECT 1 FROM DeckCards M JOIN Decks D ON D.Id = M.DeckId "
                              "  WHERE M.CardId = Cards.Id AND M.Deleted = 0 AND D.Deleted = 0 AND D.Role <> 'drill')")
            && Target.Execute("DROP TABLE DeckCards")
            && Target.Execute("DROP TABLE Decks");
    }

    bool SetSchemaVersion(FTDatabase& Target, int Version)
    {
        // PRAGMA user_version takes no bind parameters, so the number is built into the text.
        // It is an int from our own constant, not anything a caller supplies.
        return Target.Execute("PRAGMA user_version = " + std::to_string(Version));
    }
}

int ReadSchemaVersion(FTDatabase& Target)
{
    const FTQueryTimer Timer("ReadSchemaVersion");

    FTStatement Query = Target.Prepare("PRAGMA user_version");

    if (!Query.IsValid() || !Query.Step())
    {
        return 0;
    }

    return Query.GetInt(0);
}

bool MigrateDeviceSchema(FTDatabase& Target)
{
    const FTQueryTimer Timer("MigrateDeviceSchema");

    return Target.IsOpen() && Target.Execute(DeviceReaderDocumentsDefinition)
        && (ColumnExists(Target, "ReaderDocuments", "CloudId")
            || Target.Execute("ALTER TABLE ReaderDocuments ADD COLUMN CloudId TEXT NOT NULL DEFAULT ''"))
        && (ColumnExists(Target, "ReaderDocuments", "PositionCount")
            || Target.Execute("ALTER TABLE ReaderDocuments ADD COLUMN PositionCount INTEGER NOT NULL DEFAULT 0"))
        && Target.Execute(YoutubeVideosDefinition);
}

bool MigrateSchema(FTDatabase& Target)
{
    const FTQueryTimer Timer("MigrateSchema");

    if (!Target.IsOpen())
    {
        return false;
    }

    if (TableExists(Target, "Cards") && !IndexIsNoCase(Target, "IdxCardsWord"))
    {
        if (!Target.Execute("DROP INDEX IF EXISTS IdxCardsWord")
            || !Target.Execute("CREATE INDEX IdxCardsWord ON Cards (TargetWord COLLATE NOCASE)"))
        {
            return false;
        }
    }

    const int Existing = ReadSchemaVersion(Target);

    if (Existing >= CurrentSchemaVersion)
    {
        return true;
    }

    if (!Target.BeginTransaction())
    {
        return false;
    }

    /*
     * Each block below moves Reached forward to the version it produces, and the next one picks up
     * from there. Sequential rather than a chain of alternatives: a database several versions
     * behind has to run every step between where it is and here, and an else-if would run only the
     * first one that matched and then stamp the current version over the rest.
     *
     * A new migration is one more `if (Reached < N)` block ending in `Reached = N`.
     */
    int Reached = Existing;

    if (Reached == 0)
    {
        if (!Target.Execute(FTDetail::kSchemaSql))
        {
            Target.Rollback();

            return false;
        }

        Reached = CurrentSchemaVersion;
    }

    if (Reached < 12)
    {
        if (!Target.Execute(ReaderDocumentsDefinition))
        {
            Target.Rollback();

            return false;
        }

        Reached = 12;
    }

    if (Reached < 13)
    {
        // Skipped recorded which of two ways a word left triage, but nothing ever read it, and
        // turning a word down is now a decision that lives in the run rather than in the database.
        if (ColumnExists(Target, "TriagedWords", "Skipped")
            && !Target.Execute("ALTER TABLE TriagedWords DROP COLUMN Skipped"))
        {
            Target.Rollback();

            return false;
        }

        Reached = 13;
    }

    if (Reached < 14)
    {
        // The stage a run had reached was written on every advance and read by nothing. A run is
        // deliberately not persistent - it survives leaving the training screen and not closing
        // the app - so a column recording its position could never mean anything, and quitting
        // mid-run left a number behind that sync then carried to other devices.
        if (ColumnExists(Target, "Decks", "IntensiveLearningStage")
            && !Target.Execute("ALTER TABLE Decks DROP COLUMN IntensiveLearningStage"))
        {
            Target.Rollback();

            return false;
        }

        Reached = 14;
    }

    if (Reached < 15)
    {
        // A deck's kind was inferred by matching a prefix on its id, which only worked because the
        // Learning Pool is the one deck with a made-up id. Category decks need the same answer and
        // cannot get it that way, so the kind is a column now. Existing pools are stamped from the
        // id they already have; their ids are left alone, since DeckCards references them and sync
        // keys on them.
        //
        // Guarded on the table existing, since a database a few versions behind predates most of
        // the current schema and reaching here the first time means its CardRepository predates the columns.
        if (TableExists(Target, "Decks"))
        {
            if (!ColumnExists(Target, "Decks", "Role")
                && !Target.Execute("ALTER TABLE Decks ADD COLUMN Role TEXT NOT NULL DEFAULT ''"))
            {
                Target.Rollback();

                return false;
            }

            if (!ColumnExists(Target, "Decks", "SourceKey")
                && !Target.Execute("ALTER TABLE Decks ADD COLUMN SourceKey TEXT NOT NULL DEFAULT ''"))
            {
                Target.Rollback();

                return false;
            }

            if (!Target.Execute("UPDATE Decks SET Role = 'pool' WHERE Id LIKE 'learning-pool%'"))
            {
                Target.Rollback();

                return false;
            }
        }

        Reached = 15;
    }

    if (Reached < 16)
    {
        // ReaderDocuments again. Version 12 was stamped two commits before the table was added to
        // schema.sql, and the step that creates it is guarded on being below 12, so every database
        // created in that window is missing the table and has a version that says otherwise. They
        // reached 15 the same way and sync fails on them at every cycle. Repeating the step under a
        // version above the whole affected range is the only way to reach them; it is the same
        // CREATE IF NOT EXISTS, so a database that already has the table is untouched.
        if (!Target.Execute(ReaderDocumentsDefinition))
        {
            Target.Rollback();

            return false;
        }

        Reached = 16;
    }

    if (Reached < 17)
    {
        if (!Target.Execute(YoutubeVideosDefinition))
        {
            Target.Rollback();

            return false;
        }

        Reached = 17;
    }

    if (Reached < 18)
    {
        if (TableExists(Target, "SearchHistory"))
        {
            if (!ColumnExists(Target, "SearchHistory", "PartOfSpeech")
                && !Target.Execute(
                    "ALTER TABLE SearchHistory ADD COLUMN PartOfSpeech TEXT NOT NULL DEFAULT ''"))
            {
                Target.Rollback();

                return false;
            }

            if (!ColumnExists(Target, "SearchHistory", "WordId")
                && !Target.Execute(
                    "ALTER TABLE SearchHistory ADD COLUMN WordId INTEGER NOT NULL DEFAULT 0"))
            {
                Target.Rollback();

                return false;
            }
        }

        Reached = 18;
    }

    if (Reached < 19)
    {
        if (TableExists(Target, "ReaderDocuments")
            && !ColumnExists(Target, "ReaderDocuments", "LastPosition")
            && !Target.Execute(
                "ALTER TABLE ReaderDocuments ADD COLUMN LastPosition INTEGER NOT NULL DEFAULT 0"))
        {
            Target.Rollback();

            return false;
        }

        Reached = 19;
    }

    if (Reached < 20)
    {
        if (TableExists(Target, "ReaderDocuments")
            && !ColumnExists(Target, "ReaderDocuments", "LastZoom")
            && !Target.Execute(
                "ALTER TABLE ReaderDocuments ADD COLUMN LastZoom REAL NOT NULL DEFAULT 0"))
        {
            Target.Rollback();

            return false;
        }

        Reached = 20;
    }

    if (Reached < 21)
    {
        if (!Target.Execute(WordFormsDefinition) || !RetireFormsDecks(Target))
        {
            Target.Rollback();

            return false;
        }

        Reached = 21;
    }

    if (Reached < 22)
    {
        if (TableExists(Target, "ReaderDocuments"))
        {
            if (!ColumnExists(Target, "ReaderDocuments", "LastScrollX")
                && !Target.Execute(
                    "ALTER TABLE ReaderDocuments ADD COLUMN LastScrollX REAL NOT NULL DEFAULT 0"))
            {
                Target.Rollback();

                return false;
            }

            if (!ColumnExists(Target, "ReaderDocuments", "LastScrollY")
                && !Target.Execute(
                    "ALTER TABLE ReaderDocuments ADD COLUMN LastScrollY REAL NOT NULL DEFAULT 0"))
            {
                Target.Rollback();

                return false;
            }
        }

        Reached = 22;
    }

    if (Reached < 23)
    {
        if (!Target.Execute(PlacementsDefinition))
        {
            Target.Rollback();

            return false;
        }

        Reached = 23;
    }

    if (Reached < 24)
    {
        for (const char* Column : {"DueCount", "GoalCount"})
        {
            if (TableExists(Target, "DailyStats") && !ColumnExists(Target, "DailyStats", Column)
                && !Target.Execute(std::string("ALTER TABLE DailyStats ADD COLUMN ") + Column
                                   + " INTEGER NOT NULL DEFAULT 0"))
            {
                Target.Rollback();

                return false;
            }
        }

        Reached = 24;
    }

    if (Reached < 25)
    {
        if (TableExists(Target, "Cards") && !ColumnExists(Target, "Cards", "MeaningGist")
            && !Target.Execute("ALTER TABLE Cards ADD COLUMN MeaningGist TEXT NOT NULL DEFAULT ''"))
        {
            Target.Rollback();

            return false;
        }

        Reached = 25;
    }

    if (Reached < 26)
    {
        if (!GiveDailyStatsADevice(Target))
        {
            Target.Rollback();

            return false;
        }

        Reached = 26;
    }

    if (Reached < 27)
    {
        if (!Target.Execute(SyncedSettingsDefinition))
        {
            Target.Rollback();

            return false;
        }

        Reached = 27;
    }

    if (Reached < 28)
    {
        if (!Target.Execute(JournalDefinition)
            || !Target.Execute("DROP TABLE IF EXISTS ReaderDocuments")
            || !Target.Execute("DROP TABLE IF EXISTS YoutubeVideos"))
        {
            Target.Rollback();

            return false;
        }

        Reached = 28;
    }

    if (Reached < 29)
    {
        if (!Target.Execute(ErasuresDefinition))
        {
            Target.Rollback();

            return false;
        }

        Reached = 29;
    }

    if (Reached < 30)
    {
        if (!EnsureJournalDueDate(Target) || (TableExists(Target, "Cards") && !SeedStartingEvents(Target)))
        {
            Target.Rollback();

            return false;
        }

        Reached = 30;
    }

    if (Reached < 31)
    {
        if (!EnsureJournalDueDate(Target))
        {
            Target.Rollback();

            return false;
        }

        Reached = 31;
    }

    if (Reached < 32)
    {
        if (TableExists(Target, "Cards") && !MoveCardsOffDecks(Target))
        {
            Target.Rollback();

            return false;
        }

        Reached = 32;
    }

    if (Reached < 33)
    {
        if (!Target.Execute(CloudBooksDefinition))
        {
            Target.Rollback();

            return false;
        }

        Reached = 33;
    }

    if (Reached < 34)
    {
        if (TableExists(Target, "CloudBooks") && !ColumnExists(Target, "CloudBooks", "PositionCount")
            && !Target.Execute("ALTER TABLE CloudBooks ADD COLUMN PositionCount INTEGER NOT NULL DEFAULT 0"))
        {
            Target.Rollback();

            return false;
        }

        Reached = 34;
    }

    if (Reached < 35)
    {
        if (TableExists(Target, "Cards") && !ColumnExists(Target, "Cards", "Hint")
            && !Target.Execute("ALTER TABLE Cards ADD COLUMN Hint TEXT NOT NULL DEFAULT ''"))
        {
            Target.Rollback();

            return false;
        }

        Reached = 35;
    }

    if (Reached < 36)
    {
        const FTDayNumber Today{GetTodayDayNumber()};
        std::vector<std::string> CardIds;

        if (TableExists(Target, "Cards"))
        {
            FTStatement Query{Target.Prepare(
                "SELECT Id FROM Cards WHERE Deleted = 0 AND LeitnerStage = 3 AND CompletedDate IS NOT NULL")};

            if (!Query.IsValid())
            {
                Target.Rollback();
                return false;
            }

            while (Query.Step())
            {
                CardIds.push_back(Query.GetText(0));
            }

            if (Query.HasFailed())
            {
                Target.Rollback();
                return false;
            }
        }

        for (const std::string& CardId : CardIds)
        {
            FTJournalEvent Event{MakeJournalEvent(CardId, JournalPlaced, 4, Today)};
            Event.DueDate = Today + 14;
            FTStatement Update{Target.Prepare(
                "UPDATE Cards SET LeitnerStage = 4, CompletedDate = NULL, BoxChangedDate = ?2, DueDate = ?3 WHERE Id = ?1")};
            Update.BindText(1, CardId);
            Update.BindInt(2, Today);
            Update.BindInt(3, *Event.DueDate);

            if (!AppendJournalEvent(Target, Event) || !Update.Run())
            {
                Target.Rollback();
                return false;
            }
        }

        Reached = 36;
    }

    if (Reached != CurrentSchemaVersion)
    {
        // A database sitting between a real release and the current schema, with nothing above
        // covering the gap. Refusing beats guessing: stamping CurrentSchemaVersion onto a database
        // that never actually ran the intervening changes would make it look upgraded while
        // silently missing whatever those changes added.
        Target.Rollback();

        return false;
    }

    if (!EnsureDeviceId(Target) || !SetSchemaVersion(Target, CurrentSchemaVersion))
    {
        Target.Rollback();

        return false;
    }

    return Target.Commit();
}

std::string ReadLocalDeviceId(FTDatabase& Target)
{
    if (!TableExists(Target, "LocalState"))
    {
        return {};
    }

    FTStatement Query = Target.Prepare("SELECT Value FROM LocalState WHERE Key = 'DeviceId'");

    return Query.IsValid() && Query.Step() ? Query.GetText(0) : std::string{};
}

namespace
{
    std::atomic<int64_t> ClockSkew{0};
    std::atomic<int64_t> NewestStamp{0};
}

int64_t GetRawSystemTimeMilliseconds()
{
    const auto Now = std::chrono::system_clock::now().time_since_epoch();

    return std::chrono::duration_cast<std::chrono::milliseconds>(Now).count();
}

int64_t GetSystemTimeMilliseconds()
{
    const int64_t Corrected = GetRawSystemTimeMilliseconds() + ClockSkew.load();

    int64_t Newest = NewestStamp.load();

    while (Corrected > Newest && !NewestStamp.compare_exchange_weak(Newest, Corrected))
    {
    }

    return std::max(Corrected, Newest);
}

void ObserveServerTime(int64_t ServerMilliseconds)
{
    const int64_t Difference = ServerMilliseconds - GetRawSystemTimeMilliseconds();

    SetClockSkewMilliseconds(std::llabs(Difference) > MaximumClockDriftMilliseconds ? Difference : 0);
}

void SetClockSkewMilliseconds(int64_t Skew)
{
    ClockSkew.store(Skew);
}

int64_t GetClockSkewMilliseconds()
{
    return ClockSkew.load();
}

void NoteSeenTimestamp(int64_t Milliseconds)
{
    const int64_t Corrected = GetRawSystemTimeMilliseconds() + ClockSkew.load();

    if (Milliseconds > Corrected + MaximumClockDriftMilliseconds)
    {
        return;
    }

    int64_t Newest = NewestStamp.load();

    while (Milliseconds > Newest && !NewestStamp.compare_exchange_weak(Newest, Milliseconds))
    {
    }
}

int64_t ClampFutureTimestamp(int64_t Milliseconds)
{
    const int64_t Corrected = GetRawSystemTimeMilliseconds() + ClockSkew.load();

    return Milliseconds > Corrected + MaximumClockDriftMilliseconds ? Corrected : Milliseconds;
}

bool StoreClockSkew(FTDatabase& Target, int64_t Skew)
{
    return Target.Execute(LocalStateDefinition)
        && Target.Execute("INSERT INTO LocalState (Key, Value) VALUES ('ClockSkewMs', '"
                          + std::to_string(Skew)
                          + "') ON CONFLICT(Key) DO UPDATE SET Value = excluded.Value");
}

int64_t LoadClockSkew(FTDatabase& Target)
{
    if (!TableExists(Target, "LocalState"))
    {
        return 0;
    }

    FTStatement Query = Target.Prepare("SELECT Value FROM LocalState WHERE Key = 'ClockSkewMs'");

    if (!Query.IsValid() || !Query.Step())
    {
        return 0;
    }

    try
    {
        return std::stoll(Query.GetText(0));
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

#ifdef QT_DEBUG
namespace
{
    constexpr const char* DebugDayOffsetKey = "debug/dayOffset";
}

void SetDebugDayOffset(int Days)
{
    QSettings Settings;
    Settings.setValue(DebugDayOffsetKey, Days);
}

int GetDebugDayOffset()
{
    // Read each time rather than cached: this changes while the app runs, and a stale copy would
    // leave half the app on one day and half on another.
    const QSettings Settings;

    return Settings.value(DebugDayOffsetKey, 0).toInt();
}
#endif

FTDayNumber DayNumberForMilliseconds(int64_t Milliseconds)
{
    constexpr int SecondsBeforeTheDayTurns = 2 * 60 * 60;

    const QDateTime Instant = QDateTime::fromMSecsSinceEpoch(Milliseconds);
    const QDate Day = Instant.addSecs(-SecondsBeforeTheDayTurns).date();

    return static_cast<FTDayNumber>(Day.toJulianDay() - QDate(1970, 1, 1).toJulianDay());
}

FTDayNumber GetTodayDayNumber()
{
    const FTDayNumber Real = DayNumberForMilliseconds(GetRawSystemTimeMilliseconds());

#ifdef QT_DEBUG
    return Real + GetDebugDayOffset();
#else
    return Real;
#endif
}
