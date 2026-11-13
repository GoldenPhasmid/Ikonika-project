#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "IntensiveLearningController.h"

#include "AppDatabase.h"
#include "AppState.h"
#include "TestAppSpeech.h"
#include "AudioTranscriptionController.h"
#include "FlashcardController.h"
#include "FlipRecallController.h"
#include "MultipleChoiceController.h"
#include "SpeedMatchController.h"
#include "TestAppDatabase.h"
#include "WrittenProductionController.h"

#include "core/FTCard.h"
#include "SessionSettingsGuard.h"
#include "SessionSettings.h"
#include "core/FTSettings.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <string>

namespace
{
    struct FQuestionSideGuard
    {
        ~FQuestionSideGuard()
        {
            FTSettings::Get().SetQuestionSide(SHOW_RANDOM);
        }
    };

    using FIntensiveRulesGuard = FSessionSettingsGuard;

    struct FSeededSession
    {
        std::string SessionKey;
        QStringList CardIds;
    };

    FSeededSession SeedSession(int CardCount)
    {
        static int Counter = 0;

        FSeededSession Result;
        Result.SessionKey = "session-" + std::to_string(Counter);

        for (int Index = 0; Index < CardCount; ++Index)
        {
            ++Counter;

            FTCard Card;
            Card.TargetWord = "word" + std::to_string(Counter);
            Card.Translation = "translation" + std::to_string(Counter);

            const std::optional<std::string> CardId = AppDatabase::Get().GetCardRepository().AddCard(Card);
            REQUIRE(CardId.has_value());

            Result.CardIds.append(QString::fromStdString(*CardId));
        }

        return Result;
    }

    void CompleteFlashcardStage(IntensiveLearningController& Controller, int MaxSteps = 1000)
    {
        int Steps = 0;

        while (Controller.GetStage() == 1)
        {
            auto* Flashcard = qobject_cast<FlashcardController*>(Controller.GetActiveController());
            REQUIRE(Flashcard != nullptr);

            Flashcard->next();

            REQUIRE(++Steps < MaxSteps);
        }
    }

    void CompleteFlipRecallStage(IntensiveLearningController& Controller, int MaxSteps = 1000)
    {
        int Steps = 0;

        while (Controller.GetStage() == 2)
        {
            auto* Flip = qobject_cast<FlipRecallController*>(Controller.GetActiveController());
            REQUIRE(Flip != nullptr);

            Flip->advance();

            REQUIRE(++Steps < MaxSteps);
        }
    }

    void CompleteChoiceStage(IntensiveLearningController& Controller)
    {
        while (Controller.GetStage() == 3)
        {
            auto* Choice = qobject_cast<MultipleChoiceController*>(Controller.GetActiveController());
            REQUIRE(Choice != nullptr);

            Choice->choose(Choice->GetCorrectOptionIndex());
            Choice->advance();
        }
    }

    void AnswerMasteryLoopController(ExerciseControllerBase* Controller)
    {
        if (auto* Audio = qobject_cast<AudioTranscriptionController*>(Controller))
        {
            Audio->submitAnswer(Audio->GetPrompt());
            Audio->advance();
            return;
        }

        if (auto* Written = qobject_cast<WrittenProductionController*>(Controller))
        {
            Written->submitAnswer(Written->GetAnswer());
            Written->advance();
            return;
        }

        FAIL("Unexpected mastery loop controller type");
    }

    bool IsMasteryLoopController(const ExerciseControllerBase* Controller)
    {
        return qobject_cast<const AudioTranscriptionController*>(Controller) != nullptr
            || qobject_cast<const WrittenProductionController*>(Controller) != nullptr;
    }

    void CompleteMasteryLoopStage(IntensiveLearningController& Controller, int MaxSteps = 1000)
    {
        int Steps = 0;

        while (IsMasteryLoopController(Controller.GetActiveController()))
        {
            AnswerMasteryLoopController(Controller.GetActiveController());

            REQUIRE(++Steps < MaxSteps);
        }
    }

