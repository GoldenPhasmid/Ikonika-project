#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "FlashcardController.h"

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

    FTSessionStartParams MakeFlashcardParams(std::vector<FTCardHandle> Cards)
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

TEST_CASE("A fresh flashcard session starts on the first card", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeFlashcardParams(MakeCards(5)));

    CHECK(Controller.GetCurrentSessionCount() == 1);
    CHECK(Controller.GetMaxSessionCount() == 5);
    CHECK_FALSE(Controller.CanGoPrevious());
    CHECK(Controller.CanGoNext());
    CHECK_FALSE(Controller.GetPrompt().isEmpty());
    CHECK_FALSE(Controller.GetAnswer().isEmpty());
    CHECK(Controller.GetPrompt() != Controller.GetAnswer());
}

TEST_CASE("The displayed card self-corrects if it was removed by any other path", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FTSessionStartParams Params = MakeFlashcardParams(MakeCards(5));
    const std::shared_ptr<FCardState> CardState = Params.CardState;

    FlashcardController Controller;
    Controller.Start(Params);

    const std::string ShownId = Controller.GetCurrentCard()->Id;

    CardState->MarkRemoved(ShownId);

    REQUIRE(Controller.GetCurrentCard() != nullptr);
    CHECK(Controller.GetCurrentCard()->Id != ShownId);
    CHECK_FALSE(Controller.GetPrompt().isEmpty());
}

TEST_CASE("Starting a session speaks the first card's prompt", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeFlashcardParams(MakeCards(5)));

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetPrompt());
}

TEST_CASE("next past the frontier draws a new card", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeFlashcardParams(MakeCards(5)));
    
    const QString FirstPrompt = Controller.GetPrompt();

    Controller.next();

    CHECK(Controller.GetCurrentSessionCount() == 2);
    CHECK(Controller.GetPrompt() != FirstPrompt);
    CHECK(Controller.CanGoPrevious());
}

TEST_CASE("next replays history instead of drawing when not at the frontier", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeFlashcardParams(MakeCards(5)));

    Controller.next();
    const QString SecondPrompt = Controller.GetPrompt();

    Controller.previous();
    REQUIRE(Controller.GetCurrentSessionCount() == 1);

    Controller.next();
    
    CHECK(Controller.GetPrompt() == SecondPrompt);
    CHECK(Controller.GetCurrentSessionCount() == 2);
}

TEST_CASE("next at the end of the deck is a safe no-op", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeFlashcardParams(MakeCards(2)));

    Controller.next();
    REQUIRE(Controller.GetCurrentSessionCount() == 2);
    CHECK(Controller.CanGoNext());

    Controller.next();

    CHECK(Controller.GetCurrentSessionCount() == 2);
    CHECK_FALSE(Controller.CanGoNext());

    Controller.next();

    CHECK(Controller.GetCurrentSessionCount() == 2);
}

TEST_CASE("previous at the first card is a no-op", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeFlashcardParams(MakeCards(5)));

    const QString FirstPrompt = Controller.GetPrompt();

    Controller.previous();

    CHECK_FALSE(Controller.CanGoPrevious());
    CHECK(Controller.GetCurrentSessionCount() == 1);
    CHECK(Controller.GetPrompt() == FirstPrompt);
}

TEST_CASE("next then previous returns to the exact same card and side", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeFlashcardParams(MakeCards(5)));

    const QString FirstPrompt = Controller.GetPrompt();
    const QString FirstAnswer = Controller.GetAnswer();

    Controller.next();
    Controller.previous();

    CHECK(Controller.GetPrompt() == FirstPrompt);
    CHECK(Controller.GetAnswer() == FirstAnswer);
    CHECK(Controller.GetCurrentSessionCount() == 1);
}

TEST_CASE("A card's side stays fixed across repeat visits", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeFlashcardParams(MakeCards(5)));

    const QString FirstPrompt = Controller.GetPrompt();

    Controller.next();
    Controller.next();
    Controller.previous();
    Controller.previous();

    CHECK(Controller.GetPrompt() == FirstPrompt);
}

TEST_CASE("currentSessionCount reflects browsing position, not total drawn", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeFlashcardParams(MakeCards(5)));

    Controller.next();
    Controller.next();
    Controller.next();
    REQUIRE(Controller.GetCurrentSessionCount() == 4);

    Controller.previous();
    Controller.previous();

    CHECK(Controller.GetCurrentSessionCount() == 2);
    CHECK(Controller.GetMaxSessionCount() == 5);
}

TEST_CASE("Restarting clears history back to the first card", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeFlashcardParams(MakeCards(5)));

    Controller.next();
    Controller.next();
    REQUIRE(Controller.GetCurrentSessionCount() == 3);

    Controller.restart();

    CHECK(Controller.GetCurrentSessionCount() == 1);
    CHECK_FALSE(Controller.CanGoPrevious());
}

TEST_CASE("A one card deck reports canGoNext briefly before the session catches up", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FlashcardController Controller;
    Controller.Start(MakeFlashcardParams(MakeCards(1)));

    CHECK(Controller.GetCurrentSessionCount() == 1);
    CHECK(Controller.CanGoNext());

    Controller.next();

    CHECK(Controller.GetCurrentSessionCount() == 1);
    CHECK_FALSE(Controller.CanGoNext());
}

TEST_CASE("FixedLength greater than the card count recycles through the deck again", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FTSessionStartParams Params = MakeFlashcardParams(MakeCards(3));
    Params.FixedLength = 7;

    FlashcardController Controller;
    Controller.Start(Params);

    CHECK(Controller.GetMaxSessionCount() == 7);

    for (int Step = 0; Step < 6; ++Step)
    {
        Controller.next();
    }

    CHECK(Controller.GetCurrentSessionCount() == 7);
    CHECK(Controller.CanGoNext());

    Controller.next();

    CHECK(Controller.GetCurrentSessionCount() == 7);
    CHECK_FALSE(Controller.CanGoNext());
}

TEST_CASE("FixedLength less than the card count ends the session early", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FTSessionStartParams Params = MakeFlashcardParams(MakeCards(5));
    Params.FixedLength = 2;

    FlashcardController Controller;
    Controller.Start(Params);

    CHECK(Controller.GetMaxSessionCount() == 2);

    Controller.next();

    CHECK(Controller.GetCurrentSessionCount() == 2);
    CHECK(Controller.CanGoNext());

    Controller.next();

    CHECK(Controller.GetCurrentSessionCount() == 2);
    CHECK_FALSE(Controller.CanGoNext());
    CHECK(Controller.CanGoPrevious());
}

TEST_CASE("A negative FixedLength never triggers the length cap", "[flashcard]")
{
    FAppSpeechGuard SpeechGuard;

    FTSessionStartParams Params = MakeFlashcardParams(MakeCards(3));
    Params.FixedLength = -1;

    FlashcardController Controller;
    Controller.Start(Params);

    CHECK(Controller.GetMaxSessionCount() == -1);

    for (int Step = 0; Step < 50; ++Step)
    {
        Controller.next();
    }

    CHECK(Controller.GetCurrentSessionCount() == 51);
    CHECK(Controller.CanGoNext());
}
