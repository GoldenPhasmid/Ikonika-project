#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "MultipleChoiceController.h"

#include "TestAppSpeech.h"

#include "SessionSettingsGuard.h"
#include "SessionSettings.h"
#include "core/FTSettings.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
    using FQuestionSideGuard = FSessionSettingsGuard;

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

    int FindWrongOption(const MultipleChoiceController& Controller)
    {
        const int OptionCount = Controller.GetOptions().size();
        return (Controller.GetCorrectOptionIndex() + 1) % OptionCount;
    }
}

TEST_CASE("A fresh multiple choice session builds options including the correct answer", "[choice]")
{
    FAppSpeechGuard SpeechGuard;
    FQuestionSideGuard SideGuard;
    SessionSettings::Training().SetChoiceSide(SHOW_WORD);

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    REQUIRE(Controller.GetOptions().size() == 4);
    REQUIRE(Controller.GetCorrectOptionIndex() >= 0);
    CHECK(Controller.GetOptions()[Controller.GetCorrectOptionIndex()].toString() == Controller.GetAnswer());
}

TEST_CASE("Options are capped at four and contain no duplicates", "[choice]")
{
    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(10)));

    const QVariantList Options = Controller.GetOptions();
    REQUIRE(Options.size() == 4);

    for (int First = 0; First < Options.size(); ++First)
    {
        for (int Second = First + 1; Second < Options.size(); ++Second)
        {
            CHECK(Options[First] != Options[Second]);
        }
    }
}

TEST_CASE("A voice multiple choice always asks the word and offers translations", "[choice]")
{
    FAppSpeechGuard SpeechGuard;
    FQuestionSideGuard SideGuard;
    SessionSettings::Training().SetChoiceSide(SHOW_TRANSLATION);

    MultipleChoiceController Controller;
    Controller.SetVoicePrompt(true);
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK(Controller.IsVoicePrompt());
    CHECK(Controller.ShowsTargetLanguage());
    CHECK(Controller.GetPrompt().startsWith(QStringLiteral("word")));

    for (const QVariant& Option : Controller.GetOptions())
    {
        CHECK(Option.toString().startsWith(QStringLiteral("translation")));
    }
}

TEST_CASE("A fresh session is unanswered with no chosen option", "[choice]")
{
    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetChosenOptionIndex() == -1);
}

TEST_CASE("Choosing the correct option records a correct answer", "[choice]")
{
    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.choose(Controller.GetCorrectOptionIndex());

    CHECK(Controller.IsAnswered());
    CHECK(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetChosenOptionIndex() == Controller.GetCorrectOptionIndex());
}

TEST_CASE("Choosing a wrong option records an incorrect answer", "[choice]")
{
    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int WrongOption = FindWrongOption(Controller);
    Controller.choose(WrongOption);

    CHECK(Controller.IsAnswered());
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetChosenOptionIndex() == WrongOption);
}

TEST_CASE("Choosing again after already answered is a no-op", "[choice]")
{
    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int FirstChoice = FindWrongOption(Controller);
    Controller.choose(FirstChoice);
    Controller.choose(Controller.GetCorrectOptionIndex());

    CHECK(Controller.GetChosenOptionIndex() == FirstChoice);
}

TEST_CASE("Choosing an out-of-range option is ignored", "[choice]")
{
    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.choose(-1);
    Controller.choose(Controller.GetOptions().size());

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetChosenOptionIndex() == -1);
}

TEST_CASE("Skipping records an incorrect answer with no chosen option", "[choice]")
{
    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.skip();

    CHECK(Controller.IsAnswered());
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetChosenOptionIndex() == -1);
}

TEST_CASE("Advancing before answering is a no-op", "[choice]")
{
    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int CountBefore = Controller.GetCurrentSessionCount();

    Controller.advance();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == CountBefore);
}

TEST_CASE("Advancing after answering moves to the next card and resets the choice", "[choice]")
{
    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int CountBefore = Controller.GetCurrentSessionCount();

    Controller.choose(Controller.GetCorrectOptionIndex());
    Controller.advance();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetChosenOptionIndex() == -1);
    CHECK(Controller.GetCurrentSessionCount() == CountBefore + 1);
    CHECK(Controller.GetCorrectOptionIndex() >= 0);
}

TEST_CASE("Stays silent up front when the question is word-first", "[choice]")
{
    FQuestionSideGuard SideGuard;
    SessionSettings::Training().SetChoiceSide(SHOW_WORD);

    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK(SpeechGuard.Mock.SpeakCount == 0);
}

TEST_CASE("Speaks the word once when answering a word-first question", "[choice]")
{
    FQuestionSideGuard SideGuard;
    SessionSettings::Training().SetChoiceSide(SHOW_WORD);

    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.choose(Controller.GetCorrectOptionIndex());

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetPrompt());
}

TEST_CASE("Speaks the word once when a word-first question is skipped", "[choice]")
{
    FQuestionSideGuard SideGuard;
    SessionSettings::Training().SetChoiceSide(SHOW_WORD);

    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.skip();

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
}

TEST_CASE("A voice multiple choice does not speak after the reveal", "[choice]")
{
    FQuestionSideGuard SideGuard;
    SessionSettings::Training().SetChoiceSide(SHOW_TRANSLATION);

    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.SetVoicePrompt(true);
    Controller.Start(MakeParams(MakeCards(5)));

    REQUIRE(Controller.IsVoicePrompt());
    REQUIRE(SpeechGuard.Mock.SpeakCount == 1);

    Controller.choose(Controller.GetCorrectOptionIndex());

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
}

TEST_CASE("Speaks the correct word only after answering a translation-first question", "[choice]")
{
    FQuestionSideGuard SideGuard;
    SessionSettings::Training().SetChoiceSide(SHOW_TRANSLATION);

    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK(SpeechGuard.Mock.SpeakCount == 0);

    Controller.choose(Controller.GetCorrectOptionIndex());

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetAnswer());
}

TEST_CASE("Continuing to the next card stays silent when word-first", "[choice]")
{
    FQuestionSideGuard SideGuard;
    SessionSettings::Training().SetChoiceSide(SHOW_WORD);

    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.choose(Controller.GetCorrectOptionIndex());
    const int SpeakCountAfterAnswer = SpeechGuard.Mock.SpeakCount;

    Controller.advance();

    CHECK(SpeechGuard.Mock.SpeakCount == SpeakCountAfterAnswer);
}

TEST_CASE("Continuing to the next card speaks its prompt in a voice multiple choice", "[choice]")
{
    FQuestionSideGuard SideGuard;
    SessionSettings::Training().SetChoiceSide(SHOW_TRANSLATION);

    FAppSpeechGuard SpeechGuard;

    MultipleChoiceController Controller;
    Controller.SetVoicePrompt(true);
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.choose(Controller.GetCorrectOptionIndex());
    const int SpeakCountAfterAnswer = SpeechGuard.Mock.SpeakCount;

    Controller.advance();

    CHECK(SpeechGuard.Mock.SpeakCount == SpeakCountAfterAnswer + 1);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetPrompt());
}
