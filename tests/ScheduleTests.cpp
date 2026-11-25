#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/FTDictionaryPackage.h"
#include "core/FTSchedule.h"
#include "core/FTSettings.h"

#include <utility>
#include <vector>

using namespace std;

namespace
{
    constexpr FTDayNumber Today = 20000;

    FTCard MakeCard()
    {
        FTCard Result;
        Result.TargetWord = "le chien";
        Result.Translation = "dog";

        return Result;
    }

    FTCard MakeLearnedCard(FTDayNumber LearnedOn)
    {
        FTCard Result = MakeCard();
        LearnCard(Result, LearnedOn);

        return Result;
    }

    struct RulesGuard
    {
        ~RulesGuard()
        {
            const FTScheduleRules Defaults;
            FTSettings::Get().SetCustomIntensity(Defaults.Custom);
            FTSettings::Get().SetLearningIntensity(Defaults.IntensityIndex);
        }
    };

    std::vector<int> AnswerEveryReview(FTCard& Subject, FTDayNumber LearnedOn, int ForgottenReview)
    {
        std::vector<int> Days;

        while (Subject.DueDate.has_value() && Days.size() < 50)
        {
            const FTDayNumber Day = *Subject.DueDate;
            const bool bRemembered = static_cast<int>(Days.size()) != ForgottenReview;

            Days.push_back(Day - LearnedOn);
            AnswerReview(Subject, bRemembered, Day);
        }

        return Days;
    }
}