    int ExtractCardNumber(const QString& Text)
    {
        QString Digits;

        for (const QChar Character : Text)
        {
            if (Character.isDigit())
            {
                Digits.append(Character);
            }
        }

        return Digits.toInt();
    }

    int FindUnmatchedLeftRow(const SpeedMatchController& Controller)
    {
        const QList<bool> Matched = Controller.GetLeftMatched();

        for (int Row = 0; Row < Matched.size(); ++Row)
        {
            if (!Matched[Row])
            {
                return Row;
            }
        }

        return -1;
    }

    int FindUnmatchedRightRowForCard(const SpeedMatchController& Controller, int CardNumber)
    {
        const QStringList Items = Controller.GetRightItems();
        const QList<bool> Matched = Controller.GetRightMatched();

        for (int Row = 0; Row < Items.size(); ++Row)
        {
            if (!Matched[Row] && ExtractCardNumber(Items[Row]) == CardNumber)
            {
                return Row;
            }
        }

        return -1;
    }

    void CompleteSpeedMatchStage(IntensiveLearningController& Controller, int MaxSteps = 1000)
    {
        int Steps = 0;

        while (auto* Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController()))
        {
            while (Match->IsAwaitingAnswer())
            {
                const int LeftRow = FindUnmatchedLeftRow(*Match);
                REQUIRE(LeftRow >= 0);

                const int CardNumber = ExtractCardNumber(Match->GetLeftItems()[LeftRow]);
                const int RightRow = FindUnmatchedRightRowForCard(*Match, CardNumber);
                REQUIRE(RightRow >= 0);

                Match->tapLeft(LeftRow);
                Match->tapRight(RightRow);
            }

            Match->advance();

            REQUIRE(++Steps < MaxSteps);
        }
    }
}

TEST_CASE("startCards fails when none of the card ids resolve", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;

    CHECK_FALSE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), {"missing-1", "missing-2"}));
}

TEST_CASE("startCards builds a session and enters stage 1 with a flashcard controller", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;

    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));
    CHECK(Controller.HasActiveSession());
    CHECK(Controller.GetSessionWordCount() == 3);
    CHECK(Controller.GetStage() == 1);
    CHECK(qobject_cast<FlashcardController*>(Controller.GetActiveController()) != nullptr);
}

TEST_CASE("resume picks a session back up after another session ran in between", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession First = SeedSession(3);
    const FSeededSession Second = SeedSession(3);

    IntensiveLearningController Controller;

    REQUIRE(Controller.startCards(QString::fromStdString(First.SessionKey), First.CardIds));

    auto* FlashcardA = qobject_cast<FlashcardController*>(Controller.GetActiveController());
    REQUIRE(FlashcardA != nullptr);
    FlashcardA->next();

    REQUIRE(Controller.startCards(QString::fromStdString(Second.SessionKey), Second.CardIds));
    CHECK(Controller.GetSessionWordCount() == 3);

    REQUIRE(Controller.resume(QString::fromStdString(First.SessionKey)));
    CHECK(Controller.GetStage() == 1);

    auto* ResumedFlashcard = qobject_cast<FlashcardController*>(Controller.GetActiveController());
    REQUIRE(ResumedFlashcard != nullptr);
    CHECK(ResumedFlashcard->GetCurrentSessionCount() >= 1);
}

