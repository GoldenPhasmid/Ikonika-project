#include <catch2/catch_test_macros.hpp>

#include "AppWorkspaceMerger.h"

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTErasure.h"
#include "core/FTJournal.h"
#include "core/FTSchedule.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"
#include "core/FTUuid.h"

#include <QScopeGuard>
#include <QTemporaryDir>

#include <memory>
#include <string>
#include <vector>

namespace
{
    struct FCopy
    {
        FCopy()
        {
            std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
            REQUIRE(Opened.has_value());

            Database = std::make_unique<FTDatabase>(std::move(*Opened));
            REQUIRE(MigrateSchema(*Database));

            Repository = std::make_unique<FTCardRepository>(*Database, [this] { return Clock; });
            Repository->SetActiveLanguages("fr", "en");
        }

        std::unique_ptr<FTDatabase> Database;
        std::unique_ptr<FTCardRepository> Repository;
        int64_t Clock = 1000;
        QTemporaryDir Folder;
        FTSettings Settings{Folder.filePath("settings.ini")};
    };

    struct FSeeded
    {
        std::string CardId;
    };

    FSeeded SeedCard(FCopy& Copy)
    {
        FTCard Card;
        Card.TargetWord = "avoir";
        Card.Translation = "to have";

        const std::optional<std::string> CardId = Copy.Repository->AddCard(Card);
        REQUIRE(CardId.has_value());

        return FSeeded{*CardId};
    }

    void Merge(FCopy& Local, FCopy& Downloaded)
    {
        AppWorkspaceMerger Merger{Local.Settings};
        const AppMergeResult Result = Merger.Merge(*Local.Database, *Downloaded.Database);

        REQUIRE(Result.bSuccess);
    }

    FTJournalEvent InsertEvent(FCopy& Copy, const std::string& CardId, const char* EventId, FTDayNumber Day,
                               int EventValue = 0)
    {
        FTJournalEvent Event;
        Event.Id = MakeTimeOrderedUuid();
        Event.EventId = EventId;
        Event.CardId = CardId;
        Event.EventValue = EventValue;
        Event.Day = Day;

        FTStatement Insert = Copy.Database->Prepare(
            "INSERT INTO Journal (Id, EventId, CardId, EventValue, Day) VALUES (?1, ?2, ?3, ?4, ?5)");
        REQUIRE(Insert.IsValid());

        Insert.BindText(1, Event.Id);
        Insert.BindText(2, Event.EventId);
        Insert.BindText(3, Event.CardId);
        Insert.BindInt(4, Event.EventValue);
        Insert.BindInt(5, Event.Day);
        REQUIRE(Insert.Run());

        return Event;
    }

    int CountRows(FCopy& Copy, const std::string& Table)
    {
        FTStatement Query = Copy.Database->Prepare("SELECT COUNT(*) FROM " + Table);
        REQUIRE(Query.IsValid());
        REQUIRE(Query.Step());

        return Query.GetInt(0);
    }

    std::string ReadCardText(FCopy& Copy, const std::string& CardId, const std::string& Column)
    {
        FTStatement Query = Copy.Database->Prepare("SELECT " + Column + " FROM Cards WHERE Id = ?1");
        REQUIRE(Query.IsValid());

        Query.BindText(1, CardId);
        REQUIRE(Query.Step());

        return Query.GetText(0);
    }

    int64_t ReadCardNumber(FCopy& Copy, const std::string& CardId, const std::string& Column)
    {
        FTStatement Query = Copy.Database->Prepare("SELECT " + Column + " FROM Cards WHERE Id = ?1");
        REQUIRE(Query.IsValid());

        Query.BindText(1, CardId);
        REQUIRE(Query.Step());

        return Query.GetInt64(0);
    }

