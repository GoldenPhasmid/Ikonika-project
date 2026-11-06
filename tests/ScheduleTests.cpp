#include <catch2/catch_test_macros.hpp>

#include "core/FTDictionaryPackage.h"
#include "core/FTSchedule.h"
#include "core/FTSettings.h"

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

    FTCard MakeCardInStage(int Stage, FTDayNumber EnteredOn, FTDayNumber DueOn)
    {
        FTCard Result = MakeCard();
        Result.LeitnerStage = Stage;
        Result.BoxChangedDate = EnteredOn;
        Result.DueDate = DueOn;

        return Result;
    }

    /** Restores the default ladder on destruction, since FTSettings is a process-wide singleton. */
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

    void SetIntervals(std::initializer_list<int> Days)
    {
        FTSettings::Get().SetIntervalDays(QList<int>(Days.begin(), Days.end()));
    }
}

TEST_CASE("The default ladder brings a word back on days 1, 3, 7, 21, 61 and 181", "[schedule]")
{
    RulesGuard Guard;

    CHECK(GetLastStage() == 6);

    CHECK(GetIntervalDays(1) == 1);
    CHECK(GetIntervalDays(2) == 2);
    CHECK(GetIntervalDays(3) == 4);
    CHECK(GetIntervalDays(4) == 14);
    CHECK(GetIntervalDays(5) == 40);
    CHECK(GetIntervalDays(6) == 120);

    int Day = 0;
    std::vector<int> Days;

    for (int Stage = 1; Stage <= GetLastStage(); ++Stage)
    {
        Day += GetIntervalDays(Stage);
        Days.push_back(Day);
    }

    CHECK(Days == std::vector<int>{1, 3, 7, 21, 61, 181});
}

TEST_CASE("A stage outside the ladder still returns a usable interval", "[schedule]")
{
    RulesGuard Guard;

    CHECK(GetIntervalDays(0) == 1);
    CHECK(GetIntervalDays(-4) == 1);

    // Clamped to the last box rather than failing, so a card from a longer ladder still schedules.
    CHECK(GetIntervalDays(99) == GetIntervalDays(GetLastStage()));
}

TEST_CASE("A new card sits in the pool with no dates", "[schedule]")
{
    const FTCard Fresh = MakeCard();

    CHECK(IsInPool(Fresh));
    CHECK_FALSE(IsDue(Fresh, Today));
    CHECK_FALSE(IsExpired(Fresh, Today));
}

TEST_CASE("Promoting a pool card puts it in the first stage, due tomorrow", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCard();

    Promote(Card, Today);

    CHECK(Card.LeitnerStage == FirstLadderStage);
    CHECK(Card.DueDate == Today + 1);
    CHECK(Card.BoxChangedDate == Today);
    CHECK_FALSE(IsCompleted(Card));
}

TEST_CASE("Promoting walks up the ladder, one stage at a time", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCard();

    Promote(Card, Today);
    CHECK(Card.LeitnerStage == 1);
    CHECK(Card.DueDate == Today + 1);

    Promote(Card, Today + 1);
    CHECK(Card.LeitnerStage == 2);
    CHECK(Card.DueDate == Today + 1 + 2);

    Promote(Card, Today + 3);
    CHECK(Card.LeitnerStage == 3);
    CHECK(Card.DueDate == Today + 3 + 4);
}

TEST_CASE("Promoting past the last stage completes the card", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(GetLastStage(), Today - 16, Today);

    Promote(Card, Today);

    CHECK(IsCompleted(Card));
    CHECK(Card.CompletedDate == Today);
    CHECK_FALSE(Card.DueDate.has_value());
    CHECK_FALSE(IsDue(Card, Today + 100));
    CHECK_FALSE(IsExpired(Card, Today + 100));
}

