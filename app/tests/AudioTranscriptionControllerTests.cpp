#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "AudioTranscriptionController.h"

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

TEST_CASE("A fresh audio transcription session speaks the target word", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == "word0");
}

TEST_CASE("Replaying speaks the target word again", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    REQUIRE(SpeechGuard.Mock.SpeakCount == 1);

    Controller.replay();

    CHECK(SpeechGuard.Mock.SpeakCount == 2);
    CHECK(SpeechGuard.Mock.LastText == "word0");
}

TEST_CASE("Submitting the exact target word records a correct answer", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.submitAnswer("word0");

    CHECK(Controller.IsAnswered());
    CHECK(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetLastExpectedAnswer() == "word0");
    CHECK(Controller.GetLastCardTranslation() == "translation0");
}

TEST_CASE("A card's other spelling is accepted when typed", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    FTCard Form = MakeCard("je paie", "payer");
    Form.AlternativeSpellings = {"je paye"};

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams({MakeCardHandle(Form), MakeCardHandle(Form)}));

    CHECK(SpeechGuard.Mock.LastText == "je paie");

    Controller.submitAnswer("je paye");

    CHECK(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetLastExpectedAnswer() == "je paie");

    Controller.advance();
    Controller.submitAnswer("je payes");

    CHECK_FALSE(Controller.WasLastAnswerCorrect());
}

TEST_CASE("A wrong transcription holds the card until the word is typed right", "[audiotranscription][retype]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams(MakeCards(3)));

    const QString Word = Controller.GetPrompt();

    Controller.submitAnswer("nope");

    CHECK(Controller.IsRetypePending());

    Controller.advance();

    CHECK(Controller.IsAnswered());

    Controller.submitAnswer("again wrong");

    CHECK(Controller.IsRetypePending());
    CHECK(Controller.GetResultMarks().size() == 1);

    Controller.submitAnswer(Word);
    Controller.advance();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetResultMarks().size() == 1);
}

TEST_CASE("Submitting the wrong text records an incorrect answer", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.submitAnswer("nope");

    CHECK(Controller.IsAnswered());
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Submitting blank text is not an answer", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.submitAnswer("");
    Controller.submitAnswer("   ");

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetResultMarks().isEmpty());
}

TEST_CASE("Submitting again after already answered is a no-op", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.submitAnswer("word0");
    Controller.submitAnswer("nope");

    CHECK(Controller.GetLastGivenAnswer() == "word0");
    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Skipping records an incorrect answer with no given answer", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.skip();

    CHECK(Controller.IsAnswered());
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetLastGivenAnswer().isEmpty());
    CHECK(Controller.GetLastCardTranslation() == "translation0");
}

TEST_CASE("Advancing before answering is a no-op", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int CountBefore = Controller.GetCurrentSessionCount();

    Controller.advance();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == CountBefore);
}

TEST_CASE("Advancing after answering moves to the next card and speaks its prompt", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int CountBefore = Controller.GetCurrentSessionCount();
    const QString FirstExpectedAnswer = SpeechGuard.Mock.LastText;

    Controller.submitAnswer(FirstExpectedAnswer);
    const int SpeakCountAfterAnswer = SpeechGuard.Mock.SpeakCount;

    Controller.advance();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == CountBefore + 1);
    CHECK(SpeechGuard.Mock.SpeakCount == SpeakCountAfterAnswer + 1);
    CHECK(SpeechGuard.Mock.LastText != FirstExpectedAnswer);
}

TEST_CASE("An answer check replaces the word comparison", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.SetAnswerCheck(
        [](const std::string& Given, const std::string& Expected)
        {
            return Given == "accept" && Expected == "Oui, je viens.";
        });

    Controller.Start(MakeParams({
        MakeCardHandle(MakeCard("Oui, je viens.", "Yes, I am coming.")),
        MakeCardHandle(MakeCard("Oui, je viens.", "Yes, I am coming.")),
    }));

    Controller.submitAnswer("oui");

    CHECK_FALSE(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetLastExpectedAnswer() == "Oui, je viens.");

    Controller.submitAnswer("accept");
    Controller.advance();
    Controller.submitAnswer("accept");

    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Without an answer check a comma still separates accepted answers", "[audiotranscription]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Controller;
    Controller.Start(MakeParams({MakeCardHandle(MakeCard("oui, bien", "yes, well"))}));

    Controller.submitAnswer("oui");

    CHECK(Controller.WasLastAnswerCorrect());
}
