#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "SpeedMatchController.h"

#include "TestAppSpeech.h"

#include "core/FTSettings.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
    FTCard MakeCard(std::string TargetWord, std::string Translation)
    {
        FTCard Result;
        Result.Id = TargetWord;
        Result.TargetWord = std::move(TargetWord);
        Result.Translation = std::move(Translation);

        return Result;
    }

    std::vector<FTCardHandle> MakeCards(int Count)
    {
        std::vector<FTCardHandle> Cards;

        for (int Index = 0; Index < Count; ++Index)
        {
            Cards.push_back(MakeCardHandle(
                MakeCard("word" + std::to_string(Index), "translation" + std::to_string(Index))));
        }

        return Cards;
    }

    FTSessionStartParams MakeParams(std::vector<FTCardHandle> Cards, int PassCount = 1)
    {
        FTSessionStartParams Params{};
        Params.CardState = std::make_shared<FCardState>(std::move(Cards));
        Params.Title = "Test Session";
        Params.TargetLanguage = "fr";
        Params.FixedLength = Params.CardState->GetCount() * PassCount;
        Params.Selector = std::make_shared<FEachOnceSelector>(Params.CardState->GetCount(), true);

        return Params;
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

    int FindRightRowForCard(const SpeedMatchController& Controller, int CardNumber)
    {
        const QStringList Items = Controller.GetRightItems();

        for (int Row = 0; Row < Items.size(); ++Row)
        {
            if (ExtractCardNumber(Items[Row]) == CardNumber)
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

    int FindWrongRightRow(const SpeedMatchController& Controller, int LeftRow)
    {
        const int LeftCardNumber = ExtractCardNumber(Controller.GetLeftItems()[LeftRow]);
        const QStringList RightItems = Controller.GetRightItems();

        for (int Row = 0; Row < RightItems.size(); ++Row)
        {
            if (ExtractCardNumber(RightItems[Row]) != LeftCardNumber)
            {
                return Row;
            }
        }

        return -1;
    }

    void MatchAllPairs(SpeedMatchController& Controller)
    {
        while (Controller.IsAwaitingAnswer())
        {
            const int LeftRow = FindUnmatchedLeftRow(Controller);
            REQUIRE(LeftRow >= 0);

            const int CardNumber = ExtractCardNumber(Controller.GetLeftItems()[LeftRow]);
            const int RightRow = FindRightRowForCard(Controller, CardNumber);
            REQUIRE(RightRow >= 0);

            Controller.tapLeft(LeftRow);
            Controller.tapRight(RightRow);
        }
    }
}

TEST_CASE("A fresh speed match session presents the whole batch for matching", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK(Controller.GetLeftItems().size() == 5);
    CHECK(Controller.GetRightItems().size() == 5);
    CHECK(Controller.IsAwaitingAnswer());
    CHECK_FALSE(Controller.IsShowingFeedback());
    CHECK(Controller.GetWrongPairCount() == 0);
}

TEST_CASE("A deck smaller than the match pair count never repeats a card on one screen", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(3)));

    const QStringList LeftItems = Controller.GetLeftItems();
    const QStringList RightItems = Controller.GetRightItems();

    REQUIRE(LeftItems.size() == 3);
    REQUIRE(RightItems.size() == 3);

    for (int First = 0; First < LeftItems.size(); ++First)
    {
        for (int Second = First + 1; Second < LeftItems.size(); ++Second)
        {
            CHECK(LeftItems[First] != LeftItems[Second]);
            CHECK(RightItems[First] != RightItems[Second]);
        }
    }
}

TEST_CASE("Tapping the correct pair marks both rows matched", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int LeftRow = 0;
    const int CardNumber = ExtractCardNumber(Controller.GetLeftItems()[LeftRow]);
    const int RightRow = FindRightRowForCard(Controller, CardNumber);
    REQUIRE(RightRow >= 0);

    Controller.tapLeft(LeftRow);
    Controller.tapRight(RightRow);

    CHECK(Controller.GetLeftMatched()[LeftRow]);
    CHECK(Controller.GetRightMatched()[RightRow]);
    CHECK(Controller.GetWrongPairCount() == 0);
    CHECK(Controller.GetSelectedLeftRow() == -1);
    CHECK(Controller.GetSelectedRightRow() == -1);
}

