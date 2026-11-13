#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "FlashcardController.h"
#include "WrittenProductionController.h"

#include "TestAppSpeech.h"

#include "core/FTCard.h"
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

TEST_CASE("English verbs keep their prefix when noun articles are hidden", "[exercisecontrollerbase][englishverbs]")
{
    FAppSpeechGuard SpeechGuard;
    const bool bPreviousShowArticle{FTSettings::Get().GetExerciseDisplayRules().bShowArticle};
    FTSettings::Get().SetShowArticle(false);

    FTCard Card{MakeCard("learn", "meaning")};
    Card.PartOfSpeech = "verb";
    FTSessionStartParams Params{MakeParams({MakeCardHandle(Card)})};
    Params.TargetLanguage = "en";

    FlashcardController Controller;
    Controller.Start(Params);
    CHECK(Controller.GetPrompt() == "to learn");
    CHECK(Controller.GetShownPrompt() == "to learn");

    WrittenProductionController Typing;
    Typing.SetAskWord(false);
    Typing.Start(Params);
    CHECK(Typing.GetAnswer() == "to learn");
    CHECK(Typing.GetShownAnswer() == "to learn");
    Typing.submitAnswer("learn");
    CHECK(Typing.WasLastAnswerCorrect());

    FTSettings::Get().SetShowArticle(bPreviousShowArticle);
}

TEST_CASE("GetTitle returns the title the session was started with", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    FTSessionStartParams Params = MakeParams(MakeCards(1));
    Params.Title = "Animals";
    Controller.Start(Params);

    CHECK(Controller.GetTitle() == "Animals");
}

TEST_CASE("GetPartOfSpeech reads the current card's part of speech", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    FTCard Card = MakeCard("word0", "translation0");
    Card.PartOfSpeech = "noun";

    std::vector<FTCardHandle> Cards;
    Cards.push_back(MakeCardHandle(std::move(Card)));

    FlashcardController Controller;
    Controller.Start(MakeParams(std::move(Cards)));

    CHECK(Controller.GetPartOfSpeech() == "noun");
}

TEST_CASE("GetPartOfSpeech is empty when the card has none", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(Controller.GetPartOfSpeech().isEmpty());
}

TEST_CASE("GetGender reports masculine, feminine and common nouns by name", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    {
        FTCard Card = MakeCard("word0", "translation0");
        Card.Gender = GenderMasculine;
        std::vector<FTCardHandle> Cards;
        Cards.push_back(MakeCardHandle(std::move(Card)));

        FlashcardController Controller;
        Controller.Start(MakeParams(std::move(Cards)));

        CHECK(Controller.GetGender() == "masculine");
    }
    {
        FTCard Card = MakeCard("word0", "translation0");
        Card.Gender = GenderFeminine;
        std::vector<FTCardHandle> Cards;
        Cards.push_back(MakeCardHandle(std::move(Card)));

        FlashcardController Controller;
        Controller.Start(MakeParams(std::move(Cards)));

        CHECK(Controller.GetGender() == "feminine");
    }
    {
        FTCard Card = MakeCard("word0", "translation0");
        Card.Gender = GenderCommon;
        std::vector<FTCardHandle> Cards;
        Cards.push_back(MakeCardHandle(std::move(Card)));

        FlashcardController Controller;
        Controller.Start(MakeParams(std::move(Cards)));

        CHECK(Controller.GetGender() == "any");
    }
}

TEST_CASE("GetGender is empty when the card has no gender", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(Controller.GetGender().isEmpty());
}

TEST_CASE("GetCardMeta joins part of speech and gender when both are present", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    FTCard Card = MakeCard("word0", "translation0");
    Card.PartOfSpeech = "noun";
    Card.Gender = GenderMasculine;

    std::vector<FTCardHandle> Cards;
    Cards.push_back(MakeCardHandle(std::move(Card)));

    FlashcardController Controller;
    Controller.Start(MakeParams(std::move(Cards)));

    CHECK(Controller.GetCardMeta() == "noun · masculine");
}

TEST_CASE("GetCardMeta falls back to whichever of part of speech or gender is present", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    {
        FTCard Card = MakeCard("word0", "translation0");
        Card.PartOfSpeech = "noun";
        std::vector<FTCardHandle> Cards;
        Cards.push_back(MakeCardHandle(std::move(Card)));

        FlashcardController Controller;
        Controller.Start(MakeParams(std::move(Cards)));

        CHECK(Controller.GetCardMeta() == "noun");
    }
    {
        FTCard Card = MakeCard("word0", "translation0");
        Card.Gender = GenderFeminine;
        std::vector<FTCardHandle> Cards;
        Cards.push_back(MakeCardHandle(std::move(Card)));

        FlashcardController Controller;
        Controller.Start(MakeParams(std::move(Cards)));

        CHECK(Controller.GetCardMeta() == "feminine");
    }
}

