#include <catch2/catch_test_macros.hpp>

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTJournal.h"
#include "core/FTSchedule.h"
#include "core/FTScheduleManager.h"
#include "core/FTSchema.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
    constexpr FTDayNumber Today = 20000;

    struct RulesGuard
    {
        ~RulesGuard()
        {
            const FTScheduleRules Defaults;
            FTSettings::Get().SetCustomIntensity(Defaults.Custom);
            FTSettings::Get().SetLearningIntensity(Defaults.IntensityIndex);
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

    FTCard SeedCardDueOn(Fixture& Test, double Stability, FTDayNumber ReviewedOn, FTDayNumber DueOn)
    {
        FTCard Card;
        Card.TargetWord = "le chien";
        Card.Translation = "dog";

        const std::optional<std::string> CardId = Test.Repository->AddCard(Card);
        REQUIRE(CardId.has_value());

        FTCard Stored = **Test.Repository->LoadCard(*CardId);
        PlaceWithStability(Stored, Stability, ReviewedOn);
        Stored.DueDate = DueOn;

        REQUIRE(Test.Repository->UpdateCardSchedule(Stored));

        return Stored;
    }

    FTCard Reload(Fixture& Test, const std::string& CardId)
    {
        return *Test.Repository->LoadCard(CardId).value();
    }

    std::vector<int> CollectDueDays(Fixture& Test, int DayCount, bool bAnswerRemembered)
    {
        std::vector<int> DueOn;

        for (int Offset = 0; Offset < DayCount; ++Offset)
        {
            const FTDayNumber Day = Today + Offset;
            const std::vector<FTCardHandle> Due = Test.Manager->LoadDueCardsForToday(Day);

            if (Due.empty())
            {
                continue;
            }

            DueOn.push_back(Offset);
            Test.Manager->ReportReviewAnswer(*Due.at(0), bAnswerRemembered, Day);
        }

        return DueOn;
    }
}

TEST_CASE("A card due today is returned unchanged", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 4.0, Today - 4, Today);

    const std::vector<FTCardHandle> Due = Test.Manager->LoadDueCardsForToday(Today);

    REQUIRE(Due.size() == 1);
    CHECK(Due[0]->LeitnerStage == Seeded.LeitnerStage);
    CHECK(Due[0]->Stability == Seeded.Stability);
}

TEST_CASE("A card long past its date is handed back as it was, nothing written", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 14.0, Today - 34, Today - 20);

    const std::vector<FTCardHandle> Due = Test.Manager->LoadDueCardsForToday(Today);

    REQUIRE(Due.size() == 1);
    CHECK(Due[0]->LeitnerStage == Seeded.LeitnerStage);
    CHECK(Due[0]->DueDate == Today - 20);

    const FTCard Reloaded = Reload(Test, Seeded.Id);
    CHECK(Reloaded.Stability == 14.0);
    CHECK(Reloaded.DueDate == Today - 20);
}

TEST_CASE("Loading today's due cards leaves them due today", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    SeedCardDueOn(Test, 4.0, Today - 5, Today - 1);
    SeedCardDueOn(Test, 14.0, Today - 30, Today - 20);
    SeedCardDueOn(Test, 2.0, Today - 9, Today - 4);

    REQUIRE(Test.Manager->LoadDueCardsForToday(Today).size() == 3);
    CHECK(Test.Manager->LoadDueCardsForToday(Today).size() == 3);
    CHECK(Test.Repository->CountDueCards(Today) == 3);
    CHECK(Test.Manager->CountDueCardsForToday(Today) == 3);
}

TEST_CASE("A review of one batch leaves the rest of the day's cards due", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Answered = SeedCardDueOn(Test, 4.0, Today - 5, Today - 1);
    SeedCardDueOn(Test, 4.0, Today - 5, Today - 1);
    SeedCardDueOn(Test, 4.0, Today - 5, Today - 1);

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

    SeedCardDueOn(Test, 2.0, Today - 2, Today + 1);

    CHECK(Test.Manager->LoadDueCardsForToday(Today).empty());
}

TEST_CASE("Reporting I know it raises stability and saves it", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 4.0, Today - 4, Today);

    CHECK(Test.Manager->ReportReviewAnswer(Seeded, true, Today));

    const FTCard Reloaded = Reload(Test, Seeded.Id);
    CHECK(*Reloaded.Stability > 4.0);
    CHECK(*Reloaded.DueDate > Today + 4);
}

TEST_CASE("Reporting I forgot lowers stability and saves it", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 14.0, Today - 14, Today);

    CHECK(Test.Manager->ReportReviewAnswer(Seeded, false, Today));

    const FTCard Reloaded = Reload(Test, Seeded.Id);
    CHECK(*Reloaded.Stability < 14.0);
    CHECK(*Reloaded.DueDate > Today);
    CHECK(Test.Manager->LoadDueCardsForToday(Today).empty());
}

TEST_CASE("Reporting an answer for an unknown card fails without crashing", "[schedulemanager]")
{
    Fixture Test;

    FTCard Ghost;
    Ghost.Id = "does-not-exist";
    Ghost.TargetWord = "le fantÃ´me";

    CHECK_FALSE(Test.Manager->ReportReviewAnswer(Ghost, true, Today));
}

TEST_CASE("A learned card comes back on days 2, 8, 23, 58 and 137, then stops", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    FTCard Card;
    Card.TargetWord = "le chien";
    Card.Translation = "dog";

    const std::optional<std::string> CardId = Test.Repository->AddCard(Card);
    REQUIRE(CardId.has_value());
    REQUIRE(Test.Repository->RecordProgressEvent(*CardId, JournalLearned, 0, Today));

    CHECK(CollectDueDays(Test, 400, true) == std::vector<int>{2, 8, 23, 58, 137});

    const FTCard Finished = Reload(Test, *CardId);
    CHECK(IsCompleted(Finished));
    CHECK_FALSE(Finished.DueDate.has_value());
    CHECK(Test.Manager->LoadDueCardsForToday(Today + 1000).empty());
}

TEST_CASE("A card is not due on any day between its reviews", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    SeedCardDueOn(Test, 2.0, Today, Today + 2);

    CHECK(Test.Manager->LoadDueCardsForToday(Today).empty());
    CHECK(Test.Manager->LoadDueCardsForToday(Today + 1).empty());
    CHECK_FALSE(Test.Manager->LoadDueCardsForToday(Today + 2).empty());
}

TEST_CASE("A card answered I forgot every time keeps coming back and is never learned", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 4.0, Today - 4, Today);

    CHECK(CollectDueDays(Test, 30, false).size() > 10);

    const FTCard After = Reload(Test, Seeded.Id);
    CHECK_FALSE(IsCompleted(After));
    CHECK(After.LeitnerStage == FirstLadderStage);
}

TEST_CASE("Today's due cards exclude what is not on the schedule", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    FTCard Unlearned;
    Unlearned.TargetWord = "le chien";
    Unlearned.Translation = "dog";
    REQUIRE(Test.Repository->AddCard(Unlearned).has_value());

    SeedCardDueOn(Test, 4.0, Today - 4, Today);
    SeedCardDueOn(Test, 4.0, Today, Today + 5);

    CHECK(Test.Manager->LoadDueCardsForToday(Today).size() == 1);
}

TEST_CASE("Applying outcomes reports how many cards moved", "[schedulemanager][repetition]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 4.0, Today - 4, Today);

    CHECK(Test.Manager->ReportSessionOutcomes({{Seeded.Id, true}}, Today) == 1);
    CHECK(*Reload(Test, Seeded.Id).Stability > 4.0);
}

TEST_CASE("An outcome for a card that has gone is skipped", "[schedulemanager][repetition]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 4.0, Today - 4, Today);

    const int Moved = Test.Manager->ReportSessionOutcomes({{"no-such-card", true}, {Seeded.Id, true}}, Today);

    CHECK(Moved == 1);
    CHECK(*Reload(Test, Seeded.Id).Stability > 4.0);
}

TEST_CASE("No outcomes moves nothing", "[schedulemanager][repetition]")
{
    Fixture Test;

    CHECK(Test.Manager->ReportSessionOutcomes({}, Today) == 0);
}

TEST_CASE("Lowering the learning intensity completes a card on its next I know it", "[schedulemanager]")
{
    RulesGuard Guard;
    Fixture Test;

    const FTCard Seeded = SeedCardDueOn(Test, 40.0, Today - 40, Today);

    FTSettings::Get().SetLearningIntensity(0);

    REQUIRE(Test.Manager->ReportReviewAnswer(Seeded, true, Today));

    const FTCard Reloaded = Reload(Test, Seeded.Id);
    CHECK(IsCompleted(Reloaded));
    CHECK_FALSE(Reloaded.DueDate.has_value());
}