TEST_CASE("Tapping a mismatched pair flashes both rows and counts as wrong", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int LeftRow = 0;
    const int WrongRightRow = FindWrongRightRow(Controller, LeftRow);
    REQUIRE(WrongRightRow >= 0);

    Controller.tapLeft(LeftRow);
    Controller.tapRight(WrongRightRow);

    CHECK_FALSE(Controller.GetLeftMatched()[LeftRow]);
    CHECK_FALSE(Controller.GetRightMatched()[WrongRightRow]);
    CHECK(Controller.GetFlashedLeftRow() == LeftRow);
    CHECK(Controller.GetFlashedRightRow() == WrongRightRow);
    CHECK(Controller.GetWrongPairCount() == 1);
}

TEST_CASE("Every attempted pair is graded, a match as correct and a mismatch as wrong", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    std::vector<bool> Grades;
    QObject::connect(&Controller, &ExerciseControllerBase::answerGraded,
                     [&Grades](bool bCorrect) { Grades.push_back(bCorrect); });

    const int WrongRightRow = FindWrongRightRow(Controller, 0);
    REQUIRE(WrongRightRow >= 0);

    Controller.tapLeft(0);
    Controller.tapRight(WrongRightRow);

    const int CardNumber = ExtractCardNumber(Controller.GetLeftItems()[0]);
    const int RightRow = FindRightRowForCard(Controller, CardNumber);
    REQUIRE(RightRow >= 0);

    Controller.tapLeft(0);
    Controller.tapRight(RightRow);

    CHECK(Grades == std::vector<bool>{false, true});
}

TEST_CASE("Selecting a new row clears the previous mismatch flash", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int WrongRightRow = FindWrongRightRow(Controller, 0);
    REQUIRE(WrongRightRow >= 0);

    Controller.tapLeft(0);
    Controller.tapRight(WrongRightRow);
    REQUIRE(Controller.GetFlashedLeftRow() == 0);

    Controller.tapLeft(0);

    CHECK(Controller.GetFlashedLeftRow() == -1);
    CHECK(Controller.GetFlashedRightRow() == -1);
}

TEST_CASE("Tapping a selected left row again deselects it", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.SetVoicePrompt(false);
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.tapLeft(0);
    REQUIRE(Controller.GetSelectedLeftRow() == 0);

    Controller.tapLeft(0);

    CHECK(Controller.GetSelectedLeftRow() == -1);
}

TEST_CASE("Tapping a selected right row again deselects it", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.tapRight(0);
    REQUIRE(Controller.GetSelectedRightRow() == 0);

    Controller.tapRight(0);

    CHECK(Controller.GetSelectedRightRow() == -1);
}

TEST_CASE("Advancing before the batch is complete is a no-op", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(12)));

    const QStringList ItemsBefore = Controller.GetLeftItems();

    Controller.advance();

    CHECK(Controller.GetLeftItems() == ItemsBefore);
    CHECK(Controller.IsAwaitingAnswer());
}

TEST_CASE("Completing a batch shows feedback, and advancing loads the next batch", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(12)));

    MatchAllPairs(Controller);

    CHECK(Controller.IsShowingFeedback());
    CHECK_FALSE(Controller.IsAwaitingAnswer());
    CHECK(Controller.GetWrongPairCount() == 0);

    Controller.advance();

    CHECK(Controller.GetLeftItems().size() == 5);
    CHECK(Controller.IsAwaitingAnswer());
    CHECK(Controller.GetWrongPairCount() == 0);
}

TEST_CASE("A deck smaller than the match pair count does not finish before its batch is answered",
          "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(3)));

    REQUIRE(Controller.GetLeftItems().size() == 3);
    CHECK_FALSE(Controller.IsFinished());
    CHECK(Controller.IsAwaitingAnswer());

    MatchAllPairs(Controller);

    CHECK_FALSE(Controller.IsFinished());
    CHECK(Controller.IsShowingFeedback());

    Controller.advance();

    CHECK(Controller.IsFinished());
}

TEST_CASE("A deck exactly the match pair count does not finish before its batch is answered",
          "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    REQUIRE(Controller.GetLeftItems().size() == 5);
    CHECK_FALSE(Controller.IsFinished());
    CHECK(Controller.IsAwaitingAnswer());

    MatchAllPairs(Controller);

    CHECK_FALSE(Controller.IsFinished());
    CHECK(Controller.IsShowingFeedback());

    Controller.advance();

    CHECK(Controller.IsFinished());
}

