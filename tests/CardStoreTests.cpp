#include <catch2/catch_test_macros.hpp>

#include "core/FTCardStore.h"

#include <string>
#include <vector>

namespace
{
    FTCard MakeCard(std::string Id, std::string TargetWord, std::string Translation)
    {
        FTCard Card;
        Card.Id = std::move(Id);
        Card.TargetWord = std::move(TargetWord);
        Card.Translation = std::move(Translation);

        return Card;
    }
}

TEST_CASE("Reading the same card twice hands back the same object", "[cardstore]")
{
    FTCardStore Store;

    const FTCardHandle First = Store.UpdateCard(MakeCard("id0", "chien", "dog"));
    const FTCardHandle Second = Store.UpdateCard(MakeCard("id0", "chien", "dog"));

    CHECK(First.get() == Second.get());
}

TEST_CASE("Different cards get different objects", "[cardstore]")
{
    FTCardStore Store;

    const FTCardHandle Dog = Store.UpdateCard(MakeCard("id0", "chien", "dog"));
    const FTCardHandle Cat = Store.UpdateCard(MakeCard("id1", "chat", "cat"));

    CHECK(Dog.get() != Cat.get());
}

TEST_CASE("A later read reaches a handle taken earlier", "[cardstore]")
{
    FTCardStore Store;

    const FTCardHandle Held = Store.UpdateCard(MakeCard("id0", "chien", "dog"));
    REQUIRE(Held->Translation == "dog");

    Store.UpdateCard(MakeCard("id0", "chien", "hound"));

    CHECK(Held->Translation == "hound");
}

TEST_CASE("An unheld card is dropped rather than kept for the life of the app", "[cardstore]")
{
    FTCardStore Store;

    {
        const FTCardHandle Held = Store.UpdateCard(MakeCard("id0", "chien", "dog"));
        CHECK(Store.GetLoadedCardCount() == 1);
    }

    CHECK(Store.GetLoadedCardCount() == 0);
    CHECK(Store.FindCard("id0") == nullptr);
}

TEST_CASE("A card read again after being dropped comes back as a new object", "[cardstore]")
{
    FTCardStore Store;

    {
        const FTCardHandle Held = Store.UpdateCard(MakeCard("id0", "chien", "dog"));
        CHECK(Held->Translation == "dog");
    }

    const FTCardHandle Second = Store.UpdateCard(MakeCard("id0", "chien", "hound"));

    REQUIRE(Second != nullptr);
    CHECK(Second->Translation == "hound");
    CHECK(Store.GetLoadedCardCount() == 1);
}

TEST_CASE("A card still held can be found by id", "[cardstore]")
{
    FTCardStore Store;

    const FTCardHandle Held = Store.UpdateCard(MakeCard("id0", "chien", "dog"));

    const FTCardHandle Found = Store.FindCard("id0");

    REQUIRE(Found != nullptr);
    CHECK(Found.get() == Held.get());
}

TEST_CASE("A card with no id is never shared", "[cardstore]")
{
    FTCardStore Store;

    const FTCardHandle First = Store.UpdateCard(MakeCard("", "chien", "dog"));
    const FTCardHandle Second = Store.UpdateCard(MakeCard("", "chat", "cat"));

    REQUIRE(First != nullptr);
    REQUIRE(Second != nullptr);
    CHECK(First.get() != Second.get());
    CHECK(First->Translation == "dog");
    CHECK(Second->Translation == "cat");
}

TEST_CASE("Forgetting a card leaves whoever holds it alone", "[cardstore]")
{
    FTCardStore Store;

    const FTCardHandle Held = Store.UpdateCard(MakeCard("id0", "chien", "dog"));

    Store.RemoveCard("id0");

    CHECK(Held->Translation == "dog");
    CHECK(Store.FindCard("id0") == nullptr);

    const FTCardHandle Fresh = Store.UpdateCard(MakeCard("id0", "chien", "hound"));

    CHECK(Fresh.get() != Held.get());
    CHECK(Held->Translation == "dog");
}

TEST_CASE("Every field of a card carries across a refresh", "[cardstore]")
{
    FTCardStore Store;

    const FTCardHandle Held = Store.UpdateCard(MakeCard("id0", "chien", "dog"));

    FTCard Updated = MakeCard("id0", "chienne", "female dog");
    Updated.Definition = "a female dog";
    Updated.Gender = GenderFeminine;
    Updated.PartOfSpeech = "noun";
    Updated.LeitnerStage = 3;
    Updated.DueDate = 42;

    Store.UpdateCard(Updated);

    CHECK(Held->TargetWord == "chienne");
    CHECK(Held->Translation == "female dog");
    CHECK(Held->Definition == "a female dog");
    REQUIRE(Held->Gender.has_value());
    CHECK(*Held->Gender == GenderFeminine);
    CHECK(Held->PartOfSpeech == "noun");
    CHECK(Held->LeitnerStage == 3);
    REQUIRE(Held->DueDate.has_value());
    CHECK(*Held->DueDate == 42);
}