TEST_CASE("A word answered I know it every time comes back on days 2, 8, 23, 58 and 137, then is learned",
          "[schedule]")
{
    RulesGuard Guard;

    FTCard Card = MakeLearnedCard(Today);

    CHECK(AnswerEveryReview(Card, Today, -1) == std::vector<int>{2, 8, 23, 58, 137});
    CHECK(IsCompleted(Card));
    CHECK(Card.CompletedDate == Today + 137);
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("One I forgot at the third review adds reviews", "[schedule]")
{
    RulesGuard Guard;

    FTCard Card = MakeLearnedCard(Today);

    CHECK(AnswerEveryReview(Card, Today, 2) == std::vector<int>{2, 8, 23, 27, 36, 57, 102, 195});
    CHECK(IsCompleted(Card));
}

TEST_CASE("A new card sits in the pool with no dates and no stability", "[schedule]")
{
    const FTCard Card = MakeCard();

    CHECK(IsInPool(Card));
    CHECK_FALSE(Card.DueDate.has_value());
    CHECK_FALSE(Card.Stability.has_value());
    CHECK_FALSE(Card.Difficulty.has_value());
}

TEST_CASE("Learning a pool card schedules its first review two days later", "[schedule]")
{
    RulesGuard Guard;

    const FTCard Card = MakeLearnedCard(Today);

    CHECK(Card.LeitnerStage == FirstLadderStage);
    CHECK(Card.DueDate == Today + 2);
    CHECK(Card.BoxChangedDate == Today);
    REQUIRE(Card.Stability.has_value());
    CHECK_THAT(*Card.Stability, Catch::Matchers::WithinAbs(3.173, 0.001));
    REQUIRE(Card.Difficulty.has_value());
    CHECK_THAT(*Card.Difficulty, Catch::Matchers::WithinAbs(5.283, 0.001));
}

TEST_CASE("I know it raises stability and I forgot lowers it", "[schedule]")
{
    RulesGuard Guard;

    FTCard Remembered = MakeLearnedCard(Today);
    FTCard Forgotten = Remembered;

    AnswerReview(Remembered, true, Today + 3);
    AnswerReview(Forgotten, false, Today + 3);

    CHECK(*Remembered.Stability > 3.173);
    CHECK(*Forgotten.Stability < 3.173);
    CHECK(*Forgotten.Difficulty > *Remembered.Difficulty);
    CHECK(*Forgotten.DueDate < *Remembered.DueDate);
    CHECK(Forgotten.DueDate >= Today + 4);
}

TEST_CASE("A late I know it grows stability more than one on time", "[schedule]")
{
    RulesGuard Guard;

    FTCard OnTime = MakeLearnedCard(Today);
    FTCard Late = OnTime;

    AnswerReview(OnTime, true, Today + 3);
    AnswerReview(Late, true, Today + 10);

    CHECK(*Late.Stability > *OnTime.Stability);
}

TEST_CASE("A missed day leaves the card as it was, still due", "[schedule]")
{
    RulesGuard Guard;

    const FTCard Card = MakeLearnedCard(Today);

    CHECK(IsDue(Card, Today + 30));
    CHECK(Card.LeitnerStage == FirstLadderStage);
    CHECK(Card.DueDate == Today + 2);
}

TEST_CASE("Due means today or earlier", "[schedule]")
{
    RulesGuard Guard;

    const FTCard Card = MakeLearnedCard(Today);

    CHECK_FALSE(IsDue(Card, Today + 1));
    CHECK(IsDue(Card, Today + 2));
    CHECK(IsDue(Card, Today + 3));
}

TEST_CASE("Two answers on the same day still move stability", "[schedule]")
{
    RulesGuard Guard;

    FTCard Card = MakeLearnedCard(Today);
    const double Before = *Card.Stability;

    AnswerReview(Card, false, Today);

    CHECK(*Card.Stability < Before);
    CHECK(Card.DueDate >= Today + 1);
}

TEST_CASE("A higher recall target brings reviews sooner", "[schedule]")
{
    RulesGuard Guard;

    FTSettings::Get().SetLearningIntensity(2);
    FTCard Strict = MakeLearnedCard(Today);
    CHECK(AnswerEveryReview(Strict, Today, -1) == std::vector<int>{1, 4, 10, 22, 46, 91, 173});

    FTSettings::Get().SetLearningIntensity(0);
    FTCard Relaxed = MakeLearnedCard(Today);
    CHECK(AnswerEveryReview(Relaxed, Today, -1) == std::vector<int>{3, 14, 49});
}

TEST_CASE("At 90% recall the next gap equals stability", "[schedule]")
{
    RulesGuard Guard;

    FTSettings::Get().SetLearningIntensity(0);

    CHECK(GetNextIntervalDays(10.0) == 10);
    CHECK(GetNextIntervalDays(100.4) == 100);
    CHECK(GetNextIntervalDays(0.01) == 1);
    CHECK(GetNextIntervalDays(100000.0) == MaximumIntervalDays);
    CHECK_THAT(GetRetrievability(10.0, 10), Catch::Matchers::WithinAbs(0.9, 0.0001));
    CHECK_THAT(GetRetrievability(10.0, 0), Catch::Matchers::WithinAbs(1.0, 0.0001));
}

TEST_CASE("A word is learned once it is as strong as the learning intensity asks", "[schedule]")
{
    RulesGuard Guard;

    for (size_t Index = 0; Index < LearningIntensities.size(); ++Index)
    {
        const FTLearningIntensity& Intensity{LearningIntensities[Index]};
        FTSettings::Get().SetLearningIntensity(Index);

        FTCard Below{MakeLearnedCard(Today)};
        PlaceWithStability(Below, Intensity.LearnedStability - 1.0, Today);
        CHECK_FALSE(IsCompleted(Below));

        FTCard Strong{MakeLearnedCard(Today)};
        PlaceWithStability(Strong, Intensity.LearnedStability, Today);
        CHECK(IsCompleted(Strong));
        CHECK_FALSE(Strong.DueDate.has_value());
    }
}

TEST_CASE("A single mistake never makes a new word learned sooner", "[schedule]")
{
    RulesGuard Guard;

    for (size_t Index = 0; Index < LearningIntensities.size(); ++Index)
    {
        const FTLearningIntensity& Intensity{LearningIntensities[Index]};
        FTSettings::Get().SetLearningIntensity(Index);

        const FTSchedulePreview Remembered{PreviewSchedule(-1)};

        for (int ForgottenReview = 0; ForgottenReview < static_cast<int>(Remembered.ReviewDays.size()); ++ForgottenReview)
        {
            const FTSchedulePreview Forgotten{PreviewSchedule(ForgottenReview)};

            CAPTURE(Intensity.TargetRetention, ForgottenReview);
            CHECK(Forgotten.CompletedDay > Remembered.CompletedDay);
            CHECK(Forgotten.ReviewDays.size() > Remembered.ReviewDays.size());
        }
    }
}

TEST_CASE("Only a word as strong as the intensity's quick review stability asks yes or no", "[schedule]")
{
    RulesGuard Guard;

    FTCard Card = MakeCard();
    CHECK_FALSE(IsQuickReviewCard(Card));

    Card.Stability = 13.9;
    CHECK_FALSE(IsQuickReviewCard(Card));

    Card.Stability = 14.0;
    CHECK(IsQuickReviewCard(Card));

    FTSettings::Get().SetCustomIntensity({93, 180, 30});
    FTSettings::Get().SetLearningIntensity(CustomIntensityIndex);
    CHECK_FALSE(IsQuickReviewCard(Card));
}

TEST_CASE("Every intensity starts quick reviews at the same stability", "[schedule]")
{
    for (const FTLearningIntensity& Intensity : LearningIntensities)
    {
        CHECK(Intensity.QuickReviewStability == LearningIntensities.front().QuickReviewStability);
    }
}

TEST_CASE("Custom intensity drives the schedule", "[schedule]")
{
    RulesGuard Guard;

    FTSettings::Get().SetCustomIntensity({90, 30, 5});
    FTSettings::Get().SetLearningIntensity(CustomIntensityIndex);

    FTCard Card{MakeLearnedCard(Today)};

    CHECK(AnswerEveryReview(Card, Today, -1) == std::vector<int>{3, 14});
    CHECK(IsCompleted(Card));
}

TEST_CASE("Progress stages climb with stability and stay within the shown count", "[schedule]")
{
    RulesGuard Guard;

    CHECK(GetLastStage() == ProgressStageCount);
    CHECK(GetProgressStage(0.4) == FirstLadderStage);
    CHECK(GetProgressStage(3.2) == 1);
    CHECK(GetProgressStage(11.1) == 2);
    CHECK(GetProgressStage(35.0) == 3);
    CHECK(GetProgressStage(102.0) == 4);
    CHECK(GetProgressStage(5000.0) == ProgressStageCount);
}

TEST_CASE("Returning to the pool clears the dates, stability and difficulty", "[schedule]")
{
    RulesGuard Guard;

    FTCard Card = MakeLearnedCard(Today);
    ReturnToPool(Card, Today + 5);

    CHECK(IsInPool(Card));
    CHECK_FALSE(Card.DueDate.has_value());
    CHECK_FALSE(Card.CompletedDate.has_value());
    CHECK_FALSE(Card.Stability.has_value());
    CHECK_FALSE(Card.Difficulty.has_value());
    CHECK(Card.BoxChangedDate == Today + 5);
}

TEST_CASE("Marking a card complete by hand keeps its stability", "[schedule]")
{
    RulesGuard Guard;

    FTCard Card = MakeLearnedCard(Today);
    const double Stability = *Card.Stability;

    MarkCompleted(Card, Today + 1);

    CHECK(IsCompleted(Card));
    CHECK(Card.CompletedDate == Today + 1);
    CHECK_FALSE(Card.DueDate.has_value());
    CHECK(Card.Stability == Stability);
}

TEST_CASE("Placing a card with a known stability schedules it that far out", "[schedule]")
{
    RulesGuard Guard;

    FTSettings::Get().SetLearningIntensity(0);

    FTCard Card = MakeCard();
    PlaceWithStability(Card, 14.0, Today);

    CHECK(Card.DueDate == Today + 14);
    CHECK(Card.Stability == 14.0);
    CHECK(Card.Difficulty.has_value());
    CHECK(Card.LeitnerStage == GetProgressStage(14.0));
}

TEST_CASE("Schedule settings are sanitised", "[schedule]")
{
    FTScheduleRules Rules;
    Rules.IntensityIndex = 9;
    Rules.Custom = {20, 9000, 9000};

    Sanitise(Rules);

    CHECK(Rules.IntensityIndex == CustomIntensityIndex);
    CHECK(Rules.Custom.TargetRetention == MinimumTargetRetention);
    CHECK(Rules.Custom.LearnedStability == MaximumLearnedStability);
    CHECK(Rules.Custom.QuickReviewStability == MaximumQuickReviewStability);
}

TEST_CASE("The preview shows the remembered path and the path with one I forgot", "[schedule]")
{
    RulesGuard Guard;

    const FTSchedulePreview Remembered = PreviewSchedule(-1);
    CHECK(Remembered.ReviewDays == std::vector<int>{2, 8, 23, 58, 137});
    CHECK(Remembered.QuickReviews == std::vector<bool>{false, false, true, true, true});
    CHECK(Remembered.CompletedDay == 137);

    const FTSchedulePreview Forgotten = PreviewSchedule(SchedulePreviewForgottenReview);
    CHECK(Forgotten.ReviewDays == std::vector<int>{2, 8, 23, 27, 36, 57, 102, 195});
    CHECK(Forgotten.CompletedDay == 195);
}

TEST_CASE("The daily goal is the training count times the sessions per day", "[schedule]")
{
    FTIntensiveLearningRules Rules;
    Rules.TrainingWordCount = 10;
    Rules.SessionsPerDay = 4;

    Sanitise(Rules);

    // A goal set independently of the session size would stop meaning anything, so it is derived
    // rather than stored.
    CHECK(Rules.GetDailyGoal() == 40);
    CHECK(Rules.TrainingWordCount == 10);
}

TEST_CASE("A pair with no dictionary behind it falls back to one that has", "[schedule][packages]")
{
    // Which pairs exist comes from the dictionary files on disk, so a test has to say what is
    // installed before it can expect a fallback.
    FTSettings::SetAvailableLanguagePairs({FTLanguageRules{"fr", "en"}});

    FTLanguageRules Rules;
    Rules.TargetLanguage = "de";
    Rules.NativeLanguage = "ja";

    Sanitise(Rules);

    CHECK(Rules.TargetLanguage == "fr");
    CHECK(Rules.NativeLanguage == "en");

    FTSettings::SetAvailableLanguagePairs({});
}

TEST_CASE("An installed language pair is left alone", "[schedule][packages]")
{
    FTSettings::SetAvailableLanguagePairs({FTLanguageRules{"fr", "en"}});

    FTLanguageRules Rules;
    const FTLanguageRules Before = Rules;

    Sanitise(Rules);

    CHECK(Rules.TargetLanguage == Before.TargetLanguage);
    CHECK(Rules.NativeLanguage == Before.NativeLanguage);

    FTSettings::SetAvailableLanguagePairs({});
}

TEST_CASE("Half a valid pair is still rejected", "[schedule][packages]")
{
    // Both halves exist, but only as fr-en. A mismatched half has to fall back rather than being
    // accepted because each code looks familiar on its own.
    FTSettings::SetAvailableLanguagePairs({FTLanguageRules{"fr", "en"},
                                           FTLanguageRules{"es", "en"}});

    FTLanguageRules Rules;
    Rules.TargetLanguage = "fr";
    Rules.NativeLanguage = "es";

    Sanitise(Rules);

    CHECK(Rules.NativeLanguage == "en");

    FTSettings::SetAvailableLanguagePairs({});
}

TEST_CASE("A pair is left alone when no dictionary is installed", "[schedule][packages]")
{
    FTSettings::SetAvailableLanguagePairs({});

    // Nothing is installed, so there is no correct pair to move to. Overwriting the user's own
    // choice here would lose it, and the setting has to survive a missing dictionary.
    FTLanguageRules Rules;
    Rules.TargetLanguage = "de";
    Rules.NativeLanguage = "ja";

    Sanitise(Rules);

    CHECK(Rules.TargetLanguage == "de");
    CHECK(Rules.NativeLanguage == "ja");
}

TEST_CASE("A dictionary reports which languages it can translate into", "[packages]")
{
    FTDictionaryPackage Package;
    Package.TargetLanguage = "fr";
    Package.NativeLanguages = {"en", "uk"};

    CHECK(Package.SupportsNative("en"));
    CHECK(Package.SupportsNative("uk"));
    CHECK_FALSE(Package.SupportsNative("es"));
}

TEST_CASE("A dictionary built for a schema outside the readable range is refused", "[packages]")
{
    FTDictionaryPackage Package;
    Package.SchemaVersion = DictionarySchemaVersion;
    CHECK(Package.IsCompatible());

    Package.SchemaVersion = OldestDictionarySchemaVersion;
    CHECK(Package.IsCompatible());

    // Opening it would half-work, which is worse than saying it cannot be read.
    Package.SchemaVersion = DictionarySchemaVersion + 1;
    CHECK_FALSE(Package.IsCompatible());

    // Older is refused for the same reason, and it is the case that actually happens: the file
    // opens, identifies itself, and then has no column the teaching order is read from.
    Package.SchemaVersion = OldestDictionarySchemaVersion - 1;
    CHECK_FALSE(Package.IsCompatible());

    // No version at all reads as zero, which is a schema this build has never written.
    Package.SchemaVersion = 0;
    CHECK_FALSE(Package.IsCompatible());
}

TEST_CASE("Session sizes are kept positive", "[schedule]")
{
    FTIntensiveLearningRules Rules;
    Rules.TrainingWordCount = 0;
    Rules.SessionsPerDay = -1;

    Sanitise(Rules);

    CHECK(Rules.TrainingWordCount >= 10);
    CHECK(Rules.SessionsPerDay == 0);
}

TEST_CASE("Zero sessions per day leaves no daily goal", "[schedule]")
{
    FTIntensiveLearningRules Rules;
    Rules.SessionsPerDay = 0;

    Sanitise(Rules);

    CHECK(Rules.SessionsPerDay == 0);
    CHECK(Rules.GetDailyGoal() == 0);
}

TEST_CASE("The examples shown per meaning is clamped to a readable range", "[settings]")
{
    FTDictionaryDisplayRules Rules;

    Rules.ExamplesPerSense = -4;
    Sanitise(Rules);
    CHECK(Rules.ExamplesPerSense == 0);

    // A sense can carry dozens of examples, so a stored value above the ceiling is a display
    // problem rather than a preference to honour.
    Rules.ExamplesPerSense = 500;
    Sanitise(Rules);
    CHECK(Rules.ExamplesPerSense == 10);

    // Zero is a real choice: hide them.
    Rules.ExamplesPerSense = 0;
    Sanitise(Rules);
    CHECK(Rules.ExamplesPerSense == 0);
}

TEST_CASE("Three examples per meaning is the default", "[settings]")
{
    CHECK(FTDictionaryDisplayRules{}.ExamplesPerSense == 3);
}