    FTCard ReadStoredProgress(FCopy& Copy, const std::string& CardId)
    {
        FTStatement Query = Copy.Database->Prepare(
            "SELECT LeitnerStage, DueDate, BoxChangedDate, CompletedDate FROM Cards WHERE Id = ?1");
        REQUIRE(Query.IsValid());

        Query.BindText(1, CardId);
        REQUIRE(Query.Step());

        FTCard Result;
        Result.LeitnerStage = Query.GetInt(0);

        if (!Query.IsNull(1))
        {
            Result.DueDate = Query.GetInt(1);
        }

        if (!Query.IsNull(2))
        {
            Result.BoxChangedDate = Query.GetInt(2);
        }

        if (!Query.IsNull(3))
        {
            Result.CompletedDate = Query.GetInt(3);
        }

        return Result;
    }

    void Edit(FCopy& Copy, const std::string& CardId, const std::string& Translation, int64_t At)
    {
        Copy.Clock = At;

        REQUIRE(Copy.Repository->UpdateCardContent(CardId, "avoir", Translation, std::nullopt, "verb", ""));
    }
}

TEST_CASE("A card only the other device has arrives in My Vocabulary", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Phone);

    Merge(Desktop, Phone);

    const std::vector<FTCardHandle> Cards = Desktop.Repository->LoadCards();
    REQUIRE(Cards.size() == 1);
    CHECK(Cards.front()->Id == Seeded.CardId);
    CHECK(Cards.front()->Translation == "to have");
}

TEST_CASE("A card arrives with the language pair it was added under", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    Phone.Repository->SetActiveLanguages("es", "en");
    SeedCard(Phone);

    Merge(Desktop, Phone);

    Desktop.Repository->SetActiveLanguages("fr", "en");
    CHECK(Desktop.Repository->LoadCards().empty());

    Desktop.Repository->SetActiveLanguages("es", "en");
    CHECK(Desktop.Repository->LoadCards().size() == 1);
}

TEST_CASE("Reviews made on two devices all survive the merge", "[merge]")
{
    const FTDayNumber Today = GetTodayDayNumber();

    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Phone);
    std::vector<FTJournalEvent> Expected{InsertEvent(Phone, Seeded.CardId, JournalLearned, Today - 10)};

    Merge(Desktop, Phone);

    Expected.push_back(InsertEvent(Phone, Seeded.CardId, JournalReviewed, Today - 9, 1));
    Expected.push_back(InsertEvent(Phone, Seeded.CardId, JournalReviewed, Today - 7, 1));
    Expected.push_back(InsertEvent(Phone, Seeded.CardId, JournalReviewed, Today - 3, 0));
    Expected.push_back(InsertEvent(Desktop, Seeded.CardId, JournalReviewed, Today - 5, 1));

    Merge(Phone, Desktop);

    CHECK(LoadJournal(*Phone.Database, Seeded.CardId).size() == 5);

    FTCard Replayed;
    ReplayJournal(Replayed, Expected, Today);

    const FTCard Stored = ReadStoredProgress(Phone, Seeded.CardId);
    CHECK(Stored.LeitnerStage == Replayed.LeitnerStage);
    CHECK(Stored.DueDate == Replayed.DueDate);
    CHECK(Stored.BoxChangedDate == Replayed.BoxChangedDate);
}

TEST_CASE("Both devices compute the same progress after merging each other", "[merge]")
{
    const FTDayNumber Today = GetTodayDayNumber();

    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Phone);
    InsertEvent(Phone, Seeded.CardId, JournalLearned, Today - 10);
    Merge(Desktop, Phone);

    InsertEvent(Phone, Seeded.CardId, JournalReviewed, Today - 9, 1);
    InsertEvent(Desktop, Seeded.CardId, JournalReviewed, Today - 6, 0);

    Merge(Phone, Desktop);
    Merge(Desktop, Phone);

    const FTCard OnPhone = ReadStoredProgress(Phone, Seeded.CardId);
    const FTCard OnDesktop = ReadStoredProgress(Desktop, Seeded.CardId);
    CHECK(OnPhone.LeitnerStage == OnDesktop.LeitnerStage);
    CHECK(OnPhone.DueDate == OnDesktop.DueDate);
    CHECK(OnPhone.BoxChangedDate == OnDesktop.BoxChangedDate);
}

