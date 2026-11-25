#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/FTDatabase.h"
#include "core/FTJournal.h"
#include "core/FTSchedule.h"
#include "core/FTScheduleOptimizer.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"

#include <QSettings>
#include <QTemporaryDir>

#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    struct WeightsGuard
    {
        ~WeightsGuard()
        {
            FTSettings::Get().SetScheduleWeights(DefaultScheduleWeights, 0);
        }
    };

    FTJournalEvent MakeEvent(const std::string& Id, const std::string& CardId, const char* EventId, FTDayNumber Day,
                             int EventValue = 0)
    {
        FTJournalEvent Result;
        Result.Id = Id;
        Result.EventId = EventId;
        Result.CardId = CardId;
        Result.EventValue = EventValue;
        Result.Day = Day;

        return Result;
    }

    FTScheduleWeights MakeSlowForgettingWeights()
    {
        FTScheduleWeights Result = DefaultScheduleWeights;
        Result[2] = 10.0;
        Result[8] = 1.9;

        return Result;
    }

    std::vector<FTReviewHistory> SimulateHistories(const FTScheduleWeights& Truth, int CardCount)
    {
        std::mt19937 Engine{7};
        std::vector<FTReviewHistory> Result;

        for (int Card = 0; Card < CardCount; ++Card)
        {
            FTDayNumber Day = Card % 30;
            FTMemoryState Scheduled = GetFirstMemoryState(DefaultScheduleWeights, true);
            FTMemoryState Actual = GetFirstMemoryState(Truth, true);
            FTReviewHistory History{{Day, true}};

            for (int Review = 0; Review < 6; ++Review)
            {
                const int Interval = GetNextIntervalDays(Scheduled.Stability);
                const double Chance = static_cast<double>(Engine()) / static_cast<double>(Engine.max());
                const bool bRemembered = Chance < GetRetrievability(Actual.Stability, Interval);

                Day += Interval;
                History.push_back({Day, bRemembered});

                Scheduled = GetNextMemoryState(DefaultScheduleWeights, Scheduled, bRemembered, Interval);
                Actual = GetNextMemoryState(Truth, Actual, bRemembered, Interval);
            }

            Result.push_back(std::move(History));
        }

        return Result;
    }
}

TEST_CASE("Review histories start at a learn and end at a reset", "[schedule][optimizer]")
{
    std::unordered_map<std::string, std::vector<FTJournalEvent>> EventsByCard;
    EventsByCard["a"] = {MakeEvent("1", "a", JournalLearned, 0),       MakeEvent("2", "a", JournalReviewed, 3, 1),
                         MakeEvent("3", "a", JournalReviewed, 14, 0),  MakeEvent("4", "a", JournalPooled, 20),
                         MakeEvent("5", "a", JournalLearned, 30),      MakeEvent("6", "a", JournalReviewed, 33, 1)};
    EventsByCard["b"] = {MakeEvent("7", "b", JournalPlaced, 0, 3), MakeEvent("8", "b", JournalReviewed, 5, 1)};
    EventsByCard["c"] = {MakeEvent("9", "c", JournalLearned, 0)};

    const std::vector<FTReviewHistory> Histories = BuildReviewHistories(EventsByCard);

    REQUIRE(Histories.size() == 2);
    REQUIRE(Histories[0].size() == 3);
    CHECK(Histories[0][1].Day == 3);
    CHECK(Histories[0][1].bRemembered);
    CHECK(Histories[0][2].Day == 14);
    CHECK_FALSE(Histories[0][2].bRemembered);
    REQUIRE(Histories[1].size() == 2);
    CHECK(Histories[1][0].Day == 30);
    CHECK(CountTrainingReviews(Histories) == 3);
}

TEST_CASE("A same-day answer moves the memory but is not a training review", "[schedule][optimizer]")
{
    const std::vector<FTReviewHistory> Histories{{{0, true}, {0, false}, {3, true}}};

    CHECK(CountTrainingReviews(Histories) == 1);
}

TEST_CASE("The schedule is refitted after a thousand reviews and then every five hundred", "[schedule][optimizer]")
{
    CHECK_FALSE(ShouldOptimizeSchedule(999, 0));
    CHECK(ShouldOptimizeSchedule(1000, 0));
    CHECK_FALSE(ShouldOptimizeSchedule(1499, 1000));
    CHECK(ShouldOptimizeSchedule(1500, 1000));
}