TEST_CASE("A session parked mid mastery loop resumes at the same stage and still promotes its cards",
         "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession First = SeedSession(3);
    const FSeededSession Second = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(First.SessionKey), First.CardIds));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    REQUIRE(Controller.GetStage() == 4);

    AnswerMasteryLoopController(Controller.GetActiveController());

    REQUIRE(Controller.startCards(QString::fromStdString(Second.SessionKey), Second.CardIds));
    REQUIRE(Controller.GetStage() == 1);

    REQUIRE(Controller.resume(QString::fromStdString(First.SessionKey)));
    CHECK(Controller.GetStage() == 4);

    CompleteMasteryLoopStage(Controller);
    REQUIRE(Controller.GetStage() == 5);

    CompleteSpeedMatchStage(Controller);
    CHECK(Controller.IsFinished());

    for (const QString& CardId : First.CardIds)
    {
        const std::optional<FTCardHandle> Card = AppDatabase::Get().GetCardRepository().LoadCard(CardId.toStdString());
        REQUIRE(Card.has_value());
        CHECK((*Card)->LeitnerStage > PoolStage);
    }
}

TEST_CASE("endSession clears the checkpoint so a later startCards call does not resume it", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    {
        IntensiveLearningController Controller;
        REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

        CompleteFlashcardStage(Controller);
        REQUIRE(Controller.GetStage() == 2);

        Controller.endSession();
    }

    IntensiveLearningController Restarted;
    REQUIRE(Restarted.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));
    CHECK(Restarted.GetStage() == 1);
}

TEST_CASE("startCards is a no-op once a session for the same key is active", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    ExerciseControllerBase* FirstActive = Controller.GetActiveController();

    CHECK(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));
    CHECK(Controller.GetActiveController() == FirstActive);
}

TEST_CASE("endSession discards an active session", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    Controller.endSession();

    CHECK_FALSE(Controller.HasActiveSession());
}

TEST_CASE("Working through stage 1 advances to stage 2", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));
    REQUIRE(Controller.GetStage() == 1);

    CompleteFlashcardStage(Controller);

    CHECK(Controller.GetStage() == 2);
    CHECK(qobject_cast<FlipRecallController*>(Controller.GetActiveController()) != nullptr);
}

TEST_CASE("A full intensive learning session runs through all five stages and ends", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    bool bSessionEnded = false;
    QString EndedKey;
    QObject::connect(&Controller, &IntensiveLearningController::sessionEnded, &Controller,
        [&](const QString& InKey)
        {
            bSessionEnded = true;
            EndedKey = InKey;
        });

    CompleteFlashcardStage(Controller);
    REQUIRE(Controller.GetStage() == 2);

    CompleteFlipRecallStage(Controller);
    REQUIRE(Controller.GetStage() == 3);

    CompleteChoiceStage(Controller);
    REQUIRE(Controller.GetStage() == 4);

    CompleteMasteryLoopStage(Controller);
    REQUIRE(Controller.GetStage() == 5);

    CompleteSpeedMatchStage(Controller);

    CHECK(Controller.IsFinished());
    CHECK(bSessionEnded);
    CHECK(EndedKey == QString::fromStdString(Seeded.SessionKey));
}

TEST_CASE("The mastery loop stage counts the answers given towards mastering every card",
         "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    REQUIRE(Controller.GetStage() == 4);

    ExerciseControllerBase* Active = Controller.GetActiveController();
    REQUIRE(Active != nullptr);

    const int AnswerTarget = 3 * SessionSettings::Training().GetRepeatsToMaster();

    CHECK(Active->GetMaxSessionCount() == AnswerTarget);
    CHECK(Active->GetCurrentSessionCount() == 0);

    AnswerMasteryLoopController(Active);

    Active = Controller.GetActiveController();
    REQUIRE(Active != nullptr);

    CHECK(Active->GetMaxSessionCount() == AnswerTarget);
    CHECK(Active->GetCurrentSessionCount() == 1);
}

TEST_CASE("Completing a session promotes every card out of the pool", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    for (const QString& CardId : Seeded.CardIds)
    {
        const std::optional<FTCardHandle> Card = AppDatabase::Get().GetCardRepository().LoadCard(CardId.toStdString());
        REQUIRE(Card.has_value());
        CHECK((*Card)->LeitnerStage == PoolStage);
    }

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    CompleteMasteryLoopStage(Controller);
    CompleteSpeedMatchStage(Controller);

    REQUIRE(Controller.IsFinished());

    for (const QString& CardId : Seeded.CardIds)
    {
        const std::optional<FTCardHandle> Card = AppDatabase::Get().GetCardRepository().LoadCard(CardId.toStdString());
        REQUIRE(Card.has_value());
        CHECK((*Card)->LeitnerStage > PoolStage);
    }
}