TEST_CASE("GetCardMeta is empty when neither part of speech nor gender is set", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(Controller.GetCardMeta().isEmpty());
}

TEST_CASE("GetDefinition reads the current card's definition", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    FTCard Card = MakeCard("word0", "translation0");
    Card.Definition = "a small domesticated carnivore";

    std::vector<FTCardHandle> Cards;
    Cards.push_back(MakeCardHandle(std::move(Card)));

    FlashcardController Controller;
    Controller.Start(MakeParams(std::move(Cards)));

    CHECK(Controller.GetDefinition() == "a small domesticated carnivore");
}

TEST_CASE("GetDefinition is empty when the card has none", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(Controller.GetDefinition().isEmpty());
}

TEST_CASE("A card with no meaning shows its hint as the example", "[exercisecontrollerbase][example]")
{
    FAppSpeechGuard SpeechGuard;

    FTCard Card = MakeCard("word0", "translation0");
    Card.Hint = "Il entra dans la maison";

    std::vector<FTCardHandle> Cards;
    Cards.push_back(MakeCardHandle(std::move(Card)));

    FlashcardController Controller;
    Controller.Start(MakeParams(std::move(Cards)));

    CHECK(Controller.GetExample() == "Il entra dans la maison");
    CHECK(Controller.GetExampleTranslation().isEmpty());
}

TEST_CASE("A card with neither meaning nor hint shows no example", "[exercisecontrollerbase][example]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(Controller.GetExample().isEmpty());
    CHECK(Controller.GetExampleTranslation().isEmpty());
}

TEST_CASE("A meaning that cannot be found falls back to the hint", "[exercisecontrollerbase][example]")
{
    FAppSpeechGuard SpeechGuard;

    FTCard Card = MakeCard("word0", "translation0");
    Card.MeaningGist = "a gist the dictionary does not hold";
    Card.Hint = "Il entra dans la maison";

    std::vector<FTCardHandle> Cards;
    Cards.push_back(MakeCardHandle(std::move(Card)));

    FlashcardController Controller;
    Controller.Start(MakeParams(std::move(Cards)));

    CHECK(Controller.GetExample() == "Il entra dans la maison");
}

TEST_CASE("Slow speech repeats the word at a slower rate", "[exercisecontrollerbase][slowspeech]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeParams(MakeCards(2)));

    SpeechGuard.Mock.SpeakCount = 0;
    Controller.speakSlowly();

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == QString::fromStdString(Controller.GetCurrentCard()->TargetWord));
    CHECK(SpeechGuard.Mock.LastRateScale == Catch::Approx(0.6f));
}

TEST_CASE("Slow speech speaks the current card even when sound was off for it", "[exercisecontrollerbase][slowspeech]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeParams(MakeCards(2)));
    Controller.advance();

    Controller.speakSlowly();

    CHECK(SpeechGuard.Mock.LastText == QString::fromStdString(Controller.GetCurrentCard()->TargetWord));
    CHECK(SpeechGuard.Mock.LastRateScale == Catch::Approx(0.6f));
}

TEST_CASE("Slow speech scales the sentence speech rate for a sentence exercise", "[exercisecontrollerbase][slowspeech]")
{
    FAppSpeechGuard SpeechGuard;
    SpeechGuard.Mock.SentenceSpeed = 0.75f;

    FlashcardController Controller;
    Controller.SetSpeaksSentences(true);
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.speakSlowly();

    CHECK(SpeechGuard.Mock.LastRateScale == Catch::Approx(0.45f));
}

TEST_CASE("Card metadata follows the current card as it changes", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    FTCard First = MakeCard("word0", "translation0");
    First.PartOfSpeech = "noun";

    FTCard Second = MakeCard("word1", "translation1");
    Second.PartOfSpeech = "verb";

    std::vector<FTCardHandle> Cards;
    Cards.push_back(MakeCardHandle(std::move(First)));
    Cards.push_back(MakeCardHandle(std::move(Second)));

    FlashcardController Controller;
    Controller.Start(MakeParams(std::move(Cards)));

    const QString FirstPartOfSpeech = Controller.GetPartOfSpeech();

    Controller.next();

    CHECK(Controller.GetPartOfSpeech() != FirstPartOfSpeech);
}

TEST_CASE("GetElapsedSeconds is zero before any session starts", "[exercisecontrollerbase]")
{
    FlashcardController Controller;

    CHECK(Controller.GetElapsedSeconds() == 0);
}

TEST_CASE("GetElapsedSeconds starts counting once a session starts", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(Controller.GetElapsedSeconds() >= 0);
}

TEST_CASE("GetSessionCardState is null before a session starts", "[exercisecontrollerbase]")
{
    WrittenProductionController Controller;

    CHECK(Controller.GetSessionCardState() == nullptr);
}

TEST_CASE("GetSessionCardState exposes the session's card pool", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    REQUIRE(Controller.GetSessionCardState() != nullptr);
    CHECK(Controller.GetSessionCardState()->GetCount() == 5);
}