TEST_CASE("Merging the same copy twice changes nothing", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Phone);
    InsertEvent(Phone, Seeded.CardId, JournalLearned, GetTodayDayNumber() - 2);

    Merge(Desktop, Phone);
    Merge(Desktop, Phone);

    CHECK(CountRows(Desktop, "Journal") == 1);
    CHECK(CountRows(Desktop, "Cards") == 1);
}

TEST_CASE("A card edited on both devices keeps the newer edit", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Phone);
    Merge(Desktop, Phone);

    SECTION("The other device edited it last")
    {
        Edit(Desktop, Seeded.CardId, "to possess", 2000);
        Edit(Phone, Seeded.CardId, "to own", 3000);

        Merge(Desktop, Phone);

        CHECK(ReadCardText(Desktop, Seeded.CardId, "Translation") == "to own");
        CHECK(ReadCardNumber(Desktop, Seeded.CardId, "LastUpdatedDate") == 3000);
    }

    SECTION("This device edited it last")
    {
        Edit(Phone, Seeded.CardId, "to own", 2000);
        Edit(Desktop, Seeded.CardId, "to possess", 3000);

        Merge(Desktop, Phone);

        CHECK(ReadCardText(Desktop, Seeded.CardId, "Translation") == "to possess");
        CHECK(ReadCardNumber(Desktop, Seeded.CardId, "LastUpdatedDate") == 3000);
    }
}

TEST_CASE("A deletion newer than the other device's edit removes the card", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Phone);
    Merge(Desktop, Phone);

    Edit(Desktop, Seeded.CardId, "to possess", 2000);

    Phone.Clock = 3000;
    REQUIRE(Phone.Repository->DeleteCards({Seeded.CardId}));

    Merge(Desktop, Phone);

    CHECK(ReadCardNumber(Desktop, Seeded.CardId, "Deleted") == 1);
    CHECK(Desktop.Repository->LoadCards().empty());
}

TEST_CASE("Progress columns from the other copy are ignored", "[merge]")
{
    const FTDayNumber Today = GetTodayDayNumber();

    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Phone);
    InsertEvent(Phone, Seeded.CardId, JournalLearned, Today);
    REQUIRE(Phone.Database->Execute("UPDATE Cards SET LeitnerStage = 3, DueDate = 1"));

    Merge(Desktop, Phone);

    const FTCard Stored = ReadStoredProgress(Desktop, Seeded.CardId);
    CHECK(Stored.LeitnerStage == FirstLadderStage);
    CHECK(Stored.DueDate == Today + GetIntervalDays(1));
}

TEST_CASE("A merge stamps no card with a new time", "[merge]")
{
    const FTDayNumber Today = GetTodayDayNumber();

    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Phone);
    InsertEvent(Phone, Seeded.CardId, JournalLearned, Today - 30);
    Merge(Desktop, Phone);

    Edit(Phone, Seeded.CardId, "to own", 2000);
    InsertEvent(Desktop, Seeded.CardId, JournalReviewed, Today - 29, 1);

    Merge(Desktop, Phone);
    Merge(Phone, Desktop);

    CHECK(ReadCardNumber(Desktop, Seeded.CardId, "LastUpdatedDate") == 2000);
    CHECK(ReadCardNumber(Phone, Seeded.CardId, "LastUpdatedDate") == 2000);
}

TEST_CASE("Daily stats from the other device are added and this device's rows are kept", "[merge]")
{
    const FTDayNumber Today = GetTodayDayNumber();

    FCopy Phone;
    FCopy Desktop;

    REQUIRE(Phone.Repository->RecordDailyProgress(Today, 5, 10, 25));
    REQUIRE(Desktop.Repository->RecordDailyProgress(Today, 3, 7, 25));

    Merge(Desktop, Phone);

    CHECK(CountRows(Desktop, "DailyStats") == 2);

    const std::vector<FTDailyStats> Days = Desktop.Repository->LoadDailyStats(Today, Today);
    REQUIRE(Days.size() == 1);
    CHECK(Days.front().LearnedCount == 8);
    CHECK(Days.front().ReviewedCount == 17);
}

TEST_CASE("This device's id never comes from the other copy", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    const std::string DesktopId = ReadLocalDeviceId(*Desktop.Database);

    Merge(Desktop, Phone);

    CHECK(ReadLocalDeviceId(*Desktop.Database) == DesktopId);
}