TEST_CASE("Removing a card mid-session excludes it from the final promotion", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    REQUIRE(Controller.removeCard(Seeded.CardIds[0]));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    CompleteMasteryLoopStage(Controller);
    CompleteSpeedMatchStage(Controller);

    REQUIRE(Controller.IsFinished());

    const std::optional<FTCardHandle> RemovedCard =
        AppDatabase::Get().GetCardRepository().LoadCard(Seeded.CardIds[0].toStdString());
    REQUIRE(RemovedCard.has_value());
    CHECK((*RemovedCard)->LeitnerStage == PoolStage);

    for (int Index = 1; Index < Seeded.CardIds.size(); ++Index)
    {
        const std::optional<FTCardHandle> Card =
            AppDatabase::Get().GetCardRepository().LoadCard(Seeded.CardIds[Index].toStdString());
        REQUIRE(Card.has_value());
        CHECK((*Card)->LeitnerStage > PoolStage);
    }
}

TEST_CASE("Removing the currently shown card in a stage moves on without getting stuck",
         "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    auto* Flashcard = qobject_cast<FlashcardController*>(Controller.GetActiveController());
    REQUIRE(Flashcard != nullptr);

    const FTCard* Shown = Flashcard->GetCurrentCard();
    REQUIRE(Shown != nullptr);

    REQUIRE(Controller.removeCard(QString::fromStdString(Shown->Id)));

    if (Flashcard->GetCurrentCard() != nullptr)
    {
        CHECK(Flashcard->GetCurrentCard()->Id != Shown->Id);
    }

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    CompleteMasteryLoopStage(Controller);
    CompleteSpeedMatchStage(Controller);

    REQUIRE(Controller.IsFinished());
}

TEST_CASE("A card removed mid-session stays excluded after the app restarts and resumes",
         "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    {
        IntensiveLearningController Controller;
        REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));
        REQUIRE(Controller.removeCard(Seeded.CardIds[0]));
    }

    IntensiveLearningController Resumed;
    REQUIRE(Resumed.resume(QString::fromStdString(Seeded.SessionKey)));

    CompleteFlashcardStage(Resumed);
    CompleteFlipRecallStage(Resumed);
    CompleteChoiceStage(Resumed);
    CompleteMasteryLoopStage(Resumed);
    CompleteSpeedMatchStage(Resumed);

    REQUIRE(Resumed.IsFinished());

    const std::optional<FTCardHandle> RemovedCard =
        AppDatabase::Get().GetCardRepository().LoadCard(Seeded.CardIds[0].toStdString());
    REQUIRE(RemovedCard.has_value());
    CHECK((*RemovedCard)->LeitnerStage == PoolStage);

    for (int Index = 1; Index < Seeded.CardIds.size(); ++Index)
    {
        const std::optional<FTCardHandle> Card =
            AppDatabase::Get().GetCardRepository().LoadCard(Seeded.CardIds[Index].toStdString());
        REQUIRE(Card.has_value());
        CHECK((*Card)->LeitnerStage > PoolStage);
    }
}

