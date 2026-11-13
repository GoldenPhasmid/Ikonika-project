#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "ExerciseLauncherController.h"

#include "AppDatabase.h"
#include "TestAppSpeech.h"
#include "TestAppDatabase.h"

#include "core/FTCard.h"
#include "core/FTSchedule.h"
#include "core/FTSettings.h"

#include <set>
#include <string>

namespace
{
    void SeedVocabulary(int CardCount)
    {
        std::vector<FTCard> Cards;
        Cards.reserve(static_cast<size_t>(CardCount));

        for (int Index = 0; Index < CardCount; ++Index)
        {
            FTCard Card;
            Card.TargetWord = "word" + std::to_string(Index);
            Card.Translation = "meaning" + std::to_string(Index);
            Cards.push_back(Card);
        }

        REQUIRE(AddTestCards(Cards));
    }

    void SeedThreeStages()
    {
        FTCard NotLearned;
        NotLearned.TargetWord = "chien";
        NotLearned.Translation = "dog";

        FTCard Learning;
        Learning.TargetWord = "chat";
        Learning.Translation = "cat";
        Learning.LeitnerStage = 2;

        FTCard Completed;
        Completed.TargetWord = "oiseau";
        Completed.Translation = "bird";
        Completed.LeitnerStage = GetLastStage();
        Completed.CompletedDate = 1000;

        REQUIRE(AddTestCards({NotLearned, Learning, Completed}));
    }

    std::set<std::string> PickedWords(const ExerciseLauncherController& Launcher)
    {
        std::set<std::string> Words;

        for (const FTCardHandle& Card : Launcher.GetActiveController()->GetSessionCardState()->GetCards())
        {
            Words.insert(Card->TargetWord);
        }

        return Words;
    }
}

TEST_CASE("An explicit repetition count overrides the settings default", "[exerciselauncher]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedVocabulary(20);

    ExerciseLauncherController Launcher;
    REQUIRE(Launcher.startExercise("flashcards", false, -1, 5));

    REQUIRE(Launcher.GetActiveController() != nullptr);
    CHECK(Launcher.GetActiveController()->GetMaxSessionCount() == 5);
}

TEST_CASE("An explicit card count is capped at the vocabulary's size", "[exerciselauncher]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedVocabulary(3);

    ExerciseLauncherController Launcher;
    REQUIRE(Launcher.startExercise("flashcards", false, 1000, 3));

    REQUIRE(Launcher.GetActiveController() != nullptr);
    CHECK(Launcher.GetActiveController()->GetSessionCardState()->GetCount() == 3);
}

TEST_CASE("Requesting more repetitions than cards recycles through the cards", "[exerciselauncher]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedVocabulary(20);

    ExerciseLauncherController Launcher;
    REQUIRE(Launcher.startExercise("flashcards", false, 3, 50));

    REQUIRE(Launcher.GetActiveController() != nullptr);
    CHECK(Launcher.GetActiveController()->GetSessionCardState()->GetCount() == 3);
    CHECK(Launcher.GetActiveController()->GetMaxSessionCount() == 50);
}

TEST_CASE("An explicit card count alone still falls back to the settings default for repetitions",
          "[exerciselauncher]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedVocabulary(20);
    const int ExpectedRepetitions = FTSettings::Get().GetIntensiveLearningRules().CardsPerExercise;

    ExerciseLauncherController Launcher;
    REQUIRE(Launcher.startExercise("flashcards", false, 4));

    REQUIRE(Launcher.GetActiveController() != nullptr);
    CHECK(Launcher.GetActiveController()->GetSessionCardState()->GetCount() == 4);
    CHECK(Launcher.GetActiveController()->GetMaxSessionCount() == ExpectedRepetitions);
}

TEST_CASE("A recycling session draws past the card count without finishing early", "[exerciselauncher]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedVocabulary(20);

    ExerciseLauncherController Launcher;
    REQUIRE(Launcher.startExercise("correctincorrect", false, 3, 10));

    ExerciseControllerBase* Controller = Launcher.GetActiveController();
    REQUIRE(Controller != nullptr);

    for (int Step = 0; Step < 6; ++Step)
    {
        Controller->advance();
    }

    CHECK_FALSE(Controller->IsFinished());
    CHECK(Controller->GetCurrentSessionCount() == 7);

    for (int Step = 0; Step < 10; ++Step)
    {
        Controller->advance();
    }

    CHECK(Controller->IsFinished());
}

TEST_CASE("Leaving the word count unset falls back to the settings default", "[exerciselauncher]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedVocabulary(20);
    const int Expected = std::min(FTSettings::Get().GetIntensiveLearningRules().CardsPerExercise, 20);

    ExerciseLauncherController Launcher;
    REQUIRE(Launcher.startExercise("flashcards"));

    REQUIRE(Launcher.GetActiveController() != nullptr);
    CHECK(Launcher.GetActiveController()->GetMaxSessionCount() == Expected);
}

TEST_CASE("Filter keys keep only the cards of the chosen stages", "[exerciselauncher]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedThreeStages();

    ExerciseLauncherController Launcher;
    REQUIRE(Launcher.startExercise("flashcards", false, -1, 3, {QStringLiteral("learning"), QStringLiteral("completed")}));

    REQUIRE(Launcher.GetActiveController() != nullptr);
    CHECK(Launcher.GetActiveController()->GetSessionCardState()->GetCount() == 2);
}

TEST_CASE("A filter that matches no card starts nothing", "[exerciselauncher]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedVocabulary(5);

    ExerciseLauncherController Launcher;
    CHECK_FALSE(Launcher.startExercise("flashcards", false, -1, 3, {QStringLiteral("completed")}));
    CHECK(Launcher.GetActiveController() == nullptr);
}

TEST_CASE("Each start picks its cards at random from the matching ones", "[exerciselauncher]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedVocabulary(40);

    ExerciseLauncherController Launcher;
    std::set<std::set<std::string>> Picks;

    for (int Start = 0; Start < 3; ++Start)
    {
        REQUIRE(Launcher.startExercise("flashcards", false, 5, 5));
        Picks.insert(PickedWords(Launcher));
    }

    CHECK(Picks.size() > 1);
}

TEST_CASE("The gap exercise starts nothing when no card has a sentence", "[exerciselauncher]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedVocabulary(5);

    ExerciseLauncherController Launcher;
    CHECK_FALSE(Launcher.startExercise("gap", false, -1, 3));
    CHECK(Launcher.GetActiveController() == nullptr);
}