TEST_CASE("A placement keeps the newer result", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    Desktop.Clock = 2000;
    REQUIRE(Desktop.Repository->SetPlacementCutoff("fr", 100));

    Phone.Clock = 3000;
    REQUIRE(Phone.Repository->SetPlacementCutoff("fr", 500));

    Merge(Desktop, Phone);

    CHECK(Desktop.Repository->FindPlacementCutoff("fr") == 500);
}

TEST_CASE("A synced setting keeps the newer value", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    REQUIRE(Desktop.Database->Execute(
        "INSERT INTO SyncedSettings (Key, Value, LastUpdatedDate) VALUES ('DailyGoal', '20', 3000)"));
    REQUIRE(Phone.Database->Execute(
        "INSERT INTO SyncedSettings (Key, Value, LastUpdatedDate) VALUES ('DailyGoal', '40', 2000)"));
    REQUIRE(Phone.Database->Execute(
        "INSERT INTO SyncedSettings (Key, Value, LastUpdatedDate) VALUES ('Reminder', '09:00', 2000)"));

    Merge(Desktop, Phone);

    FTStatement Query = Desktop.Database->Prepare("SELECT Key, Value FROM SyncedSettings ORDER BY Key");
    REQUIRE(Query.Step());
    CHECK(Query.GetText(0) == "DailyGoal");
    CHECK(Query.GetText(1) == "20");
    REQUIRE(Query.Step());
    CHECK(Query.GetText(0) == "Reminder");
    CHECK(Query.GetText(1) == "09:00");
    CHECK_FALSE(Query.Step());
}

TEST_CASE("Words marked known on either device stay known", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    REQUIRE(Phone.Repository->MarkWordTriaged("maison", "noun", 1));
    REQUIRE(Desktop.Repository->MarkWordTriaged("chien", "noun", 0));

    Merge(Desktop, Phone);

    CHECK(Desktop.Repository->IsWordTriaged("maison", "noun", 1));
    CHECK(Desktop.Repository->IsWordTriaged("chien", "noun", 0));
}

TEST_CASE("Searches from both devices are kept", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    REQUIRE(Phone.Repository->RecordSearch("maison", "noun", 1));
    REQUIRE(Desktop.Repository->RecordSearch("chien", "noun", 2));

    Merge(Desktop, Phone);

    CHECK(Desktop.Repository->LoadRecentSearches(10).size() == 2);
}

TEST_CASE("A copy without a journal fails the merge and leaves this device untouched", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Phone);
    REQUIRE(Phone.Database->Execute("DROP TABLE Journal"));

    AppWorkspaceMerger Merger{Desktop.Settings};
    const AppMergeResult Result = Merger.Merge(*Desktop.Database, *Phone.Database);

    CHECK_FALSE(Result.bSuccess);
    CHECK_FALSE(Result.Error.isEmpty());
    CHECK(CountRows(Desktop, "Cards") == 0);
}

TEST_CASE("An erase on the other device removes this device's older data", "[merge][erase]")
{
    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Desktop);
    InsertEvent(Desktop, Seeded.CardId, JournalLearned, GetTodayDayNumber());

    Phone.Clock = 3000;
    REQUIRE(Phone.Repository->DeleteEverything());

    Merge(Desktop, Phone);

    CHECK(CountRows(Desktop, "Cards") == 0);
    CHECK(CountRows(Desktop, "Journal") == 0);
    CHECK(ReadErasedDate(*Desktop.Database) == 3000);
}

TEST_CASE("Data erased here does not come back from the other copy", "[merge][erase]")
{
    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Phone);
    InsertEvent(Phone, Seeded.CardId, JournalLearned, GetTodayDayNumber());
    Merge(Desktop, Phone);

    Desktop.Clock = 3000;
    REQUIRE(Desktop.Repository->DeleteEverything());

    Merge(Desktop, Phone);

    CHECK(CountRows(Desktop, "Cards") == 0);
    CHECK(CountRows(Desktop, "Journal") == 0);
    CHECK(ReadErasedDate(*Desktop.Database) == 3000);
}