TEST_CASE("A session restored at app launch speaks nothing", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    std::vector<std::pair<std::string, std::string>> Stored;

    {
        IntensiveLearningController Controller;
        REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

        for (const auto& [SessionKey, Json] : AppState::Get().IntensiveLearningCheckpoints)
        {
            Stored.emplace_back(SessionKey.toStdString(), Json.toStdString());
        }

        AppState::Get().IntensiveLearningCheckpoints.clear();
    }

    const int SpokenBeforeLaunch = SpeechGuard.Mock.SpeakCount;
    REQUIRE(SpokenBeforeLaunch > 0);
    REQUIRE_FALSE(Stored.empty());

    IntensiveLearningController Restored;
    Restored.RestoreCheckpoints(Stored);

    REQUIRE(Restored.lastActiveKey() == QString::fromStdString(Seeded.SessionKey));
    CHECK(SpeechGuard.Mock.SpeakCount == SpokenBeforeLaunch);
}

TEST_CASE("completeStage is a no-op when there is no active session", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;

    IntensiveLearningController Controller;

    Controller.completeStage();

    CHECK(Controller.GetStage() == 0);
    CHECK_FALSE(Controller.HasActiveSession());
}

TEST_CASE("completeStage skips the current stage without finishing all of its cards", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));
    REQUIRE(Controller.GetStage() == 1);

    Controller.completeStage();

    CHECK(Controller.GetStage() == 2);
    CHECK(qobject_cast<FlipRecallController*>(Controller.GetActiveController()) != nullptr);
}

TEST_CASE("completeStage on the last stage finishes the session and promotes every card",
         "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    bool bSessionEnded = false;
    QObject::connect(&Controller, &IntensiveLearningController::sessionEnded, &Controller,
        [&](const QString&) { bSessionEnded = true; });

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    CompleteMasteryLoopStage(Controller);
    REQUIRE(Controller.GetStage() == 5);

    Controller.completeStage();

    CHECK(Controller.IsFinished());
    CHECK(bSessionEnded);

    for (const QString& CardId : Seeded.CardIds)
    {
        const std::optional<FTCardHandle> Card = AppDatabase::Get().GetCardRepository().LoadCard(CardId.toStdString());
        REQUIRE(Card.has_value());
        CHECK((*Card)->LeitnerStage > PoolStage);
    }
}

TEST_CASE("restartStage is a no-op when there is no active session", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;

    IntensiveLearningController Controller;

    Controller.restartStage();

    CHECK(Controller.GetStage() == 0);
}

TEST_CASE("restartStage resets progress on the current stage without advancing", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    auto* Flashcard = qobject_cast<FlashcardController*>(Controller.GetActiveController());
    REQUIRE(Flashcard != nullptr);

    Flashcard->next();

    REQUIRE(Controller.GetStage() == 1);
    REQUIRE(Controller.GetActiveController()->GetCurrentSessionCount() == 2);

    Controller.restartStage();

    CHECK(Controller.GetStage() == 1);
    CHECK(Controller.GetActiveController()->GetCurrentSessionCount() == 1);
}

TEST_CASE("The flip recall stage ends once every card was known FlipCount times", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    CompleteFlashcardStage(Controller);
    REQUIRE(Controller.GetStage() == 2);

    auto* Flip = qobject_cast<FlipRecallController*>(Controller.GetActiveController());
    REQUIRE(Flip != nullptr);

    int Answers = 0;

    while (Controller.GetStage() == 2)
    {
        Flip->advance();
        Flip->advance();

        REQUIRE(++Answers < 1000);
    }

    const int Expected = 3 * SessionSettings::Training().GetFlipCount();
    CHECK(Answers == Expected);
}

TEST_CASE("A forgotten flip card costs a known answer and keeps the stage going", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    CompleteFlashcardStage(Controller);
    REQUIRE(Controller.GetStage() == 2);

    auto* Flip = qobject_cast<FlipRecallController*>(Controller.GetActiveController());
    REQUIRE(Flip != nullptr);

    Flip->advance();
    Flip->answerYes();
    REQUIRE(Flip->GetCurrentSessionCount() == 1);

    int Answers = 1;
    bool bForgot = false;

    while (Controller.GetStage() == 2)
    {
        Flip->advance();

        if (!bForgot && Flip->GetCardMasteryCount() == 1)
        {
            Flip->answerNo();
            bForgot = true;
        }
        else
        {
            Flip->answerYes();
        }

        REQUIRE(++Answers < 1000);
    }

    REQUIRE(bForgot);
    CHECK(Answers == 3 * SessionSettings::Training().GetFlipCount() + 2);
}