TEST_CASE("A completed card is left alone by a further promotion", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(GetLastStage(), Today - 16, Today);
    Promote(Card, Today);

    Promote(Card, Today + 5);

    CHECK(Card.CompletedDate == Today);
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("Demoting drops one stage and schedules the new stage's own interval", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(3, Today - 8, Today);

    Demote(Card, Today);

    CHECK(Card.LeitnerStage == 2);
    CHECK(Card.DueDate == Today + GetIntervalDays(2));
    CHECK(Card.BoxChangedDate == Today);
}

TEST_CASE("Demoting in the first stage keeps the card there by default", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(1, Today - 1, Today);

    Demote(Card, Today);

    CHECK(Card.LeitnerStage == FirstLadderStage);
    CHECK(Card.DueDate == Today + GetIntervalDays(FirstLadderStage));
}

TEST_CASE("The stage the card entered on is kept when the stage does not change", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(1, Today, Today + 1);

    Demote(Card, Today + 1);
    Demote(Card, Today + 2);

    CHECK(Card.BoxChangedDate == Today);
}

TEST_CASE("Failing in the first stage can be set to return the card to the pool", "[schedule]")
{
    RulesGuard Guard;
    FTSettings::Get().SetFirstStageFailureReturnsToPool(true);

    FTCard Card = MakeCardInStage(1, Today - 1, Today);

    Demote(Card, Today);

    CHECK(IsInPool(Card));
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("A completed card is demoted back into the last stage", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(GetLastStage(), Today - 16, Today);
    Promote(Card, Today);
    REQUIRE(IsCompleted(Card));

    Demote(Card, Today + 30);

    CHECK(Card.LeitnerStage == GetLastStage());
    CHECK_FALSE(IsCompleted(Card));
    CHECK(Card.DueDate == Today + 30 + GetIntervalDays(GetLastStage()));
}

TEST_CASE("Returning to the pool clears every date the schedule set", "[schedule]")
{
    FTCard Card = MakeCardInStage(3, Today - 4, Today);

    ReturnToPool(Card, Today);

    CHECK(Card.LeitnerStage == PoolStage);
    CHECK(IsInPool(Card));
    CHECK_FALSE(Card.DueDate.has_value());
    CHECK_FALSE(IsCompleted(Card));
    CHECK(Card.BoxChangedDate == Today);
}

TEST_CASE("A completed card can be sent back to the pool by hand", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(GetLastStage(), Today - 16, Today);
    Promote(Card, Today);
    REQUIRE(IsCompleted(Card));

    ReturnToPool(Card, Today + 40);

    CHECK(IsInPool(Card));
    CHECK_FALSE(IsCompleted(Card));
}

TEST_CASE("Marking a pool card complete by hand sends it straight to the last stage", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCard();

    MarkCompleted(Card, Today);

    CHECK(Card.LeitnerStage == GetLastStage());
    CHECK(IsCompleted(Card));
    CHECK_FALSE(Card.DueDate.has_value());
    CHECK(Card.BoxChangedDate == Today);
}

TEST_CASE("Marking a mid-ladder card complete by hand skips the rest of the ladder", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(2, Today - 1, Today);

    MarkCompleted(Card, Today);

    CHECK(Card.LeitnerStage == GetLastStage());
    CHECK(IsCompleted(Card));
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("Marking an already-completed card complete again just restamps the date", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(GetLastStage(), Today - 16, Today);
    Promote(Card, Today);
    REQUIRE(IsCompleted(Card));

    MarkCompleted(Card, Today + 10);

    CHECK(Card.LeitnerStage == GetLastStage());
    CHECK(Card.CompletedDate == Today + 10);
}

TEST_CASE("The effective stage clamps a stranded card but leaves the pool and a normal stage "
          "alone", "[schedule]")
{
    RulesGuard Guard;

    CHECK(GetEffectiveStage(MakeCard()) == PoolStage);
    CHECK(GetEffectiveStage(MakeCardInStage(3, Today - 4, Today)) == 3);

    // Stranded above a since-shortened ladder: never higher than the real last stage.
    SetIntervals({1, 2, 4});
    CHECK(GetEffectiveStage(MakeCardInStage(5, Today - 16, Today)) == 3);
}

TEST_CASE("Promoting a card stranded above a shortened ladder completes it instead of erroring",
          "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(5, Today - 16, Today);

    SetIntervals({1, 2, 4});
    Promote(Card, Today);

    CHECK(IsCompleted(Card));
    CHECK(Card.CompletedDate == Today);
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("A completed card stays completed when the ladder later grows", "[schedule]")
{
    RulesGuard Guard;
    SetIntervals({1, 2, 4});
    FTCard Card = MakeCardInStage(3, Today - 8, Today);
    Promote(Card, Today);
    REQUIRE(IsCompleted(Card));

    SetIntervals({1, 2, 4, 7, 14});

    CHECK(IsCompleted(Card));
    CHECK_FALSE(IsDue(Card, Today + 100));
    CHECK_FALSE(IsExpired(Card, Today + 100));
}

TEST_CASE("Due means today or earlier, expired means strictly earlier", "[schedule]")
{
    const FTCard Tomorrow = MakeCardInStage(2, Today - 2, Today + 1);
    const FTCard Now = MakeCardInStage(2, Today - 2, Today);
    const FTCard Yesterday = MakeCardInStage(2, Today - 3, Today - 1);

    CHECK_FALSE(IsDue(Tomorrow, Today));
    CHECK_FALSE(IsExpired(Tomorrow, Today));

    CHECK(IsDue(Now, Today));
    CHECK_FALSE(IsExpired(Now, Today));

    CHECK(IsDue(Yesterday, Today));
    CHECK(IsExpired(Yesterday, Today));
}

TEST_CASE("A missed day demotes the card once", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(3, Today - 5, Today - 1);

    CHECK(ApplyExpiry(Card, Today));

    CHECK(Card.LeitnerStage == 2);
    CHECK(Card.DueDate == Today);
}

TEST_CASE("With the box penalty off a missed day keeps the card's box and leaves it due", "[schedule]")
{
    RulesGuard Guard;
    FTSettings::Get().SetMissedReviewDemotes(false);

    FTCard Card = MakeCardInStage(3, Today - 5, Today - 1);

    CHECK_FALSE(ApplyExpiry(Card, Today));

    CHECK(Card.LeitnerStage == 3);
    CHECK(IsDue(Card, Today));
}

TEST_CASE("With the box penalty off a stale first-box card still returns to the pool", "[schedule]")
{
    RulesGuard Guard;
    FTSettings::Get().SetMissedReviewDemotes(false);
    FTSettings::Get().SetMaximumDaysInFirstStage(1);

    FTCard Card = MakeCardInStage(FirstLadderStage, Today - 5, Today - 1);

    CHECK(ApplyExpiry(Card, Today));

    CHECK(IsInPool(Card));
}

TEST_CASE("A card punished for missing its date is still owed a review today", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(3, Today - 30, Today - 20);

    CHECK(ApplyExpiry(Card, Today));

    CHECK(Card.LeitnerStage == 2);
    CHECK(IsDue(Card, Today));
    CHECK(Card.DueDate == Today);
}

TEST_CASE("Applying expiry twice on the same day moves the card once", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(3, Today - 5, Today - 1);

    CHECK(ApplyExpiry(Card, Today));
    CHECK_FALSE(ApplyExpiry(Card, Today));

    CHECK(Card.LeitnerStage == 2);
}

TEST_CASE("A card that is merely due is not expired and does not move", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(3, Today - 4, Today);

    CHECK_FALSE(ApplyExpiry(Card, Today));

    CHECK(Card.LeitnerStage == 3);
    CHECK(Card.DueDate == Today);
}

TEST_CASE("Longer than three days in the first stage returns the card to the pool", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(1, Today - 4, Today - 3);

    CHECK(ApplyExpiry(Card, Today));

    CHECK(IsInPool(Card));
    CHECK_FALSE(Card.DueDate.has_value());
}

TEST_CASE("Exactly three days in the first stage does not reach the pool", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(1, Today - 3, Today - 2);

    CHECK(ApplyExpiry(Card, Today));

    CHECK(Card.LeitnerStage == FirstLadderStage);
    CHECK(Card.DueDate == Today);
}

TEST_CASE("The days allowed in the first stage is a setting", "[schedule]")
{
    RulesGuard Guard;
    FTSettings::Get().SetMaximumDaysInFirstStage(1);

    FTCard Card = MakeCardInStage(1, Today - 2, Today - 1);

    CHECK(ApplyExpiry(Card, Today));

    CHECK(IsInPool(Card));
}

TEST_CASE("Neither a pool card nor a completed one ever expires", "[schedule]")
{
    RulesGuard Guard;
    FTCard Pooled = MakeCard();
    FTCard Done = MakeCardInStage(GetLastStage(), Today - 16, Today);
    Promote(Done, Today);

    CHECK_FALSE(ApplyExpiry(Pooled, Today + 500));
    CHECK_FALSE(ApplyExpiry(Done, Today + 500));

    CHECK(IsInPool(Pooled));
    CHECK(IsCompleted(Done));
}

TEST_CASE("A card in a box with no due date does not expire", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCard();
    Card.LeitnerStage = 2;
    Card.BoxChangedDate = Today - 30;

    CHECK_FALSE(ApplyExpiry(Card, Today));
    CHECK_FALSE(IsDue(Card, Today));
    CHECK_FALSE(IsExpired(Card, Today));
    CHECK(Card.LeitnerStage == 2);
}

TEST_CASE("Custom intervals are used for both promotion and demotion", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCard();

    Promote(Card, Today);
    CHECK(Card.DueDate == Today + 1);

    Promote(Card, Today + 1);
    CHECK(Card.LeitnerStage == 2);

    SetIntervals({1, 3, 5, 7, 10});

    Promote(Card, Today + 4);
    CHECK(Card.LeitnerStage == 3);
    CHECK(Card.DueDate == Today + 4 + 5);

    CHECK(GetLastStage() == 5);
}

TEST_CASE("An empty ladder is refused rather than dividing the schedule by nothing", "[schedule]")
{
    RulesGuard Guard;

    FTSettings::Get().SetIntervalDays({});

    FTCard Card = MakeCard();

    Promote(Card, Today);

    CHECK_FALSE(IsInPool(Card));
    CHECK(GetLastStage() == static_cast<int>(FTScheduleRules{}.IntervalDays.size()));
}

TEST_CASE("A user-supplied ladder is sanitised", "[schedule]")
{
    FTScheduleRules Rules;
    Rules.IntervalDays = {0, -3, 2, 900, 5000};

    Sanitise(Rules);

    CHECK(Rules.IntervalDays == std::vector<int>{1, 1, 2, 900, MaximumIntervalDays});
}

TEST_CASE("An emptied ladder falls back to the default rather than stopping the schedule", "[schedule]")
{
    FTScheduleRules Rules;
    Rules.IntervalDays.clear();

    Sanitise(Rules);

    CHECK_FALSE(Rules.IntervalDays.empty());
    CHECK(Rules.IntervalDays == FTScheduleRules{}.IntervalDays);
}

TEST_CASE("A ladder longer than the cap is trimmed", "[schedule]")
{
    FTScheduleRules Rules;
    Rules.IntervalDays.assign(40, 3);

    Sanitise(Rules);

    CHECK(static_cast<int>(Rules.IntervalDays.size()) == MaximumLadderStages);
}

TEST_CASE("The days allowed in the first stage is sanitised too", "[schedule]")
{
    FTScheduleRules Rules;
    Rules.MaximumDaysInFirstStage = -5;

    Sanitise(Rules);

    CHECK(Rules.MaximumDaysInFirstStage >= 1);
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

TEST_CASE("Shortening the ladder completes a card that was above the new end", "[schedule]")
{
    RulesGuard Guard;
    SetIntervals({1, 2, 4});

    FTCard Card = MakeCardInStage(5, Today - 16, Today);

    Promote(Card, Today);

    CHECK(IsCompleted(Card));
}

TEST_CASE("Shortening the ladder demotes a stranded card into the new ladder", "[schedule]")
{
    RulesGuard Guard;
    SetIntervals({1, 2, 4});

    FTCard Card = MakeCardInStage(5, Today - 16, Today);

    Demote(Card, Today);

    CHECK(Card.LeitnerStage == 2);
    CHECK(Card.DueDate == Today + GetIntervalDays(2));
}

TEST_CASE("A stranded card is still due and still expires", "[schedule]")
{
    RulesGuard Guard;

    FTCard Card = MakeCardInStage(5, Today - 20, Today - 1);

    CHECK(IsDue(Card, Today));
    CHECK(IsExpired(Card, Today));

    SetIntervals({1, 2, 4});

    CHECK(ApplyExpiry(Card, Today));

    CHECK(Card.LeitnerStage == 2);
}

TEST_CASE("Lengthening the ladder leaves completed cards completed", "[schedule]")
{
    RulesGuard Guard;
    FTCard Card = MakeCardInStage(GetLastStage(), Today - 16, Today);
    Promote(Card, Today);
    REQUIRE(IsCompleted(Card));

    SetIntervals({1, 2, 4, 8, 16, 32, 64});

    Promote(Card, Today + 1);

    CHECK(IsCompleted(Card));
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
