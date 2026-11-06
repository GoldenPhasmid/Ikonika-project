#include <catch2/catch_test_macros.hpp>

#include "core/sessions/FTExerciseSession.h"

#include <memory>
#include <optional>
#include <set>
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

    std::vector<FTCardHandle> MakeCards()
    {
        return {
            MakeCardHandle(MakeCard("le chien", "dog")),
            MakeCardHandle(MakeCard("la maison", "house")),
            MakeCardHandle(MakeCard("le café", "coffee")),
            MakeCardHandle(MakeCard("l'eau", "water")),
            MakeCardHandle(MakeCard("la voiture", "car"))
        };
    }

    FTSessionStartParams MakeParams(std::vector<FTCardHandle> Cards, bool bInfinite = false,
                                     std::optional<int> FixedLength = std::nullopt)
    {
        FTSessionStartParams Params;
        Params.CardState = std::make_shared<FCardState>(std::move(Cards));
        Params.Selector = std::make_shared<FEachOnceSelector>(Params.CardState->GetCount(), bInfinite);
        Params.FixedLength = FixedLength;
        Params.TargetLanguage = "fr";

        return Params;
    }
}

TEST_CASE("A fresh session starts on the first card", "[session]")
{
    FTExerciseSession Session(MakeParams(MakeCards()));

    CHECK(Session.GetDrawnCount() == 1);
    CHECK(Session.GetCurrentCardIndex() >= 0);
    CHECK(Session.GetCurrentCard() != nullptr);
    CHECK_FALSE(Session.IsFinished());
}

TEST_CASE("A single pass covers every card once, then finishes", "[session]")
{
    const std::vector<FTCardHandle> Cards = MakeCards();
    FTExerciseSession Session(MakeParams(Cards));

    std::set<int> Seen;

    while (!Session.IsFinished())
    {
        Seen.insert(Session.GetCurrentCardIndex());
        Session.Advance();
    }

    CHECK(Seen.size() == Cards.size());
    CHECK(Session.GetCurrentCard() == nullptr);
    CHECK(Session.GetPrompt().empty());
}

TEST_CASE("An infinite selector keeps drawing until FixedLength stops it", "[session]")
{
    FTExerciseSession Session(MakeParams(MakeCards(), true, 20));

    while (!Session.IsFinished())
    {
        Session.Advance();
    }

    CHECK(Session.GetDrawnCount() == 20);
}

TEST_CASE("An empty deck is finished immediately", "[session]")
{
    FTExerciseSession Session(MakeParams({}));

    CHECK(Session.IsFinished());
    CHECK(Session.GetCurrentCard() == nullptr);
    CHECK(Session.GetPrompt().empty());
    CHECK(Session.GetAnswer().empty());
}

TEST_CASE("The prompt is always the target word, the answer always the translation", "[session]")
{
    FTExerciseSession Session(MakeParams(MakeCards()));

    const FTCard* Current = Session.GetCurrentCard();
    REQUIRE(Current != nullptr);

    CHECK(Session.GetPrompt() == Current->TargetWord);
    CHECK(Session.GetAnswer() == Current->Translation);
}

TEST_CASE("GetAnswerFor reads any card by index, not just the current one", "[session]")
{
    const std::vector<FTCardHandle> Cards = MakeCards();
    FTExerciseSession Session(MakeParams(Cards));

    CHECK(Session.GetAnswerFor(1) == Cards[1]->Translation);
    CHECK(Session.GetAnswerFor(4) == Cards[4]->Translation);
}

TEST_CASE("GetPromptFor reads any card by index, not just the current one", "[session]")
{
    const std::vector<FTCardHandle> Cards = MakeCards();
    FTExerciseSession Session(MakeParams(Cards));

    CHECK(Session.GetPromptFor(1) == Cards[1]->TargetWord);
    CHECK(Session.GetPromptFor(4) == Cards[4]->TargetWord);
}

TEST_CASE("Advancing past the end leaves the session finished rather than looping", "[session]")
{
    FTExerciseSession Session(MakeParams(MakeCards()));

    while (!Session.IsFinished())
    {
        Session.Advance();
    }

    Session.Advance();
    Session.Advance();

    CHECK(Session.IsFinished());
    CHECK(Session.GetCurrentCard() == nullptr);
}

TEST_CASE("GetCards exposes the deck in its original order", "[session]")
{
    const std::vector<FTCardHandle> Cards = MakeCards();
    FTExerciseSession Session(MakeParams(Cards));

    CHECK(Session.GetCards().size() == Cards.size());
    CHECK(Session.GetCards()[0]->TargetWord == Cards[0]->TargetWord);
}

TEST_CASE("Removing a card not currently shown drops it from the rest of the pass", "[session]")
{
    const std::vector<FTCardHandle> Cards = MakeCards();
    FTExerciseSession Session(MakeParams(Cards));

    REQUIRE(Session.NotifyCardRemoved(Cards[4]->Id));

    std::set<std::string> Seen;

    while (!Session.IsFinished())
    {
        Seen.insert(Session.GetPrompt());
        Session.Advance();
    }

    CHECK(Seen.size() == Cards.size() - 1);
    CHECK(Seen.count(Cards[4]->TargetWord) == 0);
}

TEST_CASE("Removing the card on screen right now moves the session off it immediately", "[session]")
{
    const std::vector<FTCardHandle> Cards = MakeCards();
    FTExerciseSession Session(MakeParams(Cards));

    const std::string ShownId = Session.GetCurrentCard()->Id;

    REQUIRE(Session.NotifyCardRemoved(ShownId));

    if (!Session.IsFinished())
    {
        CHECK(Session.GetCurrentCard()->Id != ShownId);
    }
}

TEST_CASE("Removing a card the session never mentioned is a no-op", "[session]")
{
    FTExerciseSession Session(MakeParams(MakeCards()));

    CHECK_FALSE(Session.NotifyCardRemoved("not-in-this-deck"));
}

TEST_CASE("Removing every card finishes an infinite session instead of spinning forever", "[session]")
{
    const std::vector<FTCardHandle> Cards = MakeCards();
    FTExerciseSession Session(MakeParams(Cards, true, 20));

    for (const FTCardHandle& Card : Cards)
    {
        Session.NotifyCardRemoved(Card->Id);
    }

    CHECK(Session.IsFinished());
    CHECK(Session.GetCurrentCard() == nullptr);
}

TEST_CASE("A removed card is left out of FCardState's active cards", "[session]")
{
    const std::vector<FTCardHandle> Cards = MakeCards();
    auto CardState = std::make_shared<FCardState>(Cards);

    CardState->MarkRemoved(Cards[2]->Id);

    CHECK(CardState->GetCount() == static_cast<int>(Cards.size()));
    CHECK(CardState->GetActiveCount() == static_cast<int>(Cards.size()) - 1);

    const std::vector<FTCardHandle> Active = CardState->GetActiveCards();
    CHECK(Active.size() == Cards.size() - 1);

    for (const FTCardHandle& Card : Active)
    {
        CHECK(Card->Id != Cards[2]->Id);
    }
}
