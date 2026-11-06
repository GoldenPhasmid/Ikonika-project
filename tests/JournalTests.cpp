#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTJournal.h"
#include "core/FTSchedule.h"
#include "core/FTScheduleManager.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"
#include "core/FTUuid.h"

#include <algorithm>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr FTDayNumber Today = 20000;
    constexpr FTDayNumber Learned = 19900;

    struct RulesGuard
    {
        ~RulesGuard()
        {
            const FTScheduleRules Defaults;
            FTSettings::Get().SetIntervalDays(QList<int>(Defaults.IntervalDays.begin(), Defaults.IntervalDays.end()));
            FTSettings::Get().SetMaximumDaysInFirstStage(Defaults.MaximumDaysInFirstStage);
            FTSettings::Get().SetFirstStageFailureReturnsToPool(Defaults.bFirstStageFailureReturnsToPool);
            FTSettings::Get().SetMissedReviewDemotes(Defaults.bMissedReviewDemotes);
        }
    };

    FTCard MakeCard()
    {
        FTCard Result;
        Result.Id = "card";
        Result.TargetWord = "avoir";
        Result.Translation = "to have";

        return Result;
    }

    FTJournalEvent MakeEvent(const std::string& Id, const char* EventId, FTDayNumber Day, int EventValue = 0)
    {
        FTJournalEvent Result;
        Result.Id = Id;
        Result.EventId = EventId;
        Result.CardId = "card";
        Result.EventValue = EventValue;
        Result.Day = Day;

        return Result;
    }

    FTCard Replay(const std::vector<FTJournalEvent>& Events, FTDayNumber On)
    {
        FTCard Result = MakeCard();
        ReplayJournal(Result, Events, On);

        return Result;
    }

    struct Fixture
    {
        Fixture()
        {
            std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
            REQUIRE(Opened.has_value());

            Target = std::make_unique<FTDatabase>(std::move(*Opened));
            REQUIRE(MigrateSchema(*Target));

            Repository = std::make_unique<FTCardRepository>(*Target, [this] { return Clock; });
            Manager = std::make_unique<FTScheduleManager>(*Repository);
        }

        std::unique_ptr<FTDatabase> Target;
        std::unique_ptr<FTCardRepository> Repository;
        std::unique_ptr<FTScheduleManager> Manager;
        int64_t Clock = 1000;
    };

    std::string SeedCard(Fixture& Test)
    {
        FTCard Card;
        Card.TargetWord = "avoir";
        Card.Translation = "to have";

        const std::optional<std::string> CardId = Test.Repository->AddCard(Card);
        REQUIRE(CardId.has_value());

        return *CardId;
    }

    void InsertEvent(Fixture& Test, const std::string& CardId, const char* EventId, FTDayNumber Day,
                     int EventValue = 0)
    {
        FTStatement Insert = Test.Target->Prepare(
            "INSERT INTO Journal (Id, EventId, CardId, EventValue, Day) VALUES (?1, ?2, ?3, ?4, ?5)");
        REQUIRE(Insert.IsValid());

        Insert.BindText(1, MakeTimeOrderedUuid());
        Insert.BindText(2, EventId);
        Insert.BindText(3, CardId);
        Insert.BindInt(4, EventValue);
        Insert.BindInt(5, Day);
        REQUIRE(Insert.Run());
    }

    FTCard ReadStoredProgress(Fixture& Test, const std::string& CardId)
    {
        FTStatement Query = Test.Target->Prepare(
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

    int64_t ReadLastUpdated(Fixture& Test, const std::string& CardId)
    {
        FTStatement Query = Test.Target->Prepare("SELECT LastUpdatedDate FROM Cards WHERE Id = ?1");
        REQUIRE(Query.IsValid());

        Query.BindText(1, CardId);
        REQUIRE(Query.Step());

        return Query.GetInt64(0);
    }

    FTCard LoadCard(Fixture& Test, const std::string& CardId)
    {
        const std::optional<FTCardHandle> Loaded = Test.Repository->LoadCard(CardId);
        REQUIRE(Loaded.has_value());

        return **Loaded;
    }

    void CheckSameProgress(const FTCard& Actual, const FTCard& Expected)
    {
        CHECK(Actual.LeitnerStage == Expected.LeitnerStage);
        CHECK(Actual.DueDate == Expected.DueDate);
        CHECK(Actual.BoxChangedDate == Expected.BoxChangedDate);
        CHECK(Actual.CompletedDate == Expected.CompletedDate);
    }
}

TEST_CASE("A card with no events sits in the pool", "[journal]")
{
    FTCard Card = MakeCard();
    Card.LeitnerStage = 3;
    Card.DueDate = Today;

    ReplayJournal(Card, {}, Today);

    CHECK(IsInPool(Card));
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("Learning a card puts it in the first stage, due after the first interval", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned)}, Learned);

    CHECK(Card.LeitnerStage == FirstLadderStage);
    CHECK(Card.BoxChangedDate == Learned);
    CHECK(Card.DueDate == Learned + GetIntervalDays(1));
}

