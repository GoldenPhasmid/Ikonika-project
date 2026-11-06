#include <catch2/catch_test_macros.hpp>

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTSchedule.h"
#include "core/FTScheduleManager.h"
#include "core/FTSchema.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
    constexpr FTDayNumber Today = 20000;

    /** Restores the default ladder on destruction, since FTSettings is a process-wide singleton. */
    struct RulesGuard
    {
        ~RulesGuard()
        {
            const FTScheduleRules Defaults;
            FTSettings::Get().SetIntervalDays(QList<int>(Defaults.IntervalDays.begin(), Defaults.IntervalDays.end()));
            FTSettings::Get().SetMaximumDaysInFirstStage(Defaults.MaximumDaysInFirstStage);
            FTSettings::Get().SetFirstStageFailureReturnsToPool(Defaults.bFirstStageFailureReturnsToPool);
        }
    };

    struct Fixture
    {
        Fixture()
        {
            std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
            REQUIRE(Opened.has_value());

            Target = std::make_unique<FTDatabase>(std::move(*Opened));
            REQUIRE(MigrateSchema(*Target));

            Repository = std::make_unique<FTCardRepository>(*Target);
            Manager = std::make_unique<FTScheduleManager>(*Repository);
        }

        std::unique_ptr<FTDatabase> Target;
        std::unique_ptr<FTCardRepository> Repository;
        std::unique_ptr<FTScheduleManager> Manager;
    };

    FTCard SeedCardDueOn(Fixture& Test, int Stage, FTDayNumber EnteredOn, FTDayNumber DueOn)
    {
        FTCard Card;
        Card.TargetWord = "le chien";
        Card.Translation = "dog";

        const std::optional<std::string> CardId = Test.Repository->AddCard(Card);
        REQUIRE(CardId.has_value());

        FTCard Stored = **Test.Repository->LoadCard(*CardId);
        Stored.LeitnerStage = Stage;
        Stored.BoxChangedDate = EnteredOn;
        Stored.DueDate = DueOn;

        REQUIRE(Test.Repository->UpdateCardSchedule(Stored));

        return Stored;
    }
}

TEST_CASE("A card merely due today is returned unchanged", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    SeedCardDueOn(Test, 3, Today - 4, Today);

    const std::vector<FTCardHandle> Due = Test.Manager->LoadDueCardsForToday(Today);

    REQUIRE(Due.size() == 1);
    CHECK(Due[0]->LeitnerStage == 3);
}

TEST_CASE("A card overdue past the expiry cutoff is demoted before it is handed back",
         "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    // Stage 3's interval is 4 days; due Today - 1 means it has been owed since then, well past
    // any single missed day, so ApplyExpiry demotes it once.
    SeedCardDueOn(Test, 3, Today - 5, Today - 1);

    const std::vector<FTCardHandle> Due = Test.Manager->LoadDueCardsForToday(Today);

    REQUIRE(Due.size() == 1);
    CHECK(Due[0]->LeitnerStage == 2);
    CHECK(Due[0]->DueDate == Today);
}

TEST_CASE("The demotion from expiry is actually persisted, not just returned once",
         "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 3, Today - 5, Today - 1);

    Test.Manager->LoadDueCardsForToday(Today);

    const std::vector<FTCardHandle> Reloaded = Test.Repository->LoadCards();

    REQUIRE(Reloaded.size() == 1);
    CHECK(Reloaded[0]->LeitnerStage == 2);
    (void)Seeded;
}

TEST_CASE("A card expired all the way back to the pool is not returned as due",
         "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    // More than three days in stage 1 sends it to the pool outright, per FTScheduleRules.
    SeedCardDueOn(Test, FirstLadderStage, Today - 10, Today - 9);

    const std::vector<FTCardHandle> Due = Test.Manager->LoadDueCardsForToday(Today);

    CHECK(Due.empty());
}