TEST_CASE("Review histories load from the journal table", "[schedule][optimizer]")
{
    std::optional<FTDatabase> Target = FTDatabase::OpenInMemory();
    REQUIRE(Target.has_value());
    REQUIRE(MigrateSchema(*Target));

    REQUIRE(AppendJournalEvent(*Target, MakeEvent("1", "a", JournalLearned, 100)));
    REQUIRE(AppendJournalEvent(*Target, MakeEvent("2", "a", JournalReviewed, 103, 1)));
    REQUIRE(AppendJournalEvent(*Target, MakeEvent("3", "a", JournalReviewed, 117, 0)));

    const std::vector<FTReviewHistory> Histories = LoadReviewHistories(*Target);

    REQUIRE(Histories.size() == 1);
    CHECK(CountTrainingReviews(Histories) == 2);
}

TEST_CASE("The optimizer moves the weights towards how the user forgets", "[schedule][optimizer]")
{
    const FTScheduleWeights Truth = MakeSlowForgettingWeights();
    const std::vector<FTReviewHistory> Histories = SimulateHistories(Truth, 400);

    REQUIRE(CountTrainingReviews(Histories) == 2400);
    REQUIRE(GetScheduleLoss(Truth, Histories) < GetScheduleLoss(DefaultScheduleWeights, Histories));

    const FTScheduleFit Fit = OptimizeScheduleWeights(Histories);

    CHECK(Fit.ReviewCount == 2400);
    CHECK(Fit.FittedLoss < Fit.DefaultLoss);
    CHECK(Fit.Weights[2] > DefaultScheduleWeights[2] + 2.0);
    CHECK(Fit.Weights[0] == DefaultScheduleWeights[0]);
    CHECK(Fit.Weights[16] == DefaultScheduleWeights[16]);
}

TEST_CASE("The optimizer gives the same weights for the same reviews", "[schedule][optimizer]")
{
    const std::vector<FTReviewHistory> Histories = SimulateHistories(MakeSlowForgettingWeights(), 200);

    CHECK(OptimizeScheduleWeights(Histories).Weights == OptimizeScheduleWeights(Histories).Weights);
}

TEST_CASE("The optimizer keeps the defaults without reviews", "[schedule][optimizer]")
{
    const FTScheduleFit Fit = OptimizeScheduleWeights({{{0, true}}});

    CHECK(Fit.ReviewCount == 0);
    CHECK(Fit.Weights == DefaultScheduleWeights);
}

TEST_CASE("Fitted weights change the next interval", "[schedule][optimizer]")
{
    const WeightsGuard Guard;

    FTCard Default;
    LearnCard(Default, 0);

    FTScheduleWeights Weights = DefaultScheduleWeights;
    Weights[2] = 10.0;
    FTSettings::Get().SetScheduleWeights(Weights, 1000);

    FTCard Fitted;
    LearnCard(Fitted, 0);

    REQUIRE(Default.DueDate.has_value());
    REQUIRE(Fitted.DueDate.has_value());
    CHECK(*Default.DueDate == 2);
    CHECK(*Fitted.DueDate == 7);
}

TEST_CASE("Fitted weights survive a reopen and stay within bounds", "[schedule][optimizer][settings]")
{
    QTemporaryDir Folder;
    const QString FilePath = Folder.filePath("settings.ini");

    FTScheduleWeights Weights = DefaultScheduleWeights;
    Weights[2] = 5.5;
    Weights[4] = 100.0;

    {
        FTSettings Settings{FilePath};
        Settings.SetScheduleWeights(Weights, 1234);
    }

    const FTSettings Reopened{FilePath};
    const FTScheduleRules& Rules = Reopened.GetScheduleRules();

    CHECK_THAT(Rules.Weights[2], Catch::Matchers::WithinAbs(5.5, 1e-6));
    CHECK(Rules.Weights[4] == 10.0);
    CHECK(Rules.FittedReviewCount == 1234);
}

TEST_CASE("Unreadable stored weights fall back to the defaults", "[schedule][optimizer][settings]")
{
    QTemporaryDir Folder;
    const QString FilePath = Folder.filePath("settings.ini");

    {
        QSettings Store{FilePath, QSettings::IniFormat};
        Store.setValue("schedule/weights", "1,2,3");
        Store.setValue("schedule/fittedReviewCount", 1000);
    }

    const FTSettings Reopened{FilePath};

    CHECK(Reopened.GetScheduleRules().Weights == DefaultScheduleWeights);
}

TEST_CASE("Stored weights from no fit leave the defaults in place", "[schedule][optimizer][settings]")
{
    QTemporaryDir Folder;
    const QString FilePath{Folder.filePath("settings.ini")};

    FTScheduleWeights Weights{DefaultScheduleWeights};
    Weights[2] = 5.5;

    {
        FTSettings Settings{FilePath};
        Settings.SetScheduleWeights(Weights, 0);
    }

    const FTSettings Reopened{FilePath};

    CHECK(Reopened.GetScheduleRules().Weights == DefaultScheduleWeights);
}