TEST_CASE("The speed match stage draws MatchCount rounds per card", "[intensivelearning]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    CompleteMasteryLoopStage(Controller);
    REQUIRE(Controller.GetStage() == 5);

    auto* Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController());
    REQUIRE(Match != nullptr);

    CompleteSpeedMatchStage(Controller);

    const int Expected = 3 * SessionSettings::Training().GetMatchCount();
    CHECK(Match->GetCurrentSessionCount() == Expected);
}

TEST_CASE("Disabling both written directions removes WrittenProductionController from the mastery loop",
         "[intensivelearning]")
{
    FIntensiveRulesGuard RulesGuard;
    SessionSettings::Training().SetGapExercise(false);
    SessionSettings::Training().SetTypeWord(false);

    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    REQUIRE(Controller.GetStage() == 4);

    int Steps = 0;

    while (Controller.GetStage() == 4)
    {
        ExerciseControllerBase* Active = Controller.GetActiveController();
        REQUIRE(Active != nullptr);
        CHECK(qobject_cast<WrittenProductionController*>(Active) == nullptr);

        AnswerMasteryLoopController(Active);

        REQUIRE(++Steps < 1000);
    }
}

TEST_CASE("The flip recall stage shows each card's word first and its translation second whatever QuestionSide is",
         "[intensivelearning]")
{
    FQuestionSideGuard SideGuard;
    FTSettings::Get().SetQuestionSide(SHOW_TRANSLATION);

    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    CompleteFlashcardStage(Controller);
    REQUIRE(Controller.GetStage() == 2);

    int Iterations = 0;
    int WordFirst = 0;
    int TranslationSecond = 0;

    while (Controller.GetStage() == 2)
    {
        auto* Flip = qobject_cast<FlipRecallController*>(Controller.GetActiveController());
        REQUIRE(Flip != nullptr);

        if (!Flip->IsAnswered())
        {
            const int KnownCount = Flip->GetCardMasteryCount();

            if (KnownCount == 0)
            {
                CHECK(Flip->ShowsTargetLanguage());
                ++WordFirst;
            }
            else if (KnownCount == 1)
            {
                CHECK_FALSE(Flip->ShowsTargetLanguage());
                ++TranslationSecond;
            }
        }

        Flip->advance();

        REQUIRE(++Iterations < 1000);
    }

    CHECK(WordFirst == 3);
    CHECK(TranslationSecond == 3);
}

TEST_CASE("The choice stage always shows the translation when QuestionSide is SHOW_TRANSLATION",
         "[intensivelearning]")
{
    FSessionSettingsGuard SideGuard;
    SessionSettings::Training().SetChoiceSide(SHOW_TRANSLATION);

    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    REQUIRE(Controller.GetStage() == 3);

    while (Controller.GetStage() == 3)
    {
        auto* Choice = qobject_cast<MultipleChoiceController*>(Controller.GetActiveController());
        REQUIRE(Choice != nullptr);
        CHECK_FALSE(Choice->ShowsTargetLanguage());

        Choice->choose(Choice->GetCorrectOptionIndex());
        Choice->advance();
    }
}

TEST_CASE("Turning off the choice and match stages leaves them out of a training session", "[intensivelearning]")
{
    FIntensiveRulesGuard RulesGuard;
    SessionSettings::Training().SetChoiceStage(false);
    SessionSettings::Training().SetMatchStage(false);

    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(Seeded.SessionKey), Seeded.CardIds));
    CHECK(Controller.GetStageCount() == 3);

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    REQUIRE(Controller.GetStage() == 3);
    CHECK(IsMasteryLoopController(Controller.GetActiveController()));

    CompleteMasteryLoopStage(Controller);

    CHECK(Controller.IsFinished());
}

