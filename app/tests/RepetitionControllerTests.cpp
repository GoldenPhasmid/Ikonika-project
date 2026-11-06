#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "RepetitionController.h"

#include "AppDatabase.h"
#include "TestAppSpeech.h"
#include "AudioTranscriptionController.h"
#include "CorrectIncorrectController.h"
#include "MultipleChoiceController.h"
#include "SpeedMatchController.h"
#include "TestAppDatabase.h"
#include "WrittenProductionController.h"

#include "core/FTCard.h"
#include "core/FTSchedule.h"
#include "SessionSettingsGuard.h"
#include "SessionSettings.h"
#include "core/FTSettings.h"

#include <QCoreApplication>

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

    struct FReviewChoiceGuard : FSessionSettingsGuard
    {
        FReviewChoiceGuard()
        {
            SessionSettings::Review().SetChoiceStage(true);
        }
    };

    std::string SeedCardDueToday(int Stage)
    {
        const FTDayNumber Today = GetTodayDayNumber();

        static int Counter = 0;
        ++Counter;

        FTCard Card;
        Card.TargetWord = "word" + std::to_string(Counter);
        Card.Translation = "translation" + std::to_string(Counter);

        const std::optional<std::string> CardId = AppDatabase::Get().GetCardRepository().AddCard(Card);
        REQUIRE(CardId.has_value());

        FTCard Stored = **AppDatabase::Get().GetCardRepository().LoadCard(*CardId);
        Stored.LeitnerStage = Stage;
        Stored.BoxChangedDate = Today - 4;
        Stored.DueDate = Today;

        REQUIRE(AppDatabase::Get().GetCardRepository().UpdateCardSchedule(Stored));

        return Stored.Id;
    }

    template <typename Predicate>
    void CompleteCorrectIncorrectStage(RepetitionController& Controller, Predicate ShouldSayYes)
    {
        while (Controller.GetStage() == 1)
        {
            auto* Review = qobject_cast<CorrectIncorrectController*>(Controller.GetActiveController());
            REQUIRE(Review != nullptr);

            const std::string CardId = Review->GetCurrentCard()->Id;

            Review->reveal();

            if (ShouldSayYes(CardId))
            {
                Review->answerYes();
            }
            else
            {
                Review->answerNo();
            }
        }
    }

    void CompleteCorrectIncorrectStage(RepetitionController& Controller)
    {
        CompleteCorrectIncorrectStage(Controller, [](const std::string&) { return true; });
    }

    void CompleteChoiceStage(RepetitionController& Controller)
    {
        while (Controller.GetStage() == 2)
        {
            auto* Choice = qobject_cast<MultipleChoiceController*>(Controller.GetActiveController());
            REQUIRE(Choice != nullptr);

            Choice->choose(Choice->GetCorrectOptionIndex());
            Choice->advance();
        }
    }

    void AnswerMasteryLoopController(ExerciseControllerBase* Controller, bool bCorrectly)
    {
        if (auto* Audio = qobject_cast<AudioTranscriptionController*>(Controller))
        {
            Audio->submitAnswer(bCorrectly ? Audio->GetPrompt() : QStringLiteral("wrong answer"));
            Audio->advance();
            return;
        }

        if (auto* Written = qobject_cast<WrittenProductionController*>(Controller))
        {
            Written->submitAnswer(bCorrectly ? Written->GetAnswer() : QStringLiteral("wrong answer"));
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

    template <typename Predicate>
    void CompleteMasteryLoopStage(RepetitionController& Controller, Predicate AnswerCorrectly, int MaxSteps = 1000)
    {
        int Steps = 0;

        while (IsMasteryLoopController(Controller.GetActiveController()))
        {
            ExerciseControllerBase* Active = Controller.GetActiveController();

            const FTCard* CurrentCard = Active->GetCurrentCard();
            REQUIRE(CurrentCard != nullptr);

            AnswerMasteryLoopController(Active, AnswerCorrectly(CurrentCard->Id));

            REQUIRE(++Steps < MaxSteps);
        }
    }

    void CompleteMasteryLoopStage(RepetitionController& Controller)
    {
        CompleteMasteryLoopStage(Controller, [](const std::string&) { return true; });
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

    void CompleteSpeedMatchStage(RepetitionController& Controller, int MaxSteps = 1000)
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

TEST_CASE("startDueToday returns false when nothing is due", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;

    RepetitionController Controller;

    CHECK_FALSE(Controller.startDueToday());
    CHECK_FALSE(Controller.HasActiveSession());
}

TEST_CASE("startDueToday builds a session from due cards and enters stage 1", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;

    REQUIRE(Controller.startDueToday());
    CHECK(Controller.HasActiveSession());
    CHECK(Controller.GetCardCount() == 3);
    CHECK(Controller.GetStage() == 1);

    auto* Active = qobject_cast<CorrectIncorrectController*>(Controller.GetActiveController());
    CHECK(Active != nullptr);
}

TEST_CASE("startDueToday is a no-op once a session is active", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    ExerciseControllerBase* FirstActive = Controller.GetActiveController();

    CHECK(Controller.startDueToday());
    CHECK(Controller.GetActiveController() == FirstActive);
}

TEST_CASE("startDueToday resumes a session parked by a previous controller instance", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);
    SeedCardDueToday(3);

    {
        RepetitionController Controller;
        REQUIRE(Controller.startDueToday());

        auto* Review = qobject_cast<CorrectIncorrectController*>(Controller.GetActiveController());
        REQUIRE(Review != nullptr);

        Review->reveal();
        Review->answerYes();
    }

    RepetitionController Resumed;

    REQUIRE(Resumed.startDueToday());
    CHECK(Resumed.HasActiveSession());
    CHECK(Resumed.GetStage() == 1);
    CHECK(Resumed.GetCardCount() == 3);

    auto* Active = qobject_cast<CorrectIncorrectController*>(Resumed.GetActiveController());
    REQUIRE(Active != nullptr);
    CHECK(Active->GetCurrentSessionCount() >= 1);
}

TEST_CASE("A session parked mid mastery loop resumes at the same stage and still promotes its cards",
         "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    const std::string CardId = SeedCardDueToday(3);

    {
        RepetitionController Controller;
        REQUIRE(Controller.startDueToday());

        CompleteCorrectIncorrectStage(Controller);
        CompleteChoiceStage(Controller);
        REQUIRE(Controller.GetStage() == 3);
    }

    RepetitionController Resumed;
    REQUIRE(Resumed.startDueToday());
    CHECK(Resumed.GetStage() == 3);

    CompleteMasteryLoopStage(Resumed);
    REQUIRE(Resumed.GetStage() == 4);

    CompleteSpeedMatchStage(Resumed);
    CHECK(Resumed.IsFinished());

    const std::optional<FTCardHandle> Reloaded = AppDatabase::Get().GetCardRepository().LoadCard(CardId);
    REQUIRE(Reloaded.has_value());
    CHECK((*Reloaded)->LeitnerStage == 4);
}

TEST_CASE("endSession clears the checkpoint so a later startDueToday call does not resume it",
         "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);

    {
        RepetitionController Controller;
        REQUIRE(Controller.startDueToday());

        CompleteCorrectIncorrectStage(Controller);
        REQUIRE(Controller.GetStage() == 2);

        Controller.endSession();
    }

    RepetitionController Restarted;
    REQUIRE(Restarted.startDueToday());
    CHECK(Restarted.GetStage() == 1);
}

TEST_CASE("endSession discards an active session", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    Controller.endSession();

    CHECK_FALSE(Controller.HasActiveSession());
}

TEST_CASE("Answering every card in stage 1 advances to stage 2", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());
    REQUIRE(Controller.GetStage() == 1);

    auto* Review = qobject_cast<CorrectIncorrectController*>(Controller.GetActiveController());
    REQUIRE(Review != nullptr);

    int Iterations = 0;
    while (Controller.GetStage() == 1)
    {
        Review->reveal();
        Review->answerYes();
        ++Iterations;
    }

    CHECK(Iterations == 2);
    CHECK(Controller.GetStage() == 2);
    CHECK(qobject_cast<MultipleChoiceController*>(Controller.GetActiveController()) != nullptr);
}

TEST_CASE("A single-card session does not skip stage 1 without the user ever answering it",
         "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    CHECK(Controller.GetStage() == 1);
}

TEST_CASE("A full repetition session runs through all four stages and ends", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    bool bSessionEnded = false;
    QObject::connect(&Controller, &RepetitionController::sessionEnded, &Controller,
        [&]() { bSessionEnded = true; });

    CompleteCorrectIncorrectStage(Controller);
    REQUIRE(Controller.GetStage() == 2);

    CompleteChoiceStage(Controller);
    REQUIRE(Controller.GetStage() == 3);

    CompleteMasteryLoopStage(Controller);
    REQUIRE(Controller.GetStage() == 4);

    CompleteSpeedMatchStage(Controller);

    CHECK(Controller.IsFinished());
    CHECK(bSessionEnded);
}

TEST_CASE("A card answered yes in stage 1 and never demoted ends up promoted", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    const std::string CardId = SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    CompleteCorrectIncorrectStage(Controller);
    CompleteChoiceStage(Controller);
    CompleteMasteryLoopStage(Controller);
    CompleteSpeedMatchStage(Controller);

    REQUIRE(Controller.IsFinished());

    const std::optional<FTCardHandle> Reloaded = AppDatabase::Get().GetCardRepository().LoadCard(CardId);
    REQUIRE(Reloaded.has_value());
    CHECK((*Reloaded)->LeitnerStage == 4);
}

TEST_CASE("A card answered no in stage 1 is not promoted", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    const std::string CardId = SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    CompleteCorrectIncorrectStage(Controller, [](const std::string&) { return false; });
    CompleteChoiceStage(Controller);
    CompleteMasteryLoopStage(Controller);
    CompleteSpeedMatchStage(Controller);

    REQUIRE(Controller.IsFinished());

    const std::optional<FTCardHandle> Reloaded = AppDatabase::Get().GetCardRepository().LoadCard(CardId);
    REQUIRE(Reloaded.has_value());
    CHECK((*Reloaded)->LeitnerStage < 3);
}

TEST_CASE("Removing a card before it is reviewed keeps it out of the outcomes", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    const std::string RemovedCardId = SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    REQUIRE(Controller.removeCard(QString::fromStdString(RemovedCardId)));

    CompleteCorrectIncorrectStage(Controller);
    CompleteChoiceStage(Controller);
    CompleteMasteryLoopStage(Controller);
    CompleteSpeedMatchStage(Controller);

    REQUIRE(Controller.IsFinished());

    const std::optional<FTCardHandle> Reloaded = AppDatabase::Get().GetCardRepository().LoadCard(RemovedCardId);
    REQUIRE(Reloaded.has_value());
    CHECK((*Reloaded)->LeitnerStage == 3);
}

TEST_CASE("A card removed mid-session stays excluded after the app restarts and resumes",
         "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    const std::string RemovedCardId = SeedCardDueToday(3);
    SeedCardDueToday(3);

    {
        RepetitionController Controller;
        REQUIRE(Controller.startDueToday());
        REQUIRE(Controller.removeCard(QString::fromStdString(RemovedCardId)));
    }

    RepetitionController Resumed;
    REQUIRE(Resumed.startDueToday());

    CompleteCorrectIncorrectStage(Resumed);
    CompleteChoiceStage(Resumed);
    CompleteMasteryLoopStage(Resumed);
    CompleteSpeedMatchStage(Resumed);

    REQUIRE(Resumed.IsFinished());

    const std::optional<FTCardHandle> Reloaded = AppDatabase::Get().GetCardRepository().LoadCard(RemovedCardId);
    REQUIRE(Reloaded.has_value());
    CHECK((*Reloaded)->LeitnerStage == 3);
}

TEST_CASE("completeStage is a no-op when there is no active session", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;

    RepetitionController Controller;

    Controller.completeStage();

    CHECK(Controller.GetStage() == 0);
    CHECK_FALSE(Controller.HasActiveSession());
}

TEST_CASE("completeStage skips the current stage without answering any of its cards", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());
    REQUIRE(Controller.GetStage() == 1);

    Controller.completeStage();

    CHECK(Controller.GetStage() == 2);

    auto* Choice = qobject_cast<MultipleChoiceController*>(Controller.GetActiveController());
    REQUIRE(Choice != nullptr);
    CHECK_FALSE(Choice->IsAnswered());
    CHECK(Choice->GetCorrectOptionIndex() >= 0);
}