TEST_CASE("A card edited after the erase survives it", "[merge][erase]")
{
    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Desktop);
    InsertEvent(Desktop, Seeded.CardId, JournalLearned, GetTodayDayNumber());
    Edit(Desktop, Seeded.CardId, "to own", 4000);

    Phone.Clock = 3000;
    REQUIRE(Phone.Repository->DeleteEverything());

    Merge(Desktop, Phone);

    CHECK(ReadCardText(Desktop, Seeded.CardId, "Translation") == "to own");
    CHECK(Desktop.Repository->LoadCards().size() == 1);
    CHECK(CountRows(Desktop, "Journal") == 1);
}

TEST_CASE("The later of two erases wins", "[merge][erase]")
{
    FCopy Phone;
    FCopy Desktop;

    Desktop.Clock = 2000;
    REQUIRE(Desktop.Repository->DeleteEverything());

    Phone.Clock = 3000;
    REQUIRE(Phone.Repository->DeleteEverything());

    Merge(Desktop, Phone);
    CHECK(ReadErasedDate(*Desktop.Database) == 3000);

    Merge(Phone, Desktop);
    CHECK(ReadErasedDate(*Phone.Database) == 3000);
}

TEST_CASE("Progress from a copy made before the journal survives the merge", "[merge]")
{
    const FTDayNumber Today = GetTodayDayNumber();

    FCopy Phone;
    FCopy Desktop;

    const FSeeded Old = SeedCard(Phone);
    FTCard OldCard = *Phone.Repository->LoadCard(Old.CardId).value();
    OldCard.LeitnerStage = 3;
    OldCard.BoxChangedDate = Today - 1;
    OldCard.DueDate = Today - 1 + GetIntervalDays(3);
    REQUIRE(Phone.Repository->UpdateCardSchedule(OldCard));
    REQUIRE(Phone.Database->Execute("PRAGMA user_version = 29"));
    REQUIRE(MigrateSchema(*Phone.Database));

    const FSeeded Mine = SeedCard(Desktop);
    FTCard MyCard = *Desktop.Repository->LoadCard(Mine.CardId).value();
    MyCard.LeitnerStage = 2;
    MyCard.BoxChangedDate = Today - 1;
    MyCard.DueDate = Today - 1 + GetIntervalDays(2);
    REQUIRE(Desktop.Repository->UpdateCardSchedule(MyCard));
    REQUIRE(Desktop.Database->Execute("PRAGMA user_version = 29"));
    REQUIRE(MigrateSchema(*Desktop.Database));

    Merge(Desktop, Phone);

    CHECK(ReadStoredProgress(Desktop, Old.CardId).LeitnerStage == 3);
    CHECK(ReadStoredProgress(Desktop, Mine.CardId).LeitnerStage == 2);
}

namespace
{
    void RecordSetting(FCopy& Copy, const std::string& Key, const QVariant& Value, int64_t At)
    {
        FTStatement Insert = Copy.Database->Prepare(
            "INSERT INTO SyncedSettings (Key, Value, LastUpdatedDate) VALUES (?1, ?2, ?3) "
            "ON CONFLICT (Key) DO UPDATE SET Value = excluded.Value, LastUpdatedDate = excluded.LastUpdatedDate");
        REQUIRE(Insert.IsValid());

        Insert.BindText(1, Key);
        Insert.BindText(2, SerializeSettingValue(Value));
        Insert.BindInt64(3, At);
        REQUIRE(Insert.Run());
    }
}

TEST_CASE("A setting changed later on the other device reaches this device's settings", "[merge][settings]")
{
    FCopy Phone;
    FCopy Desktop;

    Desktop.Settings.SetTrainingWordCount(20);
    RecordSetting(Desktop, "intensiveLearning/trainingWordCount", 20, 2000);
    RecordSetting(Phone, "intensiveLearning/trainingWordCount", 15, 3000);

    Merge(Desktop, Phone);

    CHECK(Desktop.Settings.GetIntensiveLearningRules().TrainingWordCount == 15);
}