TEST_CASE("Loading today's due cards leaves them due today", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    SeedCardDueOn(Test, 3, Today - 5, Today - 1);
    SeedCardDueOn(Test, 4, Today - 30, Today - 20);
    SeedCardDueOn(Test, 2, Today - 9, Today - 4);

    REQUIRE(Test.Manager->LoadDueCardsForToday(Today).size() == 3);

    // Loading is not answering. A second load finds the same three, which is what the review
    // screen and the Today count both do after a session ends.
    CHECK(Test.Manager->LoadDueCardsForToday(Today).size() == 3);
    CHECK(Test.Repository->CountDueCards(Today) == 3);
}

TEST_CASE("A review of one batch leaves the rest of the day's cards due", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Answered = SeedCardDueOn(Test, 3, Today - 5, Today - 1);
    SeedCardDueOn(Test, 3, Today - 5, Today - 1);
    SeedCardDueOn(Test, 3, Today - 5, Today - 1);

    REQUIRE(Test.Manager->LoadDueCardsForToday(Today).size() == 3);

    FTRepetitionOutcome Outcome;
    Outcome.CardId = Answered.Id;
    Outcome.bPromote = true;

    CHECK(Test.Manager->ReportSessionOutcomes({Outcome}, Today) == 1);

    CHECK(Test.Repository->CountDueCards(Today) == 2);
}

TEST_CASE("A card not yet due is not returned", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    SeedCardDueOn(Test, 2, Today - 2, Today + 1);

    CHECK(Test.Manager->LoadDueCardsForToday(Today).empty());
}

TEST_CASE("Reporting a remembered answer promotes the card and saves it", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 2, Today - 2, Today);

    CHECK(Test.Manager->ReportReviewAnswer(Seeded, true, Today));

    const std::vector<FTCardHandle> Reloaded = Test.Repository->LoadCards();

    REQUIRE(Reloaded.size() == 1);
    CHECK(Reloaded[0]->LeitnerStage == 3);
}

TEST_CASE("Reporting a not-remembered answer demotes the card and saves it", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 3, Today - 4, Today);

    CHECK(Test.Manager->ReportReviewAnswer(Seeded, false, Today));

    const std::vector<FTCardHandle> Reloaded = Test.Repository->LoadCards();

    REQUIRE(Reloaded.size() == 1);
    CHECK(Reloaded[0]->LeitnerStage == 2);
}

TEST_CASE("Reporting an answer for an unknown card fails without crashing", "[schedulemanager]")
{
    Fixture Test;

    FTCard Ghost;
    Ghost.Id = "does-not-exist";
    Ghost.TargetWord = "le fantôme";

    CHECK_FALSE(Test.Manager->ReportReviewAnswer(Ghost, true, Today));
}

/*
 * The tests above each check one day in isolation. These walk a card forward over many days, which
 * is where the schedule actually lives: a card is only useful if it comes back on the day the
 * ladder says and stays away in between.
 */

namespace
{
    FTCard ReloadOnlyCard(Fixture& Test)
    {
        const std::vector<FTCardHandle> Cards = Test.Repository->LoadCards();
        REQUIRE(Cards.size() == 1);

        return *Cards.at(0);
    }

    /** Days the card is handed back as due, walking one day at a time from Today. */
    std::vector<FTDayNumber> CollectDueDays(Fixture& Test, int DayCount, bool bAnswerRemembered)
    {
        std::vector<FTDayNumber> DueOn;

        for (int Offset = 0; Offset < DayCount; ++Offset)
        {
            const FTDayNumber Day = Today + Offset;

            const std::vector<FTCardHandle> Due = Test.Manager->LoadDueCardsForToday(Day);

            if (Due.empty())
            {
                continue;
            }

            DueOn.push_back(Day);

            // Answering is what moves it on; without this it would be due every day from now on.
            Test.Manager->ReportReviewAnswer(*Due.at(0), bAnswerRemembered, Day);
        }

        return DueOn;
    }
}