TEST_CASE("A training session parked in the mastery loop resumes there after the choice stage is turned off",
         "[intensivelearning][checkpoint]")
{
    FIntensiveRulesGuard RulesGuard;
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession First = SeedSession(3);
    const FSeededSession Second = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(First.SessionKey), First.CardIds));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    REQUIRE(Controller.GetStage() == 4);

    AnswerMasteryLoopController(Controller.GetActiveController());

    REQUIRE(Controller.startCards(QString::fromStdString(Second.SessionKey), Second.CardIds));

    SessionSettings::Training().SetChoiceStage(false);

    REQUIRE(Controller.resume(QString::fromStdString(First.SessionKey)));
    CHECK(Controller.GetStage() == 3);
    CHECK(IsMasteryLoopController(Controller.GetActiveController()));

    CompleteMasteryLoopStage(Controller);
    REQUIRE(Controller.GetStage() == 4);
    REQUIRE(qobject_cast<SpeedMatchController*>(Controller.GetActiveController()) != nullptr);

    CompleteSpeedMatchStage(Controller);
    CHECK(Controller.IsFinished());
}

TEST_CASE("A training session parked in the choice stage resumes at the next stage once choice is turned off",
         "[intensivelearning][checkpoint]")
{
    FIntensiveRulesGuard RulesGuard;
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession First = SeedSession(3);
    const FSeededSession Second = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(First.SessionKey), First.CardIds));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    REQUIRE(Controller.GetStage() == 3);

    auto* Choice = qobject_cast<MultipleChoiceController*>(Controller.GetActiveController());
    REQUIRE(Choice != nullptr);
    Choice->choose(Choice->GetCorrectOptionIndex());
    Choice->advance();

    REQUIRE(Controller.startCards(QString::fromStdString(Second.SessionKey), Second.CardIds));

    SessionSettings::Training().SetChoiceStage(false);

    REQUIRE(Controller.resume(QString::fromStdString(First.SessionKey)));
    CHECK(Controller.GetStage() == 3);
    CHECK(IsMasteryLoopController(Controller.GetActiveController()));
}

TEST_CASE("A training session parked in the speed match stage completes once speed match is turned off",
         "[intensivelearning][checkpoint]")
{
    FIntensiveRulesGuard RulesGuard;
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession First = SeedSession(3);
    const FSeededSession Second = SeedSession(3);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QString::fromStdString(First.SessionKey), First.CardIds));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    CompleteMasteryLoopStage(Controller);
    REQUIRE(Controller.GetStage() == 5);

    REQUIRE(Controller.startCards(QString::fromStdString(Second.SessionKey), Second.CardIds));

    SessionSettings::Training().SetMatchStage(false);

    REQUIRE(Controller.resume(QString::fromStdString(First.SessionKey)));
    CHECK(Controller.IsFinished());

    for (const QString& CardId : First.CardIds)
    {
        const std::optional<FTCardHandle> Card = AppDatabase::Get().GetCardRepository().LoadCard(CardId.toStdString());
        REQUIRE(Card.has_value());
        CHECK((*Card)->LeitnerStage > PoolStage);
    }
}

TEST_CASE("A training checkpoint saved before the choice stage could be turned off resumes in the mastery loop",
         "[intensivelearning][checkpoint]")
{
    FIntensiveRulesGuard RulesGuard;
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession First = SeedSession(3);
    const FSeededSession Second = SeedSession(3);
    const QString FirstKey = QString::fromStdString(First.SessionKey);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(FirstKey, First.CardIds));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    REQUIRE(Controller.GetStage() == 4);

    REQUIRE(Controller.startCards(QString::fromStdString(Second.SessionKey), Second.CardIds));

    QJsonObject Saved = QJsonDocument::fromJson(AppState::Get().IntensiveLearningCheckpoints[FirstKey]).object();
    REQUIRE(Saved.contains("choiceStage"));

    for (const char* Key : {"choiceStage", "choiceStageCount", "matchStage", "matchStageCount"})
    {
        Saved.remove(QLatin1String(Key));
    }

    AppState::Get().IntensiveLearningCheckpoints[FirstKey] = QJsonDocument(Saved).toJson(QJsonDocument::Compact);

    SessionSettings::Training().SetChoiceStage(false);

    REQUIRE(Controller.resume(FirstKey));
    CHECK(Controller.GetStage() == 3);
    CHECK(IsMasteryLoopController(Controller.GetActiveController()));
}