TEST_CASE("completeStage on the last stage finishes the session and applies outcomes",
         "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    bool bSessionEnded = false;
    QObject::connect(&Controller, &RepetitionController::sessionEnded, &Controller,
        [&]() { bSessionEnded = true; });

    CompleteCorrectIncorrectStage(Controller);
    CompleteChoiceStage(Controller);
    CompleteMasteryLoopStage(Controller);
    REQUIRE(Controller.GetStage() == 4);

    Controller.completeStage();

    CHECK(Controller.IsFinished());
    CHECK(bSessionEnded);
}

TEST_CASE("Completing every stage early promotes the cards the first stage never asked about",
          "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    const std::string FirstSeededId = SeedCardDueToday(3);
    const std::string SecondSeededId = SeedCardDueToday(3);
    const std::string RemovedCardId = SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());
    REQUIRE(Controller.removeCard(QString::fromStdString(RemovedCardId)));

    auto* Review = qobject_cast<CorrectIncorrectController*>(Controller.GetActiveController());
    REQUIRE(Review != nullptr);

    const std::string AnsweredNoCardId = Review->GetCurrentCard()->Id;
    const std::string SkippedCardId = AnsweredNoCardId == FirstSeededId ? SecondSeededId : FirstSeededId;
    Review->reveal();
    Review->answerNo();

    while (!Controller.IsFinished())
    {
        Controller.completeStage();
    }

    const std::optional<FTCardHandle> AnsweredNo = AppDatabase::Get().GetCardRepository().LoadCard(AnsweredNoCardId);
    const std::optional<FTCardHandle> Skipped = AppDatabase::Get().GetCardRepository().LoadCard(SkippedCardId);
    const std::optional<FTCardHandle> Removed = AppDatabase::Get().GetCardRepository().LoadCard(RemovedCardId);
    REQUIRE(AnsweredNo.has_value());
    REQUIRE(Skipped.has_value());
    REQUIRE(Removed.has_value());

    CHECK((*AnsweredNo)->LeitnerStage < 3);
    CHECK((*Skipped)->LeitnerStage == 4);
    CHECK((*Removed)->LeitnerStage == 3);
}