TEST_CASE("A remembered card comes back on each ladder interval in turn", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    // The shipped ladder, taken from the defaults rather than repeated here, so this test fails if
    // the intervals change without anyone deciding what the new schedule should look like.
    const FTScheduleRules Defaults;
    FTSettings::Get().SetIntervalDays(
        QList<int>(Defaults.IntervalDays.begin(), Defaults.IntervalDays.end()));

    SeedCardDueOn(Test, 1, Today, Today + 1);

    const std::vector<FTDayNumber> DueOn = CollectDueDays(Test, 200, true);

    REQUIRE(DueOn.size() == 6);
    CHECK(DueOn[0] == Today + 1);
    CHECK(DueOn[1] == Today + 3);
    CHECK(DueOn[2] == Today + 7);
    CHECK(DueOn[3] == Today + 21);
    CHECK(DueOn[4] == Today + 61);
    CHECK(DueOn[5] == Today + 181);
}

TEST_CASE("A card is not due on any day between its intervals", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    FTSettings::Get().SetIntervalDays({1, 2, 4, 8, 16});

    SeedCardDueOn(Test, 2, Today, Today + 2);

    // Silent until its day, which is the whole point of a schedule.
    CHECK(Test.Manager->LoadDueCardsForToday(Today).empty());
    CHECK(Test.Manager->LoadDueCardsForToday(Today + 1).empty());
    CHECK_FALSE(Test.Manager->LoadDueCardsForToday(Today + 2).empty());
}

TEST_CASE("A card answered wrongly drops a box and is scheduled on that box's own interval",
          "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    FTSettings::Get().SetIntervalDays({1, 2, 4, 8, 16});

    SeedCardDueOn(Test, 3, Today, Today);

    const std::vector<FTCardHandle> Due = Test.Manager->LoadDueCardsForToday(Today);
    REQUIRE(Due.size() == 1);

    REQUIRE(Test.Manager->ReportReviewAnswer(*Due.at(0), false, Today));

    const FTCard After = ReloadOnlyCard(Test);
    REQUIRE(After.LeitnerStage == 2);
    REQUIRE(After.DueDate.has_value());

    // Box 2's own interval from today, the same as a promotion into box 2 would get: a demoted
    // card is not owed again until its new box says so, not on the same day it just failed.
    CHECK(*After.DueDate == Today + GetIntervalDays(2));
    CHECK(Test.Manager->LoadDueCardsForToday(Today).empty());
}

TEST_CASE("A card climbing the whole ladder ends up completed and stops coming back",
          "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    // A short ladder, so the walk reaches the end without needing hundreds of days.
    FTSettings::Get().SetIntervalDays({1, 2});

    SeedCardDueOn(Test, 1, Today, Today + 1);

    const std::vector<FTDayNumber> DueOn = CollectDueDays(Test, 30, true);

    // Two boxes, so two appearances, and then it is done rather than repeating forever.
    CHECK(DueOn.size() == 2);

    const FTCard Finished = ReloadOnlyCard(Test);
    CHECK(IsCompleted(Finished));
    CHECK_FALSE(Finished.DueDate.has_value());

    // Completed cards are out of the schedule, not merely far in the future.
    CHECK(Test.Manager->LoadDueCardsForToday(Today + 1000).empty());
}

TEST_CASE("A card answered wrongly every time never leaves the bottom of the ladder",
          "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    FTSettings::Get().SetIntervalDays({1, 2, 4});

    // Expiry off, so this measures demotion rather than the overdue rule.
    FTSettings::Get().SetMaximumDaysInFirstStage(9999);

    SeedCardDueOn(Test, 3, Today, Today);

    const std::vector<FTDayNumber> DueOn = CollectDueDays(Test, 30, false);

    // It keeps coming back, which is the point: a word answered wrongly is never finished with.
    CHECK(DueOn.size() > 3);

    const FTCard After = ReloadOnlyCard(Test);
    CHECK_FALSE(IsCompleted(After));
    CHECK(After.LeitnerStage <= 1);
}

