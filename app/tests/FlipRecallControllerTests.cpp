#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "FlipRecallController.h"

#include "TestAppSpeech.h"

#include "core/FTSettings.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
    struct FQuestionSideGuard
    {
        ~FQuestionSideGuard()
        {
            FTSettings::Get().SetQuestionSide(SHOW_RANDOM);
        }
    };

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

TEST_CASE("A fresh flip session starts unanswered", "[fliprecall]")
{
    FAppSpeechGuard SpeechGuard;

    FlipRecallController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK(Controller.GetCurrentCard() != nullptr);
    CHECK_FALSE(Controller.IsAnswered());
}

TEST_CASE("First advance reveals the answer without moving to the next card", "[fliprecall]")
{
    FAppSpeechGuard SpeechGuard;

    FlipRecallController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const QString PromptBefore = Controller.GetPrompt();
    const int CountBefore = Controller.GetCurrentSessionCount();

    Controller.advance();

    CHECK(Controller.IsAnswered());
    CHECK(Controller.GetLastRevealedAnswer() == Controller.GetAnswer());
    CHECK(Controller.GetPrompt() == PromptBefore);
    CHECK(Controller.GetCurrentSessionCount() == CountBefore);
}

TEST_CASE("A question side override takes precedence over the global setting", "[fliprecall]")
{
    FQuestionSideGuard SideGuard;
    FTSettings::Get().SetQuestionSide(SHOW_WORD);

    FAppSpeechGuard SpeechGuard;

    FlipRecallController Controller;
    Controller.SetQuestionSideOverride([](const ExerciseControllerBase&) { return false; });
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK_FALSE(Controller.ShowsTargetLanguage());
}

TEST_CASE("Without an override, the flip session falls back to the global question side setting",
         "[fliprecall]")
{
    FQuestionSideGuard SideGuard;
    FTSettings::Get().SetQuestionSide(SHOW_TRANSLATION);

    FAppSpeechGuard SpeechGuard;

    FlipRecallController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK_FALSE(Controller.ShowsTargetLanguage());
}

TEST_CASE("Second advance moves to the next card and resets answered", "[fliprecall]")
{
    FAppSpeechGuard SpeechGuard;

    FlipRecallController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int FirstCount = Controller.GetCurrentSessionCount();

    Controller.advance();
    REQUIRE(Controller.IsAnswered());

    Controller.advance();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == FirstCount + 1);
}

TEST_CASE("The reveal-then-continue cycle repeats across the whole deck", "[fliprecall]")
{
    FAppSpeechGuard SpeechGuard;

    FlipRecallController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    for (int Step = 0; Step < 4; ++Step)
    {
        Controller.advance();
        REQUIRE(Controller.IsAnswered());
        Controller.advance();
        REQUIRE_FALSE(Controller.IsAnswered());
    }

    CHECK(Controller.GetCurrentSessionCount() == 5);
}

TEST_CASE("The last card in a FixedLength run can still be revealed and answered", "[fliprecall]")
{
    FAppSpeechGuard SpeechGuard;

    FTSessionStartParams Params = MakeParams(MakeCards(5));
    Params.FixedLength = 5;

    FlipRecallController Controller;
    Controller.Start(Params);

    for (int Step = 0; Step < 4; ++Step)
    {
        Controller.advance();
        Controller.advance();
    }

    REQUIRE(Controller.GetCurrentSessionCount() == 5);

    Controller.advance();

    CHECK(Controller.IsAnswered());
}

TEST_CASE("Stays silent until the card is revealed, whichever side it shows", "[fliprecall]")
{
    FQuestionSideGuard SideGuard;
    FAppSpeechGuard SpeechGuard;

    FTSettings::Get().SetQuestionSide(SHOW_WORD);

    FlipRecallController WordFirst;
    WordFirst.Start(MakeParams(MakeCards(5)));

    CHECK(SpeechGuard.Mock.SpeakCount == 0);

    FTSettings::Get().SetQuestionSide(SHOW_TRANSLATION);

    FlipRecallController TranslationFirst;
    TranslationFirst.Start(MakeParams(MakeCards(5)));

    CHECK(SpeechGuard.Mock.SpeakCount == 0);
}

TEST_CASE("Speaks the target word on reveal when the prompt already shows it", "[fliprecall]")
{
    FQuestionSideGuard SideGuard;
    FTSettings::Get().SetQuestionSide(SHOW_WORD);

    FAppSpeechGuard SpeechGuard;

    FlipRecallController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.advance();

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetPrompt());
}

TEST_CASE("Speaks the target word only after revealing a translation-first card", "[fliprecall]")
{
    FQuestionSideGuard SideGuard;
    FTSettings::Get().SetQuestionSide(SHOW_TRANSLATION);

    FAppSpeechGuard SpeechGuard;

    FlipRecallController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK(SpeechGuard.Mock.SpeakCount == 0);

    Controller.advance();

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetAnswer());
}

TEST_CASE("Continuing to the next card does not speak it", "[fliprecall]")
{
    FQuestionSideGuard SideGuard;
    FTSettings::Get().SetQuestionSide(SHOW_WORD);

    FAppSpeechGuard SpeechGuard;

    FlipRecallController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.advance();
    const int SpeakCountAfterReveal = SpeechGuard.Mock.SpeakCount;

    Controller.advance();

    CHECK(SpeechGuard.Mock.SpeakCount == SpeakCountAfterReveal);
}

TEST_CASE("Answering that the word was forgotten grades nothing aloud and moves on", "[fliprecall]")
{
    FAppSpeechGuard SpeechGuard;

    FlipRecallController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    int GradedCount = 0;
    QObject::connect(&Controller, &ExerciseControllerBase::answerGraded, &Controller, [&GradedCount](bool) { ++GradedCount; });

    const int FirstCount = Controller.GetCurrentSessionCount();

    Controller.advance();
    Controller.answerNo();

    CHECK(GradedCount == 0);
    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == FirstCount + 1);

    Controller.advance();
    Controller.answerYes();

    CHECK(GradedCount == 1);
}
