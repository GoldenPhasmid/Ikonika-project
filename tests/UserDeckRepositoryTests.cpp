#include <catch2/catch_test_macros.hpp>

#include "core/FTCard.h"
#include "core/FTCardRepository.h"
#include "core/FTDatabase.h"
#include "core/FTSchema.h"
#include "core/FTUserDeckRepository.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>

namespace
{
    struct Fixture
    {
        Fixture()
        {
            std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
            REQUIRE(Opened.has_value());

            Target = std::make_unique<FTDatabase>(std::move(*Opened));
            REQUIRE(MigrateSchema(*Target));

            Cards = std::make_unique<FTCardRepository>(*Target);
            Cards->SetActiveLanguages("fr", "en");

            Repository = std::make_unique<FTUserDeckRepository>(*Target, [this]() { return FakeNow; });
            Repository->SetActiveLanguages("fr", "en");
        }

        std::string AddCard(const std::string& Word, const std::string& Translation)
        {
            FTCard Card;
            Card.TargetWord = Word;
            Card.Translation = Translation;

            const std::optional<std::string> Id = Cards->AddCard(Card);
            REQUIRE(Id.has_value());

            return *Id;
        }

        std::optional<FTUserDeckRow> FindDeck(const std::string& Id) const
        {
            const std::vector<FTUserDeckRow> Decks = Repository->ListDecks();
            const auto Found = std::find_if(Decks.begin(), Decks.end(), [&](const FTUserDeckRow& Row)
            {
                return Row.Id == Id;
            });

            return Found == Decks.end() ? std::nullopt : std::optional<FTUserDeckRow>{*Found};
        }

        std::unique_ptr<FTDatabase> Target;
        std::unique_ptr<FTCardRepository> Cards;
        std::unique_ptr<FTUserDeckRepository> Repository;

        int64_t FakeNow = 1000;
    };
}

TEST_CASE("A deck holds the cards added to it", "[userdeck]")
{
    Fixture Test;

    const std::optional<std::string> DeckId = Test.Repository->CreateDeck("Cafe", "");
    REQUIRE(DeckId.has_value());

    const std::string Chien = Test.AddCard("chien", "dog");
    const std::string Chat = Test.AddCard("chat", "cat");

    REQUIRE(Test.Repository->AddCard(*DeckId, Chien));
    REQUIRE(Test.Repository->AddCard(*DeckId, Chat));

    const std::optional<FTUserDeckRow> Deck = Test.FindDeck(*DeckId);

    REQUIRE(Deck.has_value());
    CHECK(Deck->Name == "Cafe");
    CHECK(Deck->CardCount == 2);
    CHECK(Deck->PoolCount == 2);
    CHECK(Test.Repository->ListCardIds(*DeckId) == std::vector<std::string>{Chat, Chien});
}

TEST_CASE("Adding a card to a second deck moves it there", "[userdeck]")
{
    Fixture Test;

    const std::string DeckA = *Test.Repository->CreateDeck("A", "");
    const std::string DeckB = *Test.Repository->CreateDeck("B", "");
    const std::string Chien = Test.AddCard("chien", "dog");

    REQUIRE(Test.Repository->AddCard(DeckA, Chien));
    REQUIRE(Test.Repository->AddCard(DeckB, Chien));

    CHECK(Test.FindDeck(DeckA)->CardCount == 0);
    CHECK(Test.FindDeck(DeckB)->CardCount == 1);

    REQUIRE(Test.Repository->RemoveCard(DeckA, Chien));
    CHECK(Test.FindDeck(DeckB)->CardCount == 1);

    REQUIRE(Test.Repository->DeleteDeck(DeckA));
    CHECK(Test.FindDeck(DeckB)->CardCount == 1);
}

TEST_CASE("A deck keeps its description, and an edit without a name is refused", "[userdeck]")
{
    Fixture Test;

    const std::string DeckId = *Test.Repository->CreateDeck("Airport", "");
    CHECK(Test.FindDeck(DeckId)->Description.empty());

    const auto DescribedDeckId{Test.Repository->CreateDeck("Hotels", "", "Words for booking a room")};
    REQUIRE(DescribedDeckId.has_value());
    CHECK(Test.FindDeck(*DescribedDeckId)->Description == "Words for booking a room");

    REQUIRE(Test.Repository->EditDeck(DeckId, "Terminal", "Words for flying"));
    CHECK(Test.FindDeck(DeckId)->Name == "Terminal");
    CHECK(Test.FindDeck(DeckId)->Description == "Words for flying");

    CHECK_FALSE(Test.Repository->EditDeck(DeckId, "", "Nothing"));
    CHECK(Test.FindDeck(DeckId)->Description == "Words for flying");
}