TEST_CASE("Remembering a card on its due day moves it up one stage", "[journal]")
{
    RulesGuard Guard;

    const FTDayNumber Reviewed = Learned + GetIntervalDays(1);

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                MakeEvent("event-2", JournalReviewed, Reviewed, 1)},
                               Reviewed);

    CHECK(Card.LeitnerStage == 2);
    CHECK(Card.DueDate == Reviewed + GetIntervalDays(2));
}

TEST_CASE("Forgetting a card moves it down one stage", "[journal]")
{
    RulesGuard Guard;

    const FTDayNumber FirstReview = Learned + GetIntervalDays(1);
    const FTDayNumber SecondReview = FirstReview + GetIntervalDays(2);

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                MakeEvent("event-2", JournalReviewed, FirstReview, 1),
                                MakeEvent("event-3", JournalReviewed, SecondReview, 0)},
                               SecondReview);

    CHECK(Card.LeitnerStage == 1);
    CHECK(Card.DueDate == SecondReview + GetIntervalDays(1));
}

TEST_CASE("A missed date demotes the card once and keeps it due", "[journal]")
{
    RulesGuard Guard;

    const FTDayNumber FirstReview = Learned + GetIntervalDays(1);
    const FTDayNumber SecondReview = FirstReview + GetIntervalDays(2);
    const FTDayNumber Missed = SecondReview + GetIntervalDays(3);

    const std::vector<FTJournalEvent> Events{MakeEvent("event-1", JournalLearned, Learned),
                                             MakeEvent("event-2", JournalReviewed, FirstReview, 1),
                                             MakeEvent("event-3", JournalReviewed, SecondReview, 1)};

    const FTCard DayAfter = Replay(Events, Missed + 1);
    CHECK(DayAfter.LeitnerStage == 2);
    CHECK(DayAfter.DueDate == Missed + 1);

    const FTCard MonthAfter = Replay(Events, Missed + 30);
    CHECK(MonthAfter.LeitnerStage == 2);
    CHECK(MonthAfter.DueDate == Missed + 1);
}

TEST_CASE("A missed date demotes nothing when the setting is off", "[journal]")
{
    RulesGuard Guard;
    FTSettings::Get().SetMissedReviewDemotes(false);

    const FTDayNumber FirstReview = Learned + GetIntervalDays(1);
    const FTDayNumber SecondReview = FirstReview + GetIntervalDays(2);

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                MakeEvent("event-2", JournalReviewed, FirstReview, 1),
                                MakeEvent("event-3", JournalReviewed, SecondReview, 1)},
                               SecondReview + 30);

    CHECK(Card.LeitnerStage == 3);
}

TEST_CASE("A card demoted into the first stage and then left alone goes back to the pool", "[journal]")
{
    RulesGuard Guard;
    FTSettings::Get().SetMaximumDaysInFirstStage(3);
    FTSettings::Get().SetFirstStageFailureReturnsToPool(false);

    const FTDayNumber FirstReview = Learned + GetIntervalDays(1);
    const FTDayNumber Demoted = FirstReview + GetIntervalDays(2) + 1;

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                MakeEvent("event-2", JournalReviewed, FirstReview, 1)},
                               Demoted + 4);

    CHECK(IsInPool(Card));
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("Events replay in day order whatever order they arrive in", "[journal]")
{
    RulesGuard Guard;

    const FTDayNumber FirstReview = Learned + GetIntervalDays(1);
    const FTDayNumber SecondReview = FirstReview + GetIntervalDays(2);

    std::vector<FTJournalEvent> Events{MakeEvent("event-1", JournalLearned, Learned),
                                       MakeEvent("event-2", JournalReviewed, FirstReview, 1),
                                       MakeEvent("event-3", JournalReviewed, SecondReview, 0)};

    const FTCard InOrder = Replay(Events, SecondReview);

    std::reverse(Events.begin(), Events.end());

    CheckSameProgress(Replay(Events, SecondReview), InOrder);
}