TEST_CASE("An open training session drops the choice stage when it is entered again after choice is turned off",
         "[intensivelearning]")
{
    FIntensiveRulesGuard RulesGuard;
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession Seeded = SeedSession(3);
    const QString SessionKey = QString::fromStdString(Seeded.SessionKey);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(SessionKey, Seeded.CardIds));
    REQUIRE(Controller.GetStageCount() == 5);

    CompleteFlashcardStage(Controller);
    REQUIRE(Controller.GetStage() == 2);

    SessionSettings::Training().SetChoiceStage(false);

    REQUIRE(Controller.resume(SessionKey));
    CHECK(Controller.GetStageCount() == 4);
    CHECK(Controller.GetStage() == 2);
    REQUIRE(qobject_cast<FlipRecallController*>(Controller.GetActiveController()) != nullptr);

    CompleteFlipRecallStage(Controller);
    REQUIRE(Controller.GetStage() == 3);
    CHECK(IsMasteryLoopController(Controller.GetActiveController()));
}

TEST_CASE("A training checkpoint saved with two separate sentence stages resumes in the merged mastery stage",
         "[intensivelearning][checkpoint]")
{
    FIntensiveRulesGuard RulesGuard;
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const FSeededSession First = SeedSession(3);
    const FSeededSession Second = SeedSession(3);
    const QString FirstKey = QString::fromStdString(First.SessionKey);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(FirstKey, First.CardIds));

    CompleteFlashcardStage(Controller);
    CompleteFlipRecallStage(Controller);
    CompleteChoiceStage(Controller);
    REQUIRE(Controller.GetStage() == 4);

    REQUIRE(Controller.startCards(QString::fromStdString(Second.SessionKey), Second.CardIds));

    QJsonObject Legacy = QJsonDocument::fromJson(AppState::Get().IntensiveLearningCheckpoints[FirstKey]).object();
    REQUIRE(Legacy.contains("sentencePhaseCount"));

    Legacy.remove("sentencePhaseCount");
    Legacy.remove("sentencePhase");
    Legacy["sentenceStage"] = true;
    Legacy["sentenceStageCount"] = 2;

    constexpr int LegacySecondSentenceStage = 5;
    constexpr int LegacySpeedMatchStage = 6;

    SECTION("a checkpoint inside a sentence stage restarts the mastery stage")
    {
        Legacy["stageIndex"] = LegacySecondSentenceStage;
        AppState::Get().IntensiveLearningCheckpoints[FirstKey] = QJsonDocument(Legacy).toJson(QJsonDocument::Compact);

        REQUIRE(Controller.resume(FirstKey));
        CHECK(Controller.GetStage() == 4);
        CHECK(IsMasteryLoopController(Controller.GetActiveController()));
    }

    SECTION("a checkpoint at speed match stays at speed match")
    {
        Legacy["stageIndex"] = LegacySpeedMatchStage;
        AppState::Get().IntensiveLearningCheckpoints[FirstKey] = QJsonDocument(Legacy).toJson(QJsonDocument::Compact);

        REQUIRE(Controller.resume(FirstKey));
        CHECK(Controller.GetStage() == 5);
        CHECK(qobject_cast<SpeedMatchController*>(Controller.GetActiveController()) != nullptr);
    }
}