TEST_CASE("A setting changed later here stays", "[merge][settings]")
{
    FCopy Phone;
    FCopy Desktop;

    Desktop.Settings.SetTrainingWordCount(20);
    RecordSetting(Desktop, "intensiveLearning/trainingWordCount", 20, 3000);
    RecordSetting(Phone, "intensiveLearning/trainingWordCount", 15, 2000);

    Merge(Desktop, Phone);

    CHECK(Desktop.Settings.GetIntensiveLearningRules().TrainingWordCount == 20);
}

TEST_CASE("A merge that fails after reading the settings leaves the settings file as it was", "[merge][settings]")
{
    FCopy Phone;
    FCopy Desktop;
    SeedCard(Desktop);

    Desktop.Settings.SetTrainingWordCount(20);
    RecordSetting(Desktop, "intensiveLearning/trainingWordCount", 20, 2000);
    RecordSetting(Phone, "intensiveLearning/trainingWordCount", 15, 3000);
    FTStatement Block = Desktop.Database->Prepare(
        "CREATE TRIGGER BlockProgress BEFORE UPDATE ON Cards BEGIN SELECT RAISE(ABORT, 'blocked'); END");
    REQUIRE(Block.Run());

    AppWorkspaceMerger Merger{Desktop.Settings};

    CHECK_FALSE(Merger.Merge(*Desktop.Database, *Phone.Database).bSuccess);
    CHECK(Desktop.Settings.GetIntensiveLearningRules().TrainingWordCount == 20);
}

TEST_CASE("A failed merge removes a setting it brought that this device never had", "[merge][settings]")
{
    FCopy Phone;
    FCopy Desktop;
    SeedCard(Desktop);

    const int Default{Desktop.Settings.GetIntensiveLearningRules().TrainingWordCount};
    RecordSetting(Phone, "intensiveLearning/trainingWordCount", Default + 5, 3000);

    FTStatement Block = Desktop.Database->Prepare(
        "CREATE TRIGGER BlockProgress BEFORE UPDATE ON Cards BEGIN SELECT RAISE(ABORT, 'blocked'); END");
    REQUIRE(Block.Run());

    AppWorkspaceMerger Merger{Desktop.Settings};

    CHECK_FALSE(Merger.Merge(*Desktop.Database, *Phone.Database).bSuccess);
    CHECK(Desktop.Settings.GetIntensiveLearningRules().TrainingWordCount == Default);
    CHECK(Desktop.Settings.ReadSyncedValues().empty());
}

TEST_CASE("A language deleted on the other device takes its triaged words off this device", "[merge][erase]")
{
    FCopy Phone;
    FCopy Desktop;

    Desktop.Clock = 2000;
    REQUIRE(Desktop.Repository->MarkWordTriaged("maison", "noun", 1));
    Phone.Clock = 3000;
    REQUIRE(Phone.Repository->DeleteLanguage("fr"));

    Merge(Desktop, Phone);

    CHECK_FALSE(Desktop.Repository->IsWordTriaged("maison", "noun", 1));
}

TEST_CASE("A language deleted on the other device erases this device's older cards of it", "[merge][erase]")
{
    FCopy Phone;
    FCopy Desktop;

    Desktop.Clock = 2000;
    const FSeeded Old{SeedCard(Desktop)};
    Phone.Clock = 3000;
    REQUIRE(Phone.Repository->DeleteLanguage("fr"));

    Merge(Desktop, Phone);

    CHECK(CountRows(Desktop, "Cards") == 0);
}

TEST_CASE("A card added after its language was deleted on the other device survives the merge", "[merge][erase]")
{
    FCopy Phone;
    FCopy Desktop;

    Phone.Clock = 3000;
    REQUIRE(Phone.Repository->DeleteLanguage("fr"));
    Desktop.Clock = 4000;
    const FSeeded Added{SeedCard(Desktop)};

    Merge(Desktop, Phone);
    Merge(Phone, Desktop);

    CHECK(ReadCardText(Desktop, Added.CardId, "TargetWord") == "avoir");
    CHECK(ReadCardText(Phone, Added.CardId, "TargetWord") == "avoir");
}