TEST_CASE("Two answers on the same day replay in the order their ids were made", "[journal]")
{
    RulesGuard Guard;
    FTSettings::Get().SetFirstStageFailureReturnsToPool(false);

    const FTDayNumber Reviewed = Learned + GetIntervalDays(1);

    const FTCard ForgotThenRemembered = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                                MakeEvent("event-2", JournalReviewed, Reviewed, 0),
                                                MakeEvent("event-3", JournalReviewed, Reviewed, 1)},
                                               Reviewed);

    const FTCard RememberedThenForgot = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                                MakeEvent("event-3", JournalReviewed, Reviewed, 0),
                                                MakeEvent("event-2", JournalReviewed, Reviewed, 1)},
                                               Reviewed);

    CHECK(ForgotThenRemembered.LeitnerStage == 2);
    CHECK(RememberedThenForgot.LeitnerStage == 1);
}

TEST_CASE("Sending a card back to the pool is replayed", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                MakeEvent("event-2", JournalPooled, Learned + 1)},
                               Learned + 1);

    CHECK(IsInPool(Card));
    CHECK(Card.BoxChangedDate == Learned + 1);
}

TEST_CASE("Marking a card learned is replayed as completed", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalCompleted, Learned)}, Today);

    CHECK(IsCompleted(Card));
    CHECK(Card.CompletedDate == Learned);
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("Replaying leaves the card's content and last-updated time alone", "[journal]")
{
    RulesGuard Guard;

    FTCard Card = MakeCard();
    Card.LastUpdatedDate = 1234;

    ReplayJournal(Card, {MakeEvent("event-1", JournalLearned, Learned)}, Today);

    CHECK(Card.TargetWord == "avoir");
    CHECK(Card.Translation == "to have");
    CHECK(Card.LastUpdatedDate == 1234);
}

TEST_CASE("Event ids sort in the order they were made", "[journal][uuid]")
{
    std::vector<std::string> Made;

    for (int Index = 0; Index < 50; ++Index)
    {
        Made.push_back(MakeTimeOrderedUuid());
    }

    CHECK(std::is_sorted(Made.begin(), Made.end()));
    CHECK(std::adjacent_find(Made.begin(), Made.end()) == Made.end());
}

TEST_CASE("An event id is a valid version 7 UUID", "[journal][uuid]")
{
    const std::string Id = MakeTimeOrderedUuid();

    CHECK(IsValidUuid(Id));
    CHECK(Id[14] == '7');
}

TEST_CASE("Answering a review appends one reviewed event for that day", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 1);
    REQUIRE(RebuildCardProgress(*Test.Target, Today - 1));

    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today));

    const std::vector<FTJournalEvent> Journal = LoadJournal(*Test.Target, CardId);
    REQUIRE(Journal.size() == 2);
    CHECK(Journal.back().EventId == JournalReviewed);
    CHECK(Journal.back().EventValue == 1);
    CHECK(Journal.back().Day == Today);
    CHECK(Journal.back().CardId == CardId);
    CHECK(IsValidUuid(Journal.back().Id));
}

TEST_CASE("Answering a review leaves the card's last-updated time alone", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 1);
    REQUIRE(RebuildCardProgress(*Test.Target, Today - 1));

    const int64_t Before = ReadLastUpdated(Test, CardId);
    Test.Clock = 5000;

    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), false, Today));

    CHECK(ReadLastUpdated(Test, CardId) == Before);
}

TEST_CASE("Finishing a training appends a learned event and leaves last-updated alone", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    const int64_t Before = ReadLastUpdated(Test, CardId);
    Test.Clock = 5000;

    REQUIRE(Test.Repository->PromoteCards({CardId}));

    const std::vector<FTJournalEvent> Journal = LoadJournal(*Test.Target, CardId);
    REQUIRE(Journal.size() == 1);
    CHECK(Journal.front().EventId == JournalLearned);
    CHECK(Journal.front().Day == GetTodayDayNumber());
    CHECK(ReadStoredProgress(Test, CardId).LeitnerStage == FirstLadderStage);
    CHECK(ReadLastUpdated(Test, CardId) == Before);
}