TEST_CASE("A deck between the match pair count and its double does not finish before the last "
          "batch is answered", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(7)));

    REQUIRE(Controller.GetLeftItems().size() == 5);
    CHECK_FALSE(Controller.IsFinished());

    MatchAllPairs(Controller);
    Controller.advance();

    REQUIRE(Controller.GetLeftItems().size() == 2);
    CHECK_FALSE(Controller.IsFinished());
    CHECK(Controller.IsAwaitingAnswer());

    MatchAllPairs(Controller);

    CHECK_FALSE(Controller.IsFinished());
    CHECK(Controller.IsShowingFeedback());

    Controller.advance();

    CHECK(Controller.IsFinished());
}

TEST_CASE("A final batch smaller than the match pair count still awaits answers", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(12)));

    REQUIRE(Controller.GetLeftItems().size() == 5);
    MatchAllPairs(Controller);
    Controller.advance();

    REQUIRE(Controller.GetLeftItems().size() == 5);
    MatchAllPairs(Controller);
    Controller.advance();

    REQUIRE(Controller.GetLeftItems().size() == 2);
    CHECK(Controller.IsAwaitingAnswer());
    CHECK_FALSE(Controller.IsShowingFeedback());

    MatchAllPairs(Controller);

    CHECK(Controller.IsShowingFeedback());
    CHECK_FALSE(Controller.IsAwaitingAnswer());
    CHECK(Controller.GetWrongPairCount() == 0);
}

TEST_CASE("A batch never repeats a card when the session runs the deck more than once", "[speedmatch]")
{
    for (int Attempt = 0; Attempt < 50; ++Attempt)
    {
        SpeedMatchController Controller;
        Controller.Start(MakeParams(MakeCards(7), 2));

        std::vector<int> BatchSizes;

        while (!Controller.IsFinished())
        {
            const QStringList LeftItems = Controller.GetLeftItems();
            REQUIRE_FALSE(LeftItems.isEmpty());

            BatchSizes.push_back(static_cast<int>(LeftItems.size()));

            for (int First = 0; First < LeftItems.size(); ++First)
            {
                for (int Second = First + 1; Second < LeftItems.size(); ++Second)
                {
                    REQUIRE(LeftItems[First] != LeftItems[Second]);
                }
            }

            MatchAllPairs(Controller);
            Controller.advance();
        }

        CHECK(BatchSizes == std::vector<int>{5, 2, 5, 2});
        CHECK(Controller.GetCurrentSessionCount() == 14);
    }
}

TEST_CASE("A pass one card longer than the match pair count fits on one screen", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(6), 2));

    CHECK(Controller.GetLeftItems().size() == 6);

    MatchAllPairs(Controller);
    Controller.advance();

    CHECK(Controller.GetLeftItems().size() == 6);
}

TEST_CASE("GetCurrentSessionCount starts at zero before any pair is matched", "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK(Controller.GetCurrentSessionCount() == 0);
}

TEST_CASE("GetCurrentSessionCount only advances for a correct match, not a wrong tap",
          "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int WrongRightRow = FindWrongRightRow(Controller, 0);
    REQUIRE(WrongRightRow >= 0);

    Controller.tapLeft(0);
    Controller.tapRight(WrongRightRow);

    CHECK(Controller.GetCurrentSessionCount() == 0);

    const int LeftRow = FindUnmatchedLeftRow(Controller);
    REQUIRE(LeftRow >= 0);
    const int CardNumber = ExtractCardNumber(Controller.GetLeftItems()[LeftRow]);
    const int RightRow = FindRightRowForCard(Controller, CardNumber);
    REQUIRE(RightRow >= 0);

    Controller.tapLeft(LeftRow);
    Controller.tapRight(RightRow);

    CHECK(Controller.GetCurrentSessionCount() == 1);
}

TEST_CASE("GetCurrentSessionCount accumulates matched pairs across batches, not cards drawn",
          "[speedmatch]")
{
    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(12)));

    REQUIRE(Controller.GetLeftItems().size() == 5);
    CHECK(Controller.GetCurrentSessionCount() == 0);

    MatchAllPairs(Controller);
    CHECK(Controller.GetCurrentSessionCount() == 5);

    Controller.advance();
    REQUIRE(Controller.GetLeftItems().size() == 5);
    CHECK(Controller.GetCurrentSessionCount() == 5);

    MatchAllPairs(Controller);
    CHECK(Controller.GetCurrentSessionCount() == 10);

    Controller.advance();
    REQUIRE(Controller.GetLeftItems().size() == 2);
    CHECK(Controller.GetCurrentSessionCount() == 10);

    MatchAllPairs(Controller);
    CHECK(Controller.GetCurrentSessionCount() == 12);
}