TEST_CASE("Only the cards of decks taken out of daily learning are listed as outside it", "[userdeck]")
{
    Fixture Test;

    const std::string Kept = *Test.Repository->CreateDeck("Kept", "");
    const std::string Left = *Test.Repository->CreateDeck("Left", "");
    const std::string Chien = Test.AddCard("chien", "dog");
    const std::string Chat = Test.AddCard("chat", "cat");
    Test.AddCard("oiseau", "bird");

    REQUIRE(Test.Repository->AddCard(Kept, Chien));
    REQUIRE(Test.Repository->AddCard(Left, Chat));
    CHECK(Test.FindDeck(Left)->bDailyLearning);
    CHECK(Test.Repository->ListCardIdsOutsideDailyLearning().empty());

    REQUIRE(Test.Repository->SetDailyLearning(Left, false));
    CHECK_FALSE(Test.FindDeck(Left)->bDailyLearning);
    CHECK(Test.Repository->ListCardIdsOutsideDailyLearning() == std::unordered_set<std::string>{Chat});

    REQUIRE(Test.Repository->DeleteDeck(Left));
    CHECK(Test.Repository->ListCardIdsOutsideDailyLearning().empty());
}

TEST_CASE("Adding a card twice leaves one link, and removing it frees the slot again", "[userdeck]")
{
    Fixture Test;

    const std::string DeckId = *Test.Repository->CreateDeck("A", "");
    const std::string Chien = Test.AddCard("chien", "dog");

    REQUIRE(Test.Repository->AddCard(DeckId, Chien));
    REQUIRE(Test.Repository->AddCard(DeckId, Chien));
    CHECK(Test.FindDeck(DeckId)->CardCount == 1);

    REQUIRE(Test.Repository->RemoveCard(DeckId, Chien));
    CHECK(Test.FindDeck(DeckId)->CardCount == 0);

    REQUIRE(Test.Repository->AddCard(DeckId, Chien));
    CHECK(Test.FindDeck(DeckId)->CardCount == 1);
}

TEST_CASE("Deleting a deck keeps its cards in My Vocabulary", "[userdeck]")
{
    Fixture Test;

    const std::string DeckId = *Test.Repository->CreateDeck("A", "");
    const std::string Chien = Test.AddCard("chien", "dog");

    REQUIRE(Test.Repository->AddCard(DeckId, Chien));
    REQUIRE(Test.Repository->DeleteDeck(DeckId));

    CHECK_FALSE(Test.FindDeck(DeckId).has_value());
    CHECK(Test.Repository->ListCardIds(DeckId).empty());
    CHECK(Test.Cards->LoadCards().size() == 1);
}

TEST_CASE("A removed card no longer counts in its deck", "[userdeck]")
{
    Fixture Test;

    const std::string DeckId = *Test.Repository->CreateDeck("A", "");
    const std::string Chien = Test.AddCard("chien", "dog");

    REQUIRE(Test.Repository->AddCard(DeckId, Chien));
    REQUIRE(Test.Target->Execute("UPDATE Cards SET Deleted = 1"));

    CHECK(Test.FindDeck(DeckId)->CardCount == 0);
    CHECK(Test.Repository->ListCardIds(DeckId).empty());
}

TEST_CASE("A deck counts its cards by where they stand", "[userdeck]")
{
    Fixture Test;

    const std::string DeckId = *Test.Repository->CreateDeck("A", "");
    const std::string Pool = Test.AddCard("chien", "dog");
    const std::string Learning = Test.AddCard("chat", "cat");
    const std::string Learned = Test.AddCard("oiseau", "bird");

    for (const std::string& CardId : {Pool, Learning, Learned})
    {
        REQUIRE(Test.Repository->AddCard(DeckId, CardId));
    }

    REQUIRE(Test.Target->Execute("UPDATE Cards SET LeitnerStage = 2 WHERE Id = '" + Learning + "'"));
    REQUIRE(Test.Target->Execute("UPDATE Cards SET LeitnerStage = 4, CompletedDate = 100 WHERE Id = '" + Learned + "'"));

    const std::optional<FTUserDeckRow> Deck = Test.FindDeck(DeckId);

    REQUIRE(Deck.has_value());
    CHECK(Deck->CardCount == 3);
    CHECK(Deck->PoolCount == 1);
    CHECK(Deck->LearningCount == 1);
    CHECK(Deck->LearnedCount == 1);
}