TEST_CASE("Today's due cards exclude what is not on the ladder", "[schedulemanager]")
{
    Fixture Test;

    FTCard Unlearned;
    Unlearned.TargetWord = "le chien";
    Unlearned.Translation = "dog";

    FTCard Due;
    Due.TargetWord = "la maison";
    Due.Translation = "house";

    FTCard Later;
    Later.TargetWord = "le café";
    Later.Translation = "coffee";

    REQUIRE(Test.Repository->AddCard(Unlearned).has_value());
    REQUIRE(Test.Repository->AddCard(Due).has_value());
    REQUIRE(Test.Repository->AddCard(Later).has_value());

    const std::vector<FTCardHandle> Stored = Test.Repository->LoadCards();
    REQUIRE(Stored.size() == 3);

    FTCard DueToday = *Stored[1];
    DueToday.LeitnerStage = 2;
    DueToday.BoxChangedDate = Today;
    DueToday.DueDate = Today;
    REQUIRE(Test.Repository->UpdateCardSchedule(DueToday));

    FTCard DueLater = *Stored[2];
    DueLater.LeitnerStage = 2;
    DueLater.BoxChangedDate = Today;
    DueLater.DueDate = Today + 5;
    REQUIRE(Test.Repository->UpdateCardSchedule(DueLater));

    const std::vector<FTCardHandle> Loaded = Test.Manager->LoadDueCardsForToday(Today);

    REQUIRE(Loaded.size() == 1);
    CHECK(Loaded[0]->TargetWord == "la maison");
}

TEST_CASE("A long-missed card is expired before it is handed back", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 3, Today - 30, Today - 20);

    const std::vector<FTCardHandle> Loaded = Test.Manager->LoadDueCardsForToday(Today);

    REQUIRE(Loaded.size() == 1);

    // Twenty days late costs one box, and the card is owed a review today rather than being
    // handed a date days out that nobody asked for.
    CHECK(Loaded[0]->LeitnerStage == 2);
    CHECK(Loaded[0]->DueDate.has_value());
    CHECK(*Loaded[0]->DueDate <= Today);

    const FTCard Reloaded = *Test.Repository->LoadCard(Seeded.Id).value();
    CHECK(Reloaded.LeitnerStage == 2);
}

TEST_CASE("Nothing due loads nothing", "[schedulemanager]")
{
    Fixture Test;

    SeedCardDueOn(Test, 2, Today, Today + 3);

    CHECK(Test.Manager->LoadDueCardsForToday(Today).empty());
}