TEST_CASE("A correct match speaks the matched word", "[speedmatch]")
{
    FAppSpeechGuard SpeechGuard;

    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int LeftRow = 0;
    const int CardNumber = ExtractCardNumber(Controller.GetLeftItems()[LeftRow]);
    const int RightRow = FindRightRowForCard(Controller, CardNumber);
    REQUIRE(RightRow >= 0);

    Controller.tapLeft(LeftRow);
    Controller.tapRight(RightRow);

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == QStringLiteral("word%1").arg(CardNumber));
}

TEST_CASE("A correct match in the final batch still speaks the matched word", "[speedmatch]")
{
    FAppSpeechGuard SpeechGuard;

    SpeedMatchController Controller;
    Controller.Start(MakeParams(MakeCards(12)));

    MatchAllPairs(Controller);
    Controller.advance();
    MatchAllPairs(Controller);
    Controller.advance();
    REQUIRE(Controller.GetLeftItems().size() == 2);

    const int SpeakCountBeforeFinalBatch = SpeechGuard.Mock.SpeakCount;

    const int LeftRow = 0;
    const int CardNumber = ExtractCardNumber(Controller.GetLeftItems()[LeftRow]);
    const int RightRow = FindRightRowForCard(Controller, CardNumber);
    REQUIRE(RightRow >= 0);

    Controller.tapLeft(LeftRow);
    Controller.tapRight(RightRow);

    CHECK(SpeechGuard.Mock.SpeakCount == SpeakCountBeforeFinalBatch + 1);
    CHECK(SpeechGuard.Mock.LastText == QStringLiteral("word%1").arg(CardNumber));
}

TEST_CASE("A correct match does not speak again when the voice prompt already spoke the tapped word",
          "[speedmatch]")
{
    FAppSpeechGuard SpeechGuard;

    SpeedMatchController Controller;
    Controller.SetVoicePrompt(true);
    Controller.Start(MakeParams(MakeCards(5)));

    const int LeftRow = 0;
    const int CardNumber = ExtractCardNumber(Controller.GetLeftItems()[LeftRow]);
    const int RightRow = FindRightRowForCard(Controller, CardNumber);
    REQUIRE(RightRow >= 0);

    Controller.tapLeft(LeftRow);
    CHECK(SpeechGuard.Mock.SpeakCount == 1);

    Controller.tapRight(RightRow);
    CHECK(SpeechGuard.Mock.SpeakCount == 1);
}

TEST_CASE("Muting speech turns a voice speed match into a plain one while it runs", "[speedmatch]")
{
    FAppSpeechGuard SpeechGuard;

    SpeedMatchController Controller;
    Controller.SetVoicePrompt(true);
    Controller.Start(MakeParams(MakeCards(5)));

    REQUIRE(Controller.IsVoiceLeft());

    int StateChanges = 0;
    QObject::connect(&Controller, &ExerciseControllerBase::stateChanged, [&StateChanges]() { ++StateChanges; });

    AppSpeech::Get().SetEnabled(false);

    CHECK_FALSE(Controller.IsVoiceLeft());
    CHECK(StateChanges == 1);

    AppSpeech::Get().SetEnabled(true);

    CHECK(Controller.IsVoiceLeft());
}

TEST_CASE("A speed match without the voice prompt shows words", "[speedmatch]")
{
    FAppSpeechGuard SpeechGuard;

    SpeedMatchController Controller;
    Controller.SetVoicePrompt(false);
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK_FALSE(Controller.IsVoiceLeft());
}

TEST_CASE("A wrong tap does not speak", "[speedmatch]")
{
    FAppSpeechGuard SpeechGuard;

    SpeedMatchController Controller;
    Controller.SetVoicePrompt(false);
    Controller.Start(MakeParams(MakeCards(5)));

    const int WrongRightRow = FindWrongRightRow(Controller, 0);
    REQUIRE(WrongRightRow >= 0);

    Controller.tapLeft(0);
    Controller.tapRight(WrongRightRow);

    CHECK(SpeechGuard.Mock.SpeakCount == 0);
}