TEST_CASE("A deck sits in My Decks until it is moved into a category", "[userdeck]")
{
    Fixture Test;

    const std::optional<std::string> Category = Test.Repository->CreateCategory("Voyage", 2);
    REQUIRE(Category.has_value());

    const std::string DeckId = *Test.Repository->CreateDeck("Airport", "");
    CHECK(Test.FindDeck(DeckId)->CategoryId.empty());

    REQUIRE(Test.Repository->MoveDeck(DeckId, *Category));
    CHECK(Test.FindDeck(DeckId)->CategoryId == *Category);
}

TEST_CASE("Deleting a category sends its decks back to My Decks", "[userdeck]")
{
    Fixture Test;

    const std::string Category = *Test.Repository->CreateCategory("Voyage", 2);
    const std::string DeckId = *Test.Repository->CreateDeck("Airport", Category);

    REQUIRE(Test.Repository->DeleteCategory(Category));

    CHECK(Test.Repository->ListCategories().empty());
    REQUIRE(Test.FindDeck(DeckId).has_value());
    CHECK(Test.FindDeck(DeckId)->CategoryId.empty());
}

TEST_CASE("Categories and decks can be renamed, and an empty name is refused", "[userdeck]")
{
    Fixture Test;

    const std::string Category = *Test.Repository->CreateCategory("Voyage", 2);
    const std::string DeckId = *Test.Repository->CreateDeck("Airport", Category);

    REQUIRE(Test.Repository->RenameCategory(Category, "Trips"));
    REQUIRE(Test.Repository->RenameDeck(DeckId, "Terminal"));

    CHECK(Test.Repository->ListCategories()[0].Name == "Trips");
    CHECK(Test.FindDeck(DeckId)->Name == "Terminal");

    CHECK_FALSE(Test.Repository->RenameCategory(Category, ""));
    CHECK_FALSE(Test.Repository->RenameDeck(DeckId, ""));
    CHECK_FALSE(Test.Repository->CreateDeck("", "").has_value());
    CHECK_FALSE(Test.Repository->CreateCategory("", 0).has_value());
}

TEST_CASE("Decks and categories belong to the language pair they were made under", "[userdeck]")
{
    Fixture Test;

    REQUIRE(Test.Repository->CreateDeck("French deck", "").has_value());
    REQUIRE(Test.Repository->CreateCategory("French category", 0).has_value());

    Test.Repository->SetActiveLanguages("es", "en");

    CHECK(Test.Repository->ListDecks().empty());
    CHECK(Test.Repository->ListCategories().empty());

    REQUIRE(Test.Repository->CreateDeck("Spanish deck", "").has_value());
    CHECK(Test.Repository->ListDecks().size() == 1);

    Test.Repository->SetActiveLanguages("fr", "en");

    REQUIRE(Test.Repository->ListDecks().size() == 1);
    CHECK(Test.Repository->ListDecks()[0].Name == "French deck");
    CHECK(Test.Repository->ListDecks()[0].CreatedDate > 0);
}

TEST_CASE("Deleting a language removes its decks, categories and links", "[userdeck]")
{
    Fixture Test;

    const std::string Category = *Test.Repository->CreateCategory("Voyage", 2);
    const std::string DeckId = *Test.Repository->CreateDeck("Airport", Category);
    REQUIRE(Test.Repository->AddCard(DeckId, Test.AddCard("chien", "dog")));

    REQUIRE(Test.Cards->DeleteLanguage("fr"));

    CHECK(Test.Repository->ListDecks().empty());
    CHECK(Test.Repository->ListCategories().empty());

    FTStatement Links = Test.Target->Prepare("SELECT COUNT(*) FROM UserDeckCards");
    REQUIRE(Links.Step());
    CHECK(Links.GetInt(0) == 0);
}