TEST_CASE("Triaged words of a deleted language do not come back from the other copy", "[merge][erase]")
{
    FCopy Phone;
    FCopy Desktop;

    Phone.Clock = 2000;
    REQUIRE(Phone.Repository->MarkWordTriaged("maison", "noun", 1));
    Desktop.Clock = 3000;
    REQUIRE(Desktop.Repository->MarkWordTriaged("maison", "noun", 1));
    REQUIRE(Desktop.Repository->DeleteLanguage("fr"));

    Merge(Desktop, Phone);

    CHECK_FALSE(Desktop.Repository->IsWordTriaged("maison", "noun", 1));
}

TEST_CASE("An erase on the other device keeps this device's settings", "[merge][settings][erase]")
{
    FCopy Phone;
    FCopy Desktop;

    RecordSetting(Desktop, "intensiveLearning/trainingWordCount", 20, 2000);
    Phone.Clock = 3000;
    REQUIRE(Phone.Repository->DeleteEverything());

    Merge(Desktop, Phone);

    CHECK(CountRows(Desktop, "SyncedSettings") == 1);
}

TEST_CASE("A card a stale copy dropped from every deck stays live on the device that kept it", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;
    const FSeeded Kept{SeedCard(Desktop)};

    REQUIRE(Phone.Database->Execute(
        "CREATE TABLE Decks (Id TEXT PRIMARY KEY NOT NULL, Name TEXT NOT NULL DEFAULT '', "
        "CreatedDate INTEGER NOT NULL DEFAULT 0, LastUpdatedDate INTEGER NOT NULL DEFAULT 0, "
        "Deleted INTEGER NOT NULL DEFAULT 0, TargetLanguage TEXT NOT NULL DEFAULT '', "
        "NativeLanguage TEXT NOT NULL DEFAULT '', Role TEXT NOT NULL DEFAULT '')"));
    REQUIRE(Phone.Database->Execute(
        "CREATE TABLE DeckCards (DeckId TEXT NOT NULL, CardId TEXT NOT NULL, "
        "LastUpdatedDate INTEGER NOT NULL DEFAULT 0, Deleted INTEGER NOT NULL DEFAULT 0)"));

    FTStatement Copy = Phone.Database->Prepare(
        "INSERT INTO Cards (Id, TargetWord, Translation, CreatedDate, LastUpdatedDate, TargetLanguage, NativeLanguage) "
        "VALUES (?1, 'avoir', 'to have', ?2, ?2, 'fr', 'en')");
    REQUIRE(Copy.IsValid());
    Copy.BindText(1, Kept.CardId);
    Copy.BindInt64(2, ReadCardNumber(Desktop, Kept.CardId, "LastUpdatedDate"));
    REQUIRE(Copy.Run());

    REQUIRE(Phone.Database->Execute("PRAGMA user_version = 31"));
    REQUIRE(MigrateSchema(*Phone.Database));

    Merge(Desktop, Phone);

    CHECK(ReadCardNumber(Desktop, Kept.CardId, "Deleted") == 0);
}

TEST_CASE("Progress is rebuilt under the schedule rules the merge brings", "[merge][settings]")
{
    FCopy Phone;
    FCopy Desktop;
    const FSeeded Seeded{SeedCard(Desktop)};

    FTSettings::SetActive(&Desktop.Settings);
    const auto Restore{qScopeGuard([] { FTSettings::SetActive(nullptr); })};

    Desktop.Settings.SetMissedReviewDemotes(true);
    RecordSetting(Desktop, "schedule/missedReviewDemotes", true, 2000);
    RecordSetting(Phone, "schedule/missedReviewDemotes", false, 3000);

    const FTDayNumber Learned{GetTodayDayNumber() - 300};
    const FTDayNumber FirstReview{Learned + GetIntervalDays(1)};
    const FTDayNumber SecondReview{FirstReview + GetIntervalDays(2)};
    InsertEvent(Desktop, Seeded.CardId, JournalLearned, Learned);
    InsertEvent(Desktop, Seeded.CardId, JournalReviewed, FirstReview, 1);
    InsertEvent(Desktop, Seeded.CardId, JournalReviewed, SecondReview, 1);

    Merge(Desktop, Phone);

    CHECK_FALSE(Desktop.Settings.GetScheduleRules().bMissedReviewDemotes);
    CHECK(ReadStoredProgress(Desktop, Seeded.CardId).LeitnerStage == 3);
}