TEST_CASE("restartStage is a no-op when there is no active session", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;

    RepetitionController Controller;

    Controller.restartStage();

    CHECK(Controller.GetStage() == 0);
}

TEST_CASE("restartStage resets progress on the current stage without advancing", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    auto* Review = qobject_cast<CorrectIncorrectController*>(Controller.GetActiveController());
    REQUIRE(Review != nullptr);

    Review->reveal();
    Review->answerYes();

    REQUIRE(Controller.GetStage() == 1);
    REQUIRE(Controller.GetActiveController()->GetCurrentSessionCount() == 2);

    Controller.restartStage();

    CHECK(Controller.GetStage() == 1);

    auto* RestartedReview = qobject_cast<CorrectIncorrectController*>(Controller.GetActiveController());
    REQUIRE(RestartedReview != nullptr);
    CHECK(RestartedReview->GetCurrentSessionCount() == 1);
    CHECK_FALSE(RestartedReview->IsAnswered());
}

TEST_CASE("The choice stage draws ChoiceCount rounds per card", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    CompleteCorrectIncorrectStage(Controller);
    REQUIRE(Controller.GetStage() == 2);

    auto* Choice = qobject_cast<MultipleChoiceController*>(Controller.GetActiveController());
    REQUIRE(Choice != nullptr);

    CompleteChoiceStage(Controller);

    const int Expected = 3 * SessionSettings::Review().GetChoiceCount();
    CHECK(Choice->GetCurrentSessionCount() == Expected);
}