TEST_CASE("GetResultMarks records one entry per answered card, correct as 1 and wrong as 0", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK(Controller.GetResultMarks().isEmpty());

    Controller.submitAnswer(Controller.GetAnswer());

    REQUIRE(Controller.GetResultMarks().size() == 1);
    CHECK(Controller.GetResultMarks()[0].toDouble() == 1.0);

    Controller.advance();
    Controller.submitAnswer("definitely not the answer");

    REQUIRE(Controller.GetResultMarks().size() == 2);
    CHECK(Controller.GetResultMarks()[0].toDouble() == 1.0);
    CHECK(Controller.GetResultMarks()[1].toDouble() == 0.0);
}

TEST_CASE("GetResultMarks is cleared when the session restarts", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.submitAnswer(Controller.GetAnswer());
    REQUIRE(Controller.GetResultMarks().size() == 1);

    Controller.restart();

    CHECK(Controller.GetResultMarks().isEmpty());
}

TEST_CASE("GetLastCardIndex is unset until an answer is recorded", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    CHECK(Controller.GetLastCardIndex() == -1);

    Controller.submitAnswer(Controller.GetAnswer());

    CHECK(Controller.GetLastCardIndex() >= 0);
}

TEST_CASE("TakeSession returns no session before one has started", "[exercisecontrollerbase]")
{
    WrittenProductionController Controller;

    CHECK_FALSE(Controller.TakeSession().has_value());
}

TEST_CASE("RestoreSession with no snapshot leaves the controller without a session", "[exercisecontrollerbase]")
{
    WrittenProductionController Controller;

    const bool bRestored = Controller.RestoreSession(FTSessionStartParams{}, std::nullopt);

    CHECK_FALSE(bRestored);
    CHECK(Controller.GetCurrentCard() == nullptr);
}

TEST_CASE("RestoreSession replays a session taken from another controller", "[exercisecontrollerbase]")
{
    FAppSpeechGuard SpeechGuard;

    FTSessionStartParams SourceParams = MakeParams(MakeCards(5));

    WrittenProductionController Source;
    Source.Start(SourceParams);
    Source.submitAnswer(Source.GetAnswer());
    Source.advance();

    const std::optional<FExerciseCheckpoint> Checkpoint = Source.TakeSession();
    REQUIRE(Checkpoint.has_value());

    FTSessionStartParams TargetParams = SourceParams;
    TargetParams.Selector = std::make_shared<FEachOnceSelector>(TargetParams.CardState->GetCount(), true);

    WrittenProductionController Target;
    const bool bRestored = Target.RestoreSession(TargetParams, Checkpoint);

    CHECK(bRestored);
    CHECK(Target.GetCurrentCard() != nullptr);
    CHECK(Target.GetPrompt() == Source.GetPrompt());
    CHECK(Target.GetAnswer() == Source.GetAnswer());
    CHECK(Target.GetCurrentSessionCount() == Source.GetCurrentSessionCount());
}

namespace
{
    struct FSpeakArticleGuard
    {
        const bool bPrevious = FTSettings::Get().GetSpeechRules().bSpeakArticle;

        ~FSpeakArticleGuard()
        {
            FTSettings::Get().SetSpeakArticle(bPrevious);
        }
    };

    std::vector<FTCardHandle> MakeMasculineNounCards()
    {
        FTCard Card = MakeCard("chat", "cat");
        Card.PartOfSpeech = "noun";
        Card.Gender = GenderMasculine;

        std::vector<FTCardHandle> Cards;
        Cards.push_back(MakeCardHandle(std::move(Card)));

        return Cards;
    }
}

TEST_CASE("The voice reads the article when speaking it is on", "[exercisecontrollerbase][voicing]")
{
    FAppSpeechGuard SpeechGuard;
    FSpeakArticleGuard SettingGuard;

    FTSettings::Get().SetSpeakArticle(true);

    FlashcardController Controller;
    Controller.Start(MakeParams(MakeMasculineNounCards()));

    CHECK(SpeechGuard.Mock.LastText == "le chat");
}

TEST_CASE("The voice reads the bare word when speaking the article is off", "[exercisecontrollerbase][voicing]")
{
    FAppSpeechGuard SpeechGuard;
    FSpeakArticleGuard SettingGuard;

    FTSettings::Get().SetSpeakArticle(false);

    FlashcardController Controller;
    Controller.Start(MakeParams(MakeMasculineNounCards()));

    CHECK(SpeechGuard.Mock.LastText == "chat");
    CHECK(Controller.GetPrompt() == "le chat");
}

TEST_CASE("Turning the article off leaves a card without an article as it is", "[exercisecontrollerbase][voicing]")
{
    FAppSpeechGuard SpeechGuard;
    FSpeakArticleGuard SettingGuard;

    FTSettings::Get().SetSpeakArticle(false);

    FlashcardController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(SpeechGuard.Mock.LastText == "word0");
}