TEST_CASE("Two settings changed at the same time end up the same on both devices", "[merge][settings]")
{
    FCopy Phone;
    FCopy Desktop;

    RecordSetting(Desktop, "intensiveLearning/trainingWordCount", 20, 0);
    RecordSetting(Phone, "intensiveLearning/trainingWordCount", 15, 0);

    Merge(Desktop, Phone);
    Merge(Phone, Desktop);

    CHECK(Desktop.Settings.GetIntensiveLearningRules().TrainingWordCount
          == Phone.Settings.GetIntensiveLearningRules().TrainingWordCount);
}

TEST_CASE("Two card edits made at the same time end up the same on both devices", "[merge]")
{
    FCopy Phone;
    FCopy Desktop;

    const FSeeded Seeded = SeedCard(Phone);
    Merge(Desktop, Phone);

    Edit(Desktop, Seeded.CardId, "to possess", 3000);
    Edit(Phone, Seeded.CardId, "to own", 3000);

    Merge(Desktop, Phone);
    Merge(Phone, Desktop);

    CHECK(ReadCardText(Desktop, Seeded.CardId, "Translation") == ReadCardText(Phone, Seeded.CardId, "Translation"));
}

TEST_CASE("A merge with no chosen language adopts the language of the cards studied last", "[merge][settings]")
{
    FCopy Phone;
    FCopy Desktop;

    Phone.Clock = 2000;
    SeedCard(Phone);
    Phone.Repository->SetActiveLanguages("es", "uk");
    Phone.Clock = 3000;
    SeedCard(Phone);

    Merge(Desktop, Phone);

    CHECK(Desktop.Settings.HasChosenLanguage());
    CHECK(Desktop.Settings.GetLanguageRules().TargetLanguage == "es");
    CHECK(Desktop.Settings.GetLanguageRules().NativeLanguage == "uk");
}

TEST_CASE("A merged language wins over the language of the cards", "[merge][settings]")
{
    FCopy Phone;
    FCopy Desktop;

    Phone.Repository->SetActiveLanguages("es", "en");
    Phone.Clock = 3000;
    SeedCard(Phone);
    RecordSetting(Phone, "languages/target", "fr", 2000);
    RecordSetting(Phone, "languages/native", "en", 2000);

    Merge(Desktop, Phone);

    CHECK(Desktop.Settings.GetLanguageRules().TargetLanguage == "fr");
}

TEST_CASE("A language chosen on this device is kept when the merge brings none", "[merge][settings]")
{
    FCopy Phone;
    FCopy Desktop;

    Desktop.Settings.AdoptLanguagePair(FTLanguageRules{"pt", "en"});
    Phone.Repository->SetActiveLanguages("es", "en");
    SeedCard(Phone);

    Merge(Desktop, Phone);

    CHECK(Desktop.Settings.GetLanguageRules().TargetLanguage == "pt");
}

TEST_CASE("Workspace merge carries the newest card hint", "[workspace-merge][hint]")
{
    FCopy Local;
    FCopy Remote;
    FTCard Card;
    Card.TargetWord = "chat";
    Card.Translation = "cat";
    Card.Hint = "Le chat dort.";
    const auto Id = Remote.Repository->AddCard(Card);
    REQUIRE(Id.has_value());
    Merge(Local, Remote);
    const auto Loaded = Local.Repository->LoadCard(*Id);
    REQUIRE(Loaded.has_value());
    CHECK((*Loaded)->Hint == "Le chat dort.");
    Remote.Clock = 2000;
    REQUIRE(Remote.Repository->UpdateCardContent(*Id, "chat", "cat", {}, "noun", "", "Le chat joue."));
    Merge(Local, Remote);
    const auto Updated = Local.Repository->LoadCard(*Id);
    REQUIRE(Updated.has_value());
    CHECK((*Updated)->Hint == "Le chat joue.");
}