TEST_CASE("The speed match stage draws MatchCount rounds per card", "[repetitioncontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    CompleteCorrectIncorrectStage(Controller);
    CompleteChoiceStage(Controller);
    CompleteMasteryLoopStage(Controller);
    REQUIRE(Controller.GetStage() == 4);

    auto* Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController());
    REQUIRE(Match != nullptr);

    CompleteSpeedMatchStage(Controller);

    const int Expected = 3 * SessionSettings::Review().GetMatchCount();
    CHECK(Match->GetCurrentSessionCount() == Expected);
}

TEST_CASE("The choice stage always shows the word when QuestionSide is SHOW_WORD", "[repetitioncontroller]")
{
    FSessionSettingsGuard SideGuard;
    SessionSettings::Review().SetChoiceSide(SHOW_WORD);

    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    CompleteCorrectIncorrectStage(Controller);
    REQUIRE(Controller.GetStage() == 2);

    while (Controller.GetStage() == 2)
    {
        auto* Choice = qobject_cast<MultipleChoiceController*>(Controller.GetActiveController());
        REQUIRE(Choice != nullptr);
        CHECK(Choice->ShowsTargetLanguage());

        Choice->choose(Choice->GetCorrectOptionIndex());
        Choice->advance();
    }
}

TEST_CASE("The choice stage always shows the translation when QuestionSide is SHOW_TRANSLATION",
         "[repetitioncontroller]")
{
    FSessionSettingsGuard SideGuard;
    SessionSettings::Review().SetChoiceSide(SHOW_TRANSLATION);

    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    CompleteCorrectIncorrectStage(Controller);
    REQUIRE(Controller.GetStage() == 2);

    while (Controller.GetStage() == 2)
    {
        auto* Choice = qobject_cast<MultipleChoiceController*>(Controller.GetActiveController());
        REQUIRE(Choice != nullptr);
        CHECK_FALSE(Choice->ShowsTargetLanguage());

        Choice->choose(Choice->GetCorrectOptionIndex());
        Choice->advance();
    }
}

