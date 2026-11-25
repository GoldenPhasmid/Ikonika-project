#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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
            FTSettings::Get().SetCustomIntensity(Defaults.Custom);
            FTSettings::Get().SetLearningIntensity(Defaults.IntensityIndex);
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

    FTCard Replay(const std::vector<FTJournalEvent>& Events)
    {
        FTCard Result = MakeCard();
        ReplayJournal(Result, Events);

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
            "SELECT LeitnerStage, DueDate, BoxChangedDate, CompletedDate, Stability, Difficulty "
            "FROM Cards WHERE Id = ?1");
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

        if (!Query.IsNull(4))
        {
            Result.Stability = Query.GetDouble(4);
        }

        if (!Query.IsNull(5))
        {
            Result.Difficulty = Query.GetDouble(5);
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
        CHECK(Actual.Stability.has_value() == Expected.Stability.has_value());
        CHECK(Actual.Difficulty.has_value() == Expected.Difficulty.has_value());

        if (Actual.Stability.has_value() && Expected.Stability.has_value())
        {
            CHECK_THAT(*Actual.Stability, Catch::Matchers::WithinAbs(*Expected.Stability, 1e-9));
        }

        if (Actual.Difficulty.has_value() && Expected.Difficulty.has_value())
        {
            CHECK_THAT(*Actual.Difficulty, Catch::Matchers::WithinAbs(*Expected.Difficulty, 1e-9));
        }
    }
}

TEST_CASE("A card with no events sits in the pool", "[journal]")
{
    FTCard Card = MakeCard();
    Card.LeitnerStage = 3;
    Card.DueDate = Today;
    Card.Stability = 10.0;

    ReplayJournal(Card, {});

    CHECK(IsInPool(Card));
    CHECK_FALSE(Card.DueDate.has_value());
    CHECK_FALSE(Card.Stability.has_value());
}

TEST_CASE("Learning a card puts it in the first stage, due two days later", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned)});

    CHECK(Card.LeitnerStage == FirstLadderStage);
    CHECK(Card.BoxChangedDate == Learned);
    CHECK(Card.DueDate == Learned + 2);
    CHECK(Card.Stability.has_value());
}

TEST_CASE("Remembering a card on its due day pushes the next review out", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                MakeEvent("event-2", JournalReviewed, Learned + 2, 1)});

    CHECK(Card.LeitnerStage == 2);
    CHECK(Card.DueDate == Learned + 8);
}

TEST_CASE("Forgetting a card brings it back soon", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                MakeEvent("event-2", JournalReviewed, Learned + 2, 1),
                                MakeEvent("event-3", JournalReviewed, Learned + 8, 0)});

    CHECK(Card.LeitnerStage == FirstLadderStage);
    CHECK(*Card.DueDate - (Learned + 8) <= 3);
}

TEST_CASE("A missed date changes nothing in the replay", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                MakeEvent("event-2", JournalReviewed, Learned + 2, 1)});

    CHECK(Card.LeitnerStage == 2);
    CHECK(Card.DueDate == Learned + 8);
}

TEST_CASE("Events replay in day order whatever order they arrive in", "[journal]")
{
    RulesGuard Guard;

    std::vector<FTJournalEvent> Events{MakeEvent("event-1", JournalLearned, Learned),
                                       MakeEvent("event-2", JournalReviewed, Learned + 3, 1),
                                       MakeEvent("event-3", JournalReviewed, Learned + 14, 0)};

    const FTCard InOrder = Replay(Events);

    std::reverse(Events.begin(), Events.end());

    CheckSameProgress(Replay(Events), InOrder);
}

TEST_CASE("Two answers on the same day replay in the order their ids were made", "[journal]")
{
    RulesGuard Guard;

    const FTCard ForgotThenRemembered = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                                MakeEvent("event-2", JournalReviewed, Learned + 3, 0),
                                                MakeEvent("event-3", JournalReviewed, Learned + 3, 1)});

    const FTCard RememberedThenForgot = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                                MakeEvent("event-3", JournalReviewed, Learned + 3, 0),
                                                MakeEvent("event-2", JournalReviewed, Learned + 3, 1)});

    CHECK(*ForgotThenRemembered.Stability != *RememberedThenForgot.Stability);
}

TEST_CASE("Sending a card back to the pool is replayed", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                MakeEvent("event-2", JournalPooled, Learned + 1)});

    CHECK(IsInPool(Card));
    CHECK(Card.BoxChangedDate == Learned + 1);
    CHECK_FALSE(Card.Stability.has_value());
}

TEST_CASE("Learning a card again after a reset starts it fresh", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalLearned, Learned),
                                MakeEvent("event-2", JournalReviewed, Learned + 3, 1),
                                MakeEvent("event-3", JournalPooled, Learned + 5),
                                MakeEvent("event-4", JournalLearned, Learned + 6)});

    CHECK(Card.LeitnerStage == FirstLadderStage);
    CHECK(Card.DueDate == Learned + 8);
}

TEST_CASE("Learning a card again starts it fresh when its return to the pool was never recorded", "[journal]")
{
    RulesGuard Guard;

    const FTCard Fresh{Replay({MakeEvent("event-1", JournalLearned, Learned + 33)})};
    const FTCard AfterPlaced{Replay({MakeEvent("event-1", JournalPlaced, Learned, 1),
                                     MakeEvent("event-2", JournalLearned, Learned + 33)})};
    const FTCard AfterForgotten{Replay({MakeEvent("event-1", JournalLearned, Learned),
                                        MakeEvent("event-2", JournalReviewed, Learned + 1, 0),
                                        MakeEvent("event-3", JournalLearned, Learned + 33)})};

    CheckSameProgress(AfterPlaced, Fresh);
    CheckSameProgress(AfterForgotten, Fresh);
}

TEST_CASE("Marking a card learned is replayed as completed", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalCompleted, Learned)});

    CHECK(IsCompleted(Card));
    CHECK(Card.CompletedDate == Learned);
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("Replaying leaves the card's content and last-updated time alone", "[journal]")
{
    RulesGuard Guard;

    FTCard Card = MakeCard();
    Card.LastUpdatedDate = 1234;

    ReplayJournal(Card, {MakeEvent("event-1", JournalLearned, Learned)});

    CHECK(Card.TargetWord == "avoir");
    CHECK(Card.Translation == "to have");
    CHECK(Card.LastUpdatedDate == 1234);
}

TEST_CASE("A recorded stability and due date win over what the replay works out", "[journal]")
{
    RulesGuard Guard;

    FTJournalEvent First = MakeEvent("event-1", JournalLearned, Learned);
    First.DueDate = Learned + 5;
    First.Stability = 40.0;
    First.Difficulty = 6.0;

    const FTCard Card = Replay({First});

    CHECK(Card.DueDate == Learned + 5);
    CHECK(Card.Stability == 40.0);
    CHECK(Card.Difficulty == 6.0);
    CHECK(Card.LeitnerStage == GetProgressStage(40.0));
}

TEST_CASE("The next answer grows from the recorded stability", "[journal]")
{
    RulesGuard Guard;

    FTJournalEvent First = MakeEvent("event-1", JournalLearned, Learned);
    First.Stability = 40.0;
    First.Difficulty = 6.0;

    const FTCard Card = Replay({First, MakeEvent("event-2", JournalReviewed, Learned + 40, 1)});

    CHECK(*Card.Stability > 40.0);
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

TEST_CASE("Answering a review appends one reviewed event with the stability it produced", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 1);
    REQUIRE(RebuildCardProgress(*Test.Target));

    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today));

    const std::vector<FTJournalEvent> Journal = LoadJournal(*Test.Target, CardId);
    REQUIRE(Journal.size() == 2);
    CHECK(Journal.back().EventId == JournalReviewed);
    CHECK(Journal.back().EventValue == 1);
    CHECK(Journal.back().Day == Today);
    CHECK(Journal.back().CardId == CardId);
    CHECK(IsValidUuid(Journal.back().Id));
    CHECK(Journal.back().DueDate == Today + 4);
    REQUIRE(Journal.back().Stability.has_value());
    CHECK(Journal.back().Stability == ReadStoredProgress(Test, CardId).Stability);
    CHECK(Journal.back().Difficulty == ReadStoredProgress(Test, CardId).Difficulty);
}

TEST_CASE("Answering a review leaves the card's last-updated time alone", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 1);
    REQUIRE(RebuildCardProgress(*Test.Target));

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
    CHECK(ReadStoredProgress(Test, CardId).Stability.has_value());
    CHECK(ReadLastUpdated(Test, CardId) == Before);
}

TEST_CASE("Sending a card back to the pool appends a pooled event and clears stability", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    REQUIRE(Test.Repository->PromoteCards({CardId}));

    REQUIRE(Test.Repository->ReturnCardToPool(CardId));

    const std::vector<FTJournalEvent> Journal = LoadJournal(*Test.Target, CardId);
    REQUIRE(Journal.size() == 2);
    CHECK(Journal.back().EventId == JournalPooled);

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(IsInPool(Stored));
    CHECK_FALSE(Stored.Stability.has_value());
    CHECK_FALSE(Stored.Difficulty.has_value());
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

TEST_CASE("Loading due cards long after their date writes nothing and moves nothing", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 20);
    REQUIRE(RebuildCardProgress(*Test.Target));

    const FTCard Before = ReadStoredProgress(Test, CardId);
    const int64_t Updated = ReadLastUpdated(Test, CardId);
    Test.Clock = 5000;

    CHECK(Test.Manager->LoadDueCardsForToday(Today).size() == 1);

    CHECK(LoadJournal(*Test.Target, CardId).size() == 1);
    CHECK(ReadLastUpdated(Test, CardId) == Updated);
    CheckSameProgress(ReadStoredProgress(Test, CardId), Before);
}

TEST_CASE("Stored progress always matches a replay of the journal", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 9);
    REQUIRE(RebuildCardProgress(*Test.Target));

    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today - 8));
    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), false, Today - 3));

    FTCard Expected = MakeCard();
    ReplayJournal(Expected, LoadJournal(*Test.Target, CardId));

    CheckSameProgress(ReadStoredProgress(Test, CardId), Expected);
}

TEST_CASE("Rebuilding progress ignores what the progress columns said", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today);
    REQUIRE(Test.Target->Execute(
        "UPDATE Cards SET LeitnerStage = 3, DueDate = 1, CompletedDate = 1, Stability = 99, Difficulty = 1"));

    REQUIRE(RebuildCardProgress(*Test.Target));

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(Stored.LeitnerStage == FirstLadderStage);
    CHECK(Stored.DueDate == Today + 2);
    CHECK_FALSE(Stored.CompletedDate.has_value());
    CHECK(*Stored.Stability < 99.0);
}

TEST_CASE("Rebuilding puts a card with no events in the pool", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    REQUIRE(Test.Target->Execute("UPDATE Cards SET LeitnerStage = 2, DueDate = 1, Stability = 5"));

    REQUIRE(RebuildCardProgress(*Test.Target));

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(IsInPool(Stored));
    CHECK_FALSE(Stored.Stability.has_value());
}

TEST_CASE("Rebuilding progress leaves last-updated alone", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today);
    const int64_t Before = ReadLastUpdated(Test, CardId);

    REQUIRE(RebuildCardProgress(*Test.Target));

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

TEST_CASE("Filling the journal gives every old event the stability it left its card with", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 20);
    InsertEvent(Test, CardId, JournalReviewed, Today - 17, 1);

    REQUIRE(FillJournalStability(*Test.Target));

    const std::vector<FTJournalEvent> Journal = LoadJournal(*Test.Target, CardId);
    REQUIRE(Journal.size() == 2);
    REQUIRE(Journal.front().Stability.has_value());
    REQUIRE(Journal.back().Stability.has_value());
    CHECK_THAT(*Journal.front().Stability, Catch::Matchers::WithinAbs(3.173, 0.001));
    CHECK(*Journal.back().Stability > *Journal.front().Stability);
}

TEST_CASE("Filling the journal leaves a pool event and a recorded stability alone", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 20);
    InsertEvent(Test, CardId, JournalPooled, Today - 10);
    REQUIRE(Test.Target->Execute("UPDATE Journal SET Stability = 7, Difficulty = 4 WHERE EventId = 'learned'"));

    REQUIRE(FillJournalStability(*Test.Target));

    const std::vector<FTJournalEvent> Journal = LoadJournal(*Test.Target, CardId);
    REQUIRE(Journal.size() == 2);
    CHECK(Journal.front().Stability == 7.0);
    CHECK_FALSE(Journal.back().Stability.has_value());
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

TEST_CASE("A starting event gives the card its old box gap as stability", "[journal]")
{
    RulesGuard Guard;

    FTSettings::Get().SetLearningIntensity(0);

    const FTCard Card = Replay({MakeEvent("event-1", JournalPlaced, Learned, 4)});

    CHECK(Card.Stability == 14.0);
    CHECK(Card.BoxChangedDate == Learned);
    CHECK(Card.DueDate == Learned + 14);
}

TEST_CASE("A review after a starting event moves on from the placed stability", "[journal]")
{
    RulesGuard Guard;

    const FTCard Card = Replay({MakeEvent("event-1", JournalPlaced, Learned, 4),
                                MakeEvent("event-2", JournalReviewed, Learned + 14, 1)});

    CHECK(*Card.Stability > 14.0);
}

TEST_CASE("Seeding starting events keeps a ladder card's due date through a rebuild", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCardInStage(Test, 2, Today - 1, Today + 1);

    REQUIRE(SeedStartingEvents(*Test.Target));
    REQUIRE(RebuildCardProgress(*Test.Target));

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(Stored.BoxChangedDate == Today - 1);
    CHECK(Stored.DueDate == Today + 1);
    CHECK(Stored.Stability == 2.0);
}

TEST_CASE("Seeding starting events keeps a completed card completed", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCardInStage(Test, 6, Today - 5, std::nullopt, Today - 5);

    REQUIRE(SeedStartingEvents(*Test.Target));
    REQUIRE(RebuildCardProgress(*Test.Target));

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
    REQUIRE(RebuildCardProgress(*Test.Target));

    CHECK(ReadStoredProgress(Test, CardId).DueDate == Today + 1);
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

TEST_CASE("Migrating from the version before starting events keeps every card's due date", "[journal][schema]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCardInStage(Test, 3, Today - 2, Today + 2);
    REQUIRE(Test.Target->Execute("PRAGMA user_version = 29"));

    REQUIRE(MigrateSchema(*Test.Target));

    CHECK(ReadSchemaVersion(*Test.Target) == CurrentSchemaVersion);

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(Stored.DueDate == Today + 2);
    CHECK(Stored.Stability == 4.0);
}

TEST_CASE("Migrating to stability keeps due dates and fills the journal", "[journal][schema]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 10);
    REQUIRE(Test.Target->Execute("UPDATE Journal SET DueDate = " + std::to_string(Today + 4)));
    REQUIRE(Test.Target->Execute("UPDATE Journal SET Stability = NULL, Difficulty = NULL"));
    REQUIRE(Test.Target->Execute("UPDATE Cards SET Stability = NULL, Difficulty = NULL"));
    REQUIRE(Test.Target->Execute("PRAGMA user_version = 39"));

    REQUIRE(MigrateSchema(*Test.Target));

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(Stored.DueDate == Today + 4);
    CHECK(Stored.Stability.has_value());
    CHECK(LoadJournal(*Test.Target, CardId).front().Stability.has_value());
}

TEST_CASE("A recorded due date survives a change of the recall target through a rebuild", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    REQUIRE(Test.Repository->RecordProgressEvent(CardId, JournalLearned, 0, Today - 3));
    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today));

    const std::optional<FTDayNumber> Before = ReadStoredProgress(Test, CardId).DueDate;

    FTSettings::Get().SetLearningIntensity(0);
    REQUIRE(RebuildCardProgress(*Test.Target));

    CHECK(ReadStoredProgress(Test, CardId).DueDate == Before);
}

TEST_CASE("A word learned at one intensity stays learned when the intensity is raised", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    FTSettings::Get().SetLearningIntensity(0);

    const std::string CardId = SeedCard(Test);
    REQUIRE(Test.Repository->RecordProgressEvent(CardId, JournalLearned, 0, Today - 55));
    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today - 52));
    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today - 41));
    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today - 6));
    REQUIRE(IsCompleted(ReadStoredProgress(Test, CardId)));

    FTSettings::Get().SetLearningIntensity(2);
    REQUIRE(RebuildCardProgress(*Test.Target));

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK(IsCompleted(Stored));
    CHECK(Stored.CompletedDate == Today - 6);
    CHECK_FALSE(Stored.DueDate.has_value());
}

TEST_CASE("A word still on the schedule stays there when the intensity is lowered", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    REQUIRE(Test.Repository->RecordProgressEvent(CardId, JournalLearned, 0, Today - 64));
    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today - 62));
    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today - 56));
    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today - 41));
    REQUIRE(Test.Manager->ReportReviewAnswer(LoadCard(Test, CardId), true, Today - 6));

    const FTCard Before = ReadStoredProgress(Test, CardId);
    REQUIRE_FALSE(IsCompleted(Before));

    FTSettings::Get().SetLearningIntensity(0);
    REQUIRE(RebuildCardProgress(*Test.Target));

    const FTCard Stored = ReadStoredProgress(Test, CardId);
    CHECK_FALSE(IsCompleted(Stored));
    CHECK(Stored.DueDate == Before.DueDate);
}

TEST_CASE("Filling the journal gives an old event with no due date the one it produced", "[journal]")
{
    RulesGuard Guard;
    Fixture Test;

    const std::string CardId = SeedCard(Test);
    InsertEvent(Test, CardId, JournalLearned, Today - 20);

    REQUIRE(FillJournalStability(*Test.Target));

    CHECK(LoadJournal(*Test.Target, CardId).front().DueDate == Today - 18);
}

TEST_CASE("Stored progress matches a replay over a long run of random days", "[journal][slow]")
{
    RulesGuard Guard;

    const unsigned int Seed = GENERATE(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u);

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
            ReplayJournal(Expected, LoadJournal(*Test.Target, CardId));

            INFO("seed " << Seed << ", day " << Day - Today << ", card " << CardId);
            CheckSameProgress(ReadStoredProgress(Test, CardId), Expected);
        }
    }
}