TEST_CASE("A remembered card climbs the ladder one box at a time", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    FTCard Card = SeedCardDueOn(Test, 1, Today, Today);

    // What a finished repetition session does to every card it promotes. Before this was wired up
    // a card reached box 1 and stayed due there for ever.
    for (int Expected = 2; Expected <= GetLastStage(); ++Expected)
    {
        REQUIRE(Test.Manager->ReportReviewAnswer(Card, true, Today));

        Card = *Test.Repository->LoadCard(Card.Id).value();

        CHECK(Card.LeitnerStage == Expected);
        CHECK_FALSE(Card.CompletedDate.has_value());
    }

    // Past the last box it leaves rotation rather than climbing further.
    REQUIRE(Test.Manager->ReportReviewAnswer(Card, true, Today));

    Card = *Test.Repository->LoadCard(Card.Id).value();

    CHECK(Card.CompletedDate.has_value());
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("A card stranded above a shortened ladder completes on its next answered review",
          "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    FTCard Card = SeedCardDueOn(Test, 5, Today - 16, Today);

    FTSettings::Get().SetIntervalDays({1, 2, 4});

    REQUIRE(Test.Manager->ReportReviewAnswer(Card, true, Today));

    Card = *Test.Repository->LoadCard(Card.Id).value();

    CHECK(Card.CompletedDate.has_value());
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("Growing the ladder leaves completed cards completed", "[schedulemanager][laddermigration]")
{
    RulesGuard Guard;
    Fixture Test;
    FTSettings::Get().SetIntervalDays({1, 2, 4});
    FTCard Card = SeedCardDueOn(Test, 3, Today - 8, Today);
    REQUIRE(Test.Manager->ReportReviewAnswer(Card, true, Today));
    FTSettings::Get().SetIntervalDays({1, 2, 4, 14, 40, 120});
    CHECK(Test.Manager->LoadDueCardsForToday(Today + 100).empty());
    Card = *Test.Repository->LoadCard(Card.Id).value();
    CHECK(Card.CompletedDate.has_value());
    CHECK(Card.LeitnerStage == 3);
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("Applying outcomes reports how many cards moved", "[schedulemanager][repetition]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 2, Today, Today);

    CHECK(Test.Manager->ReportSessionOutcomes({{Seeded.Id, true}}, Today) == 1);
    CHECK((*Test.Repository->LoadCard(Seeded.Id))->LeitnerStage == 3);
}

TEST_CASE("An outcome for a card that has gone is skipped", "[schedulemanager][repetition]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 2, Today, Today);

    // A deck deleted while a session was running should not cost the rest of the run.
    const int Moved = Test.Manager->ReportSessionOutcomes(
        {{"no-such-card", true}, {Seeded.Id, true}}, Today);

    CHECK(Moved == 1);
    CHECK((*Test.Repository->LoadCard(Seeded.Id))->LeitnerStage == 3);
}

TEST_CASE("No outcomes moves nothing", "[schedulemanager][repetition]")
{
    Fixture Test;

    // Quitting partway commits nothing, which reaches here as an empty list rather than as a
    // session that never finished.
    CHECK(Test.Manager->ReportSessionOutcomes({}, Today) == 0);
}

TEST_CASE("A day's load punishes only what missed its date", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    for (int Index = 0; Index < 100; ++Index)
    {
        SeedCardDueOn(Test, 3, Today - 4, Today);
    }

    for (int Index = 0; Index < 100; ++Index)
    {
        SeedCardDueOn(Test, 3, Today - 10, Today - 6);
    }

    const std::vector<FTCardHandle> Due = Test.Manager->LoadDueCardsForToday(Today);

    REQUIRE(Due.size() == 200);

    int OnTime = 0;
    int Punished = 0;

    for (const FTCardHandle& Subject : Due)
    {
        CHECK(Subject->DueDate <= Today);

        if (Subject->LeitnerStage == 3)
        {
            ++OnTime;
        }

        if (Subject->LeitnerStage == 2)
        {
            ++Punished;
        }
    }

    CHECK(OnTime == 100);
    CHECK(Punished == 100);
}

TEST_CASE("Loading twice in one day punishes nothing twice", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    for (int Index = 0; Index < 100; ++Index)
    {
        SeedCardDueOn(Test, 3, Today - 10, Today - 6);
    }

    Test.Manager->LoadDueCardsForToday(Today);

    const std::vector<FTCardHandle> Second = Test.Manager->LoadDueCardsForToday(Today);

    REQUIRE(Second.size() == 100);

    for (const FTCardHandle& Subject : Second)
    {
        CHECK(Subject->LeitnerStage == 2);
    }
}

TEST_CASE("A card left unanswered is not punished again the next day", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    SeedCardDueOn(Test, 3, Today - 10, Today - 6);

    REQUIRE(Test.Manager->LoadDueCardsForToday(Today).size() == 1);

    const std::vector<FTCardHandle> Tomorrow = Test.Manager->LoadDueCardsForToday(Today + 1);

    REQUIRE(Tomorrow.size() == 1);
    CHECK(Tomorrow[0]->LeitnerStage == 2);
    CHECK(Tomorrow[0]->DueDate <= Today + 1);
}