TEST_CASE("Turning off the choice and match stages leaves them out of a review session", "[repetitioncontroller]")
{
    FSessionSettingsGuard SettingsGuard;
    SessionSettings::Review().SetChoiceStage(false);
    SessionSettings::Review().SetMatchStage(false);

    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());
    CHECK(Controller.GetStageCount() == 2);

    CompleteCorrectIncorrectStage(Controller);
    REQUIRE(Controller.GetStage() == 2);
    CHECK(IsMasteryLoopController(Controller.GetActiveController()));

    CompleteMasteryLoopStage(Controller);

    CHECK(Controller.IsFinished());
}

TEST_CASE("A review session parked in the mastery loop resumes there after the choice stage is turned off",
         "[repetitioncontroller][checkpoint]")
{
    FSessionSettingsGuard SettingsGuard;
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);

    {
        RepetitionController Controller;
        REQUIRE(Controller.startDueToday());

        CompleteCorrectIncorrectStage(Controller);
        CompleteChoiceStage(Controller);
        REQUIRE(Controller.GetStage() == 3);
    }

    SessionSettings::Review().SetChoiceStage(false);

    RepetitionController Resumed;
    REQUIRE(Resumed.startDueToday());
    CHECK(Resumed.GetStage() == 2);
    CHECK(IsMasteryLoopController(Resumed.GetActiveController()));

    CompleteMasteryLoopStage(Resumed);
    REQUIRE(Resumed.GetStage() == 3);
    REQUIRE(qobject_cast<SpeedMatchController*>(Resumed.GetActiveController()) != nullptr);

    CompleteSpeedMatchStage(Resumed);
    CHECK(Resumed.IsFinished());
}

TEST_CASE("An open review session drops the choice stage when it is entered again after choice is turned off",
         "[repetitioncontroller]")
{
    FSessionSettingsGuard SettingsGuard;
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FReviewChoiceGuard ChoiceGuard;

    SeedCardDueToday(3);
    SeedCardDueToday(3);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());
    REQUIRE(Controller.GetStageCount() == 4);

    SessionSettings::Review().SetChoiceStage(false);

    REQUIRE(Controller.startDueToday());
    CHECK(Controller.GetStageCount() == 3);
    CHECK(Controller.GetStage() == 1);

    CompleteCorrectIncorrectStage(Controller);
    REQUIRE(Controller.GetStage() == 2);
    CHECK(IsMasteryLoopController(Controller.GetActiveController()));
}

