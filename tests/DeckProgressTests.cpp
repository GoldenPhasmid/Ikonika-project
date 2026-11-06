#include <catch2/catch_test_macros.hpp>

#include "core/FTCard.h"
#include "core/FTDeckProgress.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
    FTDeckWord MakeDeckWord(std::string Headword, std::string PartOfSpeech, std::string Gist = {})
    {
        FTDeckWord Word;
        Word.Headword = std::move(Headword);
        Word.PartOfSpeech = std::move(PartOfSpeech);
        Word.Gist = std::move(Gist);

        return Word;
    }

    FTCardHandle MakeCard(std::string TargetWord, std::string PartOfSpeech, std::string Gist, int Stage,
                          bool bCompleted = false)
    {
        auto Card = std::make_shared<FTCard>();
        Card->TargetWord = std::move(TargetWord);
        Card->PartOfSpeech = std::move(PartOfSpeech);
        Card->MeaningGist = std::move(Gist);
        Card->LeitnerStage = Stage;

        if (bCompleted)
        {
            Card->CompletedDate = 100;
        }

        return Card;
    }
}

TEST_CASE("A deck word without a gist takes the furthest card of its reading", "[decks]")
{
    const std::vector<FTCardHandle> Cards{
        MakeCard("light", "noun", "lamp", 1),
        MakeCard("Light", "noun", "brightness", 3),
        MakeCard("light", "adj", "pale", 2, true),
    };

    const std::vector<FTDeckWordProgress> Progress =
        MatchDeckWords({MakeDeckWord("light", "noun")}, Cards, {});

    REQUIRE(Progress.size() == 1);
    REQUIRE(Progress[0].Card);
    CHECK(Progress[0].Card->MeaningGist == "brightness");
    CHECK_FALSE(Progress[0].bKnown);
}

TEST_CASE("A deck word with a gist takes a card of any meaning of its reading", "[decks]")
{
    const std::vector<FTCardHandle> Cards{MakeCard("light", "noun", "lamp", 2)};

    const std::vector<FTDeckWordProgress> Progress = MatchDeckWords(
        {MakeDeckWord("light", "noun", "lamp"), MakeDeckWord("light", "noun", "brightness"),
         MakeDeckWord("light", "verb", "ignite")},
        Cards, {{"light", "noun"}});

    REQUIRE(Progress.size() == 3);
    CHECK(Progress[0].Card);
    REQUIRE(Progress[1].Card);
    CHECK(Progress[1].Card->MeaningGist == "lamp");
    CHECK_FALSE(Progress[1].bKnown);
    CHECK_FALSE(Progress[2].Card);
}

TEST_CASE("A card with no part of speech counts under every reading", "[decks]")
{
    const std::vector<FTCardHandle> Cards{MakeCard("house", "", "", 1)};

    const std::vector<FTDeckWordProgress> Progress =
        MatchDeckWords({MakeDeckWord("house", "noun"), MakeDeckWord("house", "verb")}, Cards, {});

    CHECK(Progress[0].Card);
    CHECK(Progress[1].Card);
}

TEST_CASE("A deck word marked known with no card of its reading is known", "[decks]")
{
    const std::vector<FTDeckWordProgress> Progress = MatchDeckWords(
        {MakeDeckWord("abandon", "verb"), MakeDeckWord("abandon", "noun"), MakeDeckWord("ability", "noun")},
        {}, {{"Abandon", "verb"}});

    CHECK(Progress[0].bKnown);
    CHECK_FALSE(Progress[1].bKnown);
    CHECK_FALSE(Progress[2].bKnown);
}