TEST_CASE("Sending a card back to the pool appends a pooled event", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    REQUIRE(Test.Repository->PromoteCards({CardId}));

    REQUIRE(Test.Repository->ReturnCardToPool(CardId));

    const std::vector<FTJournalEvent> Journal = LoadJournal(*Test.Target, CardId);
    REQUIRE(Journal.size() == 2);
    CHECK(Journal.back().EventId == JournalPooled);
    CHECK(IsInPool(ReadStoredProgress(Test, CardId)));
}

TEST_CASE("Marking a card learned appends a completed event", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);

    REQUIRE(Test.Repository->MarkCardCompleted(CardId));

    const std::vector<FTJournalEvent> Journal = LoadJournal(*Test.Target, CardId);
    REQUIRE(Journal.size() == 1);
    CHECK(Journal.front().EventId == JournalCompleted);
    CHECK(IsCompleted(ReadStoredProgress(Test, CardId)));
}

TEST_CASE("A missed date writes nothing to the journal and leaves last-updated alone", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 20);
    InsertEvent(Test, CardId, JournalReviewed, Today - 19, 1);
    REQUIRE(RebuildCardProgress(*Test.Target, Today - 19));

    const int64_t Before = ReadLastUpdated(Test, CardId);
    Test.Clock = 5000;

    Test.Manager->LoadDueCardsForToday(Today);

    CHECK(LoadJournal(*Test.Target, CardId).size() == 2);
    CHECK(ReadLastUpdated(Test, CardId) == Before);
}

TEST_CASE("Stored progress always matches a replay of the journal", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 9);
    REQUIRE(RebuildCardProgress(*Test.Target, Today - 9));

    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today - 8));
    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), false, Today - 3));
    Test.Manager->LoadDueCardsForToday(Today);

    FTCard Expected = MakeCard();
    ReplayJournal(Expected, LoadJournal(*Test.Target, CardId), Today);

    CheckSameProgress(ReadStoredProgress(Test, CardId), Expected);
}

TEST_CASE("Rebuilding progress ignores what the progress columns said", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today);
    REQUIRE(Test.Target->Execute("UPDATE Cards SET LeitnerStage = 3, DueDate = 1, CompletedDate = 1"));

    REQUIRE(RebuildCardProgress(*Test.Target, Today));

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(Stored.LeitnerStage == FirstLadderStage);
    CHECK(Stored.DueDate == Today + GetIntervalDays(1));
    CHECK_FALSE(Stored.CompletedDate.has_value());
}

TEST_CASE("Rebuilding puts a card with no events in the pool", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    REQUIRE(Test.Target->Execute("UPDATE Cards SET LeitnerStage = 2, DueDate = 1"));

    REQUIRE(RebuildCardProgress(*Test.Target, Today));

    CHECK(IsInPool(ReadStoredProgress(Test, CardId)));
}

TEST_CASE("Rebuilding progress leaves last-updated alone", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today);
    const int64_t Before = ReadLastUpdated(Test, CardId);

    REQUIRE(RebuildCardProgress(*Test.Target, Today));

    CHECK(ReadLastUpdated(Test, CardId) == Before);
}

TEST_CASE("Editing a card's translation stamps its last-updated time", "[journal]")
{
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    Test.Clock = 5000;

    REQUIRE(Test.Repository->UpdateCardContent(CardId, "avoir", "to own", std::nullopt, "verb", ""));

    CHECK(ReadLastUpdated(Test, CardId) == 5000);
}

namespace
{
    std::string SeedCardInStage(Fixture& Test, int Stage, std::optional<FTDayNumber> BoxChanged,
                                std::optional<FTDayNumber> Due, std::optional<FTDayNumber> Completed = std::nullopt)
    {
        const std::string CardId = SeedCard(Test);

        FTCard Card = LoadCard(Test, CardId);
        Card.LeitnerStage = Stage;
        Card.BoxChangedDate = BoxChanged;
        Card.DueDate = Due;
        Card.CompletedDate = Completed;

        REQUIRE(Test.Repository->UpdateCardSchedule(Card));

        return CardId;
    }
}

TEST_CASE("A starting event places the card in its stage on its day", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalPlaced, Learned, 3)}, Learned);

    CHECK(Card.LeitnerStage == 3);
    CHECK(Card.BoxChangedDate == Learned);
    CHECK(Card.DueDate == Learned + GetIntervalDays(3));
}

TEST_CASE("A review after a starting event moves on from the placed stage", "[journal]")
{
    RulesGuard Guard;

    const FTDayNumber Reviewed = Learned + GetIntervalDays(2);

    const FTCard Card = Replay({MakeEvent("event-1", JournalPlaced, Learned, 2),
                                MakeEvent("event-2", JournalReviewed, Reviewed, 0)},
                               Reviewed);

    CHECK(Card.LeitnerStage == 1);
}

TEST_CASE("Seeding starting events keeps a ladder card's progress through a rebuild", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCardInStage(Test, 2, Today - 1, Today - 1 + GetIntervalDays(2));

    REQUIRE(SeedStartingEvents(*Test.Target));
    REQUIRE(RebuildCardProgress(*Test.Target, Today));

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(Stored.LeitnerStage == 2);
    CHECK(Stored.BoxChangedDate == Today - 1);
    CHECK(Stored.DueDate == Today - 1 + GetIntervalDays(2));
}

TEST_CASE("Seeding starting events keeps a completed card completed", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCardInStage(Test, GetLastStage(), Today - 5, std::nullopt, Today - 5);

    REQUIRE(SeedStartingEvents(*Test.Target));
    REQUIRE(RebuildCardProgress(*Test.Target, Today));

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(IsCompleted(Stored));
    CHECK(Stored.CompletedDate == Today - 5);
}

TEST_CASE("Seeding starting events places a card with no box date so its due date holds", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCardInStage(Test, 2, std::nullopt, Today + 1);

    REQUIRE(SeedStartingEvents(*Test.Target));
    REQUIRE(RebuildCardProgress(*Test.Target, Today));

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(Stored.LeitnerStage == 2);
    CHECK(Stored.DueDate == Today + 1);
}

TEST_CASE("Seeding starting events leaves a pool card without events", "[journal]")
{
    Fixture Test;

    const std::string CardId = SeedCard(Test);

    REQUIRE(SeedStartingEvents(*Test.Target));

    CHECK(LoadJournal(*Test.Target, CardId).empty());
}

TEST_CASE("Seeding starting events skips a card that already has events", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCardInStage(Test, 2, Today - 1, Today + 1);
    InsertEvent(Test, CardId, JournalLearned, Today - 3);

    REQUIRE(SeedStartingEvents(*Test.Target));

    CHECK(LoadJournal(*Test.Target, CardId).size() == 1);
}

TEST_CASE("Seeding starting events twice adds nothing", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCardInStage(Test, 2, Today - 1, Today + 1);

    REQUIRE(SeedStartingEvents(*Test.Target));
    REQUIRE(SeedStartingEvents(*Test.Target));

    CHECK(LoadJournal(*Test.Target, CardId).size() == 1);
}

TEST_CASE("Two copies with the same progress seed the same starting event", "[journal]")
{
    RulesGuard Guard;
    Fixture First;
    Fixture Second;

    const std::string CardId = SeedCardInStage(First, 2, Today - 1, Today + 1);
    REQUIRE(Second.Target->Execute("INSERT INTO Cards (Id, TargetWord, Translation, LeitnerStage, DueDate, "
                                   "BoxChangedDate, CreatedDate, LastUpdatedDate) "
                                   "VALUES ('" + CardId + "', 'avoir', 'to have', 2, "
                                   + std::to_string(Today + 1) + ", " + std::to_string(Today - 1) + ", 1000, 1000)"));

    REQUIRE(SeedStartingEvents(*First.Target));
    REQUIRE(SeedStartingEvents(*Second.Target));

    const std::vector<FTJournalEvent> FirstJournal = LoadJournal(*First.Target, CardId);
    const std::vector<FTJournalEvent> SecondJournal = LoadJournal(*Second.Target, CardId);
    REQUIRE(FirstJournal.size() == 1);
    REQUIRE(SecondJournal.size() == 1);
    CHECK(FirstJournal.front().Id == SecondJournal.front().Id);
}

TEST_CASE("Migrating from the version before starting events keeps every card's progress", "[journal][schema]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCardInStage(Test, 3, Today - 2, Today - 2 + GetIntervalDays(3));
    REQUIRE(Test.Target->Execute("PRAGMA user_version = 29"));

    REQUIRE(MigrateSchema(*Test.Target));
    REQUIRE(RebuildCardProgress(*Test.Target, Today));

    CHECK(ReadSchemaVersion(*Test.Target) == CurrentSchemaVersion);
    CHECK(ReadStoredProgress(Test, CardId).LeitnerStage == 3);
}