TEST_CASE("Review opens with a quick pass over cards from box 4 and up", "[repetitioncontroller][quickpass]")
{
    FSessionSettingsGuard SettingsGuard;
    SessionSettings::Review().SetChoiceStage(false);

    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const std::string Known = SeedCardDueToday(4);
    const std::string Forgotten = SeedCardDueToday(5);
    SeedCardDueToday(2);
    SeedCardDueToday(2);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());

    CHECK(Controller.IsQuickPass());
    CHECK(Controller.GetCardCount() == 2);
    CHECK(Controller.GetStageCount() == 1);

    CompleteCorrectIncorrectStage(Controller, [&Known](const std::string& CardId) { return CardId == Known; });
    REQUIRE(Controller.IsFinished());

    QCoreApplication::processEvents();

    REQUIRE(Controller.HasActiveSession());
    CHECK_FALSE(Controller.IsQuickPass());
    CHECK(Controller.GetCardCount() == 3);

    const std::optional<FTCardHandle> Promoted = AppDatabase::Get().GetCardRepository().LoadCard(Known);
    REQUIRE(Promoted.has_value());
    CHECK((*Promoted)->LeitnerStage == 5);

    int Reviewed = 0;

    while (Controller.GetStage() == 1)
    {
        auto* Review = qobject_cast<CorrectIncorrectController*>(Controller.GetActiveController());
        REQUIRE(Review != nullptr);
        CHECK(Review->GetCurrentCard()->Id != Forgotten);

        Review->reveal();
        Review->answerYes();

        REQUIRE(++Reviewed < 10);
    }

    CHECK(Reviewed == 2);

    CompleteMasteryLoopStage(Controller);
    CompleteSpeedMatchStage(Controller);
    REQUIRE(Controller.IsFinished());

    const std::optional<FTCardHandle> Demoted = AppDatabase::Get().GetCardRepository().LoadCard(Forgotten);
    REQUIRE(Demoted.has_value());
    CHECK((*Demoted)->LeitnerStage < 5);
}

TEST_CASE("Forgetting every quick pass card with nothing earlier due goes straight to the mastery loop",
          "[repetitioncontroller][quickpass]")
{
    FSessionSettingsGuard SettingsGuard;
    SessionSettings::Review().SetChoiceStage(false);

    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const std::string Forgotten = SeedCardDueToday(4);

    RepetitionController Controller;
    REQUIRE(Controller.startDueToday());
    REQUIRE(Controller.IsQuickPass());

    CompleteCorrectIncorrectStage(Controller, [](const std::string&) { return false; });

    QCoreApplication::processEvents();

    REQUIRE(Controller.HasActiveSession());
    CHECK(Controller.GetStage() == 2);
    CHECK(IsMasteryLoopController(Controller.GetActiveController()));

    CompleteMasteryLoopStage(Controller);
    CompleteSpeedMatchStage(Controller);
    REQUIRE(Controller.IsFinished());

    const std::optional<FTCardHandle> Demoted = AppDatabase::Get().GetCardRepository().LoadCard(Forgotten);
    REQUIRE(Demoted.has_value());
    CHECK((*Demoted)->LeitnerStage < 4);
}

TEST_CASE("Knowing every quick pass card with nothing earlier due ends the review", "[repetitioncontroller][quickpass]")
{
    FSessionSettingsGuard SettingsGuard;
    SessionSettings::Review().SetChoiceStage(false);

    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    SeedCardDueToday(4);
    SeedCardDueToday(6);

    RepetitionController Controller;

    bool bSessionEnded = false;
    QObject::connect(&Controller, &RepetitionController::sessionEnded, &Controller, [&]() { bSessionEnded = true; });

    REQUIRE(Controller.startDueToday());
    REQUIRE(Controller.IsQuickPass());

    CompleteCorrectIncorrectStage(Controller);

    QCoreApplication::processEvents();

    CHECK(Controller.IsFinished());
    CHECK_FALSE(Controller.HasActiveSession());
    CHECK(bSessionEnded);
}

TEST_CASE("A forgotten quick pass card stays forgotten when the review after it resumes", "[repetitioncontroller][quickpass][checkpoint]")
{
    FSessionSettingsGuard SettingsGuard;
    SessionSettings::Review().SetChoiceStage(false);

    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    const std::string Forgotten = SeedCardDueToday(4);
    SeedCardDueToday(2);

    {
        RepetitionController Controller;
        REQUIRE(Controller.startDueToday());

        CompleteCorrectIncorrectStage(Controller, [](const std::string&) { return false; });

        QCoreApplication::processEvents();

        REQUIRE(Controller.HasActiveSession());
        REQUIRE(Controller.GetStage() == 1);
    }

    RepetitionController Resumed;
    REQUIRE(Resumed.startDueToday());

    CHECK_FALSE(Resumed.IsQuickPass());
    CHECK(Resumed.GetCardCount() == 2);

    CompleteCorrectIncorrectStage(Resumed);
    CompleteMasteryLoopStage(Resumed);
    CompleteSpeedMatchStage(Resumed);
    REQUIRE(Resumed.IsFinished());

    const std::optional<FTCardHandle> Demoted = AppDatabase::Get().GetCardRepository().LoadCard(Forgotten);
    REQUIRE(Demoted.has_value());
    CHECK((*Demoted)->LeitnerStage < 4);
}