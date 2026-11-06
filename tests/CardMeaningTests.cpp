#include <catch2/catch_test_macros.hpp>

#include "core/FTCardMeaning.h"

#include <optional>
#include <string>
#include <vector>

namespace
{
    FTDictionarySense MakeMeaning(std::string Gist, std::vector<std::string> Translations,
                                  std::optional<int> Gender = std::nullopt)
    {
        FTDictionarySense Meaning;
        Meaning.Gist = std::move(Gist);
        Meaning.Gender = Gender;

        for (std::string& Text : Translations)
        {
            Meaning.Translations.push_back({std::move(Text), std::nullopt});
        }

        return Meaning;
    }

    FTCard MakeCard(std::string Translation, std::string Gist = {}, std::optional<int> Gender = std::nullopt)
    {
        FTCard Card;
        Card.TargetWord = "prendre";
        Card.PartOfSpeech = "verb";
        Card.Translation = std::move(Translation);
        Card.MeaningGist = std::move(Gist);
        Card.Gender = Gender;

        return Card;
    }

    const std::vector<FTDictionarySense> Prendre{
        MakeMeaning("saisir", {"to take", "to grab"}),
        MakeMeaning("manger", {"to have", "to eat"}),
        MakeMeaning("emporter", {"to take away", "to take"}),
    };
}

TEST_CASE("A card's gist finds its meaning", "[meaning]")
{
    const FTDictionarySense* Found = FindCardMeaning(MakeCard("eat", "manger"), Prendre);

    REQUIRE(Found != nullptr);
    CHECK(Found->Gist == "manger");

    CHECK(FindCardMeaning(MakeCard("eat"), Prendre) == nullptr);
    CHECK(FindCardMeaning(MakeCard("eat", "boire"), Prendre) == nullptr);
}

TEST_CASE("A translation that belongs to one meaning names it", "[meaning]")
{
    CHECK(MatchMeaningGist(MakeCard("eat"), Prendre) == "manger");
    CHECK(MatchMeaningGist(MakeCard("to grab"), Prendre) == "saisir");

    CHECK(MatchMeaningGist(MakeCard("Have, to eat"), Prendre) == "manger");
}

TEST_CASE("A translation shared by two meanings, or by none, names nothing", "[meaning]")
{
    CHECK(MatchMeaningGist(MakeCard("to take"), Prendre).empty());
    CHECK(MatchMeaningGist(MakeCard("grab, eat"), Prendre).empty());
    CHECK(MatchMeaningGist(MakeCard("to drink"), Prendre).empty());
    CHECK(MatchMeaningGist(MakeCard("eat"), {}).empty());
}

TEST_CASE("A card without a clear meaning takes the one its most popular translation belongs to", "[meaning]")
{
    std::vector<FTDictionarySense> Meanings = Prendre;
    Meanings[0].Translations[0].Probability = 0.2;
    Meanings[1].Translations[1].Probability = 0.5;
    Meanings[2].Translations[1].Probability = 0.4;

    CHECK(PickPopularMeaningGist(MakeCard("to take"), Meanings) == "emporter");
    CHECK(PickPopularMeaningGist(MakeCard("grab, eat"), Meanings) == "manger");
    CHECK(PickPopularMeaningGist(MakeCard("to drink"), Meanings) == "manger");
    CHECK(PickPopularMeaningGist(MakeCard("to drink"), {}).empty());
}

TEST_CASE("A popular meaning of the other gender is not picked", "[meaning]")
{
    std::vector<FTDictionarySense> Moule{
        MakeMeaning("forme", {"mould"}, 0),
        MakeMeaning("coquillage", {"mussel"}, 1),
    };
    Moule[0].Translations[0].Probability = 0.9;
    Moule[1].Translations[0].Probability = 0.1;

    CHECK(PickPopularMeaningGist(MakeCard("shellfish", {}, 1), Moule) == "coquillage");
}

TEST_CASE("A meaning of the other gender is not the card's", "[meaning]")
{
    const std::vector<FTDictionarySense> Moule{
        MakeMeaning("forme", {"mould"}, 0),
        MakeMeaning("coquillage", {"mussel"}, 1),
    };

    CHECK(MatchMeaningGist(MakeCard("mussel", {}, 0), Moule).empty());
    CHECK(MatchMeaningGist(MakeCard("mussel", {}, 1), Moule) == "coquillage");
    CHECK(FindCardMeaning(MakeCard("mussel", "coquillage", 0), Moule) == nullptr);
}

TEST_CASE("Meanings are ordered by how often their translations are used", "[meaning]")
{
    FTDictionarySense Salvation = MakeMeaning("délivrance", {"salvation", "redemption"});
    Salvation.Translations[0].Probability = 0.02;
    Salvation.Translations[1].Probability = 0.001;

    FTDictionarySense Greeting = MakeMeaning("bonjour", {"hi"});
    Greeting.Translations[0].Probability = 0.39;

    FTDictionarySense Rescue = MakeMeaning("sauvetage", {"rescue"});
    FTDictionarySense Wave = MakeMeaning("geste", {"wave"});

    std::vector<FTDictionarySense> Meanings{Rescue, Salvation, Wave, Greeting};

    SortMeaningsByUse(Meanings);

    REQUIRE(Meanings.size() == 4);
    CHECK(Meanings[0].Gist == "bonjour");
    CHECK(Meanings[1].Gist == "délivrance");
    CHECK(Meanings[2].Gist == "sauvetage");
    CHECK(Meanings[3].Gist == "geste");
}