TEST_CASE("A recorded due date wins over the current intervals", "[journal]")
{
    RulesGuard Guard;

    FTJournalEvent First = MakeEvent("event-1", JournalLearned, Learned);
    First.DueDate = Learned + 1;

    FTJournalEvent Second = MakeEvent("event-2", JournalReviewed, Learned + 1, 1);
    Second.DueDate = Learned + 3;

    FTSettings::Get().SetIntervalDays({5, 5, 5});

    const FTCard Card = Replay({First, Second}, Learned + 1);

    CHECK(Card.LeitnerStage == 2);
    CHECK(Card.DueDate == Learned + 3);
}

TEST_CASE("Answering records the due date it produced", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 1);
    REQUIRE(RebuildCardProgress(*Test.Target, Today - 1));

    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today));

    const std::vector<FTJournalEvent> Journal = LoadJournal(*Test.Target, CardId);
    REQUIRE(Journal.back().DueDate.has_value());
    CHECK(*Journal.back().DueDate == Today + GetIntervalDays(2));
}

TEST_CASE("Changing an interval leaves scheduled cards' due dates through a rebuild", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 2);
    REQUIRE(RebuildCardProgress(*Test.Target, Today - 2));
    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today - 1));

    const std::optional<FTDayNumber> Before = ReadStoredProgress(Test, CardId).DueDate;

    FTSettings::Get().SetIntervalDays({1, 1, 1});
    REQUIRE(RebuildCardProgress(*Test.Target, Today - 1));

    CHECK(ReadStoredProgress(Test, CardId).DueDate == Before);
}

TEST_CASE("A starting event keeps a punished card's due date", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCardInStage(Test, 2, Today - 3, Today - 3);

    REQUIRE(SeedStartingEvents(*Test.Target));
    REQUIRE(RebuildCardProgress(*Test.Target, Today));

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(Stored.LeitnerStage == 2);
    CHECK(Stored.DueDate == Today - 3);
}

TEST_CASE("Stored progress matches a replay over a long run of random days", "[journal][slow]")
{
    RulesGuard Guard;

    const bool bFirstStageFailureReturnsToPool = GENERATE(false, true);
    const unsigned int Seed = GENERATE(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u);

    FTSettings::Get().SetFirstStageFailureReturnsToPool(bFirstStageFailureReturnsToPool);
    FTSettings::Get().SetMaximumDaysInFirstStage(3);

    Fixture Test;
    std::mt19937 Random{Seed};
    std::uniform_int_distribution<int> Percent{0, 99};

    std::vector<std::string> CardIds;

    for (int Index = 0; Index < 12; ++Index)
    {
        CardIds.push_back(SeedCard(Test));
    }

    for (FTDayNumber Day = Today; Day < Today + 120; ++Day)
    {
        if (Percent(Random) < 30)
        {
            continue;
        }

        for (const FTCardHandle& Due : Test.Manager->LoadDueCardsForToday(Day))
        {
            const int Roll = Percent(Random);

            if (Roll < 10)
            {
                continue;
            }

            REQUIRE(Test.Manager->ReportReviewAnswer(*Due, Roll >= 40, Day));
        }

        for (const std::string& CardId : CardIds)
        {
            const int Roll = Percent(Random);

            if (IsInPool(ReadStoredProgress(Test, CardId)) && Roll < 30)
            {
                REQUIRE(Test.Repository->RecordProgressEvent(CardId, JournalLearned, 0, Day));
            }
            else if (Roll >= 97)
            {
                REQUIRE(Test.Repository->RecordProgressEvent(CardId, JournalPooled, 0, Day));
            }
            else if (Roll == 96)
            {
                REQUIRE(Test.Repository->RecordProgressEvent(CardId, JournalCompleted, 0, Day));
            }
        }

        for (const std::string& CardId : CardIds)
        {
            FTCard Expected = MakeCard();
            ReplayJournal(Expected, LoadJournal(*Test.Target, CardId), Day);

            INFO("seed " << Seed << ", day " << Day - Today << ", card " << CardId);
            CheckSameProgress(ReadStoredProgress(Test, CardId), Expected);
        }
    }
}
