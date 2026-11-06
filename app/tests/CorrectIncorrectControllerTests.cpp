#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "CorrectIncorrectController.h"

#include "TestAppSpeech.h"

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

    FTSessionStartParams MakeParams(std::vector<FTCardHandle> Cards)
    {
        FTSessionStartParams Params{};
        Params.CardState = std::make_shared<FCardState>(std::move(Cards));
        Params.Title = "Test Session";
        Params.TargetLanguage = "fr";
        Params.FixedLength = Params.CardState->GetCount();
        Params.Selector = std::make_shared<FEachOnceSelector>(Params.CardState->GetCount(), true);

        return Params;
    }
}

TEST_CASE("A fresh correct/incorrect session speaks the prompt and starts unanswered", "[correctincorrect]")
{
    FAppSpeechGuard SpeechGuard;

    CorrectIncorrectController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetPrompt());
}

TEST_CASE("Revealing shows the answer without advancing or speaking again", "[correctincorrect]")
{
    FAppSpeechGuard SpeechGuard;

    CorrectIncorrectController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int CountBefore = Controller.GetCurrentSessionCount();

    Controller.reveal();

    CHECK(Controller.IsAnswered());
    CHECK(Controller.GetLastRevealedAnswer() == Controller.GetAnswer());
    CHECK(Controller.GetCurrentSessionCount() == CountBefore);
    CHECK(SpeechGuard.Mock.SpeakCount == 1);
}

TEST_CASE("Answering before revealing is a no-op", "[correctincorrect]")
{
    FAppSpeechGuard SpeechGuard;

    CorrectIncorrectController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int CountBefore = Controller.GetCurrentSessionCount();

    Controller.answerYes();
    Controller.answerNo();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == CountBefore);
}

TEST_CASE("Answering yes after reveal records a positive outcome and emits answerRecorded", "[correctincorrect]")
{
    FAppSpeechGuard SpeechGuard;

    CorrectIncorrectController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const std::string ExpectedCardId = Controller.GetCurrentCard()->Id;
    const int CountBefore = Controller.GetCurrentSessionCount();

    std::string EmittedCardId;
    bool EmittedPositive = false;
    int EmitCount = 0;

    QObject::connect(&Controller, &CorrectIncorrectController::answerRecorded, &Controller,
        [&](const std::string& CardId, bool bPositive)
        {
            ++EmitCount;
            EmittedCardId = CardId;
            EmittedPositive = bPositive;
        });

    Controller.reveal();
    Controller.answerYes();

    REQUIRE(EmitCount == 1);
    CHECK(EmittedCardId == ExpectedCardId);
    CHECK(EmittedPositive);
    CHECK(Controller.WasLastAnswerCorrect());
    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == CountBefore + 1);
}

TEST_CASE("Answering no after reveal records a negative outcome and emits answerRecorded", "[correctincorrect]")
{
    FAppSpeechGuard SpeechGuard;

    CorrectIncorrectController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const std::string ExpectedCardId = Controller.GetCurrentCard()->Id;

    std::string EmittedCardId;
    bool EmittedPositive = true;
    int EmitCount = 0;

    QObject::connect(&Controller, &CorrectIncorrectController::answerRecorded, &Controller,
        [&](const std::string& CardId, bool bPositive)
        {
            ++EmitCount;
            EmittedCardId = CardId;
            EmittedPositive = bPositive;
        });

    Controller.reveal();
    Controller.answerNo();

    REQUIRE(EmitCount == 1);
    CHECK(EmittedCardId == ExpectedCardId);
    CHECK_FALSE(EmittedPositive);
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Answering speaks the next card's prompt", "[correctincorrect]")
{
    FAppSpeechGuard SpeechGuard;

    CorrectIncorrectController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    REQUIRE(SpeechGuard.Mock.SpeakCount == 1);

    Controller.reveal();
    Controller.answerYes();

    CHECK(SpeechGuard.Mock.SpeakCount == 2);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetPrompt());
}
