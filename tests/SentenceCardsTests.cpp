#include <catch2/catch_test_macros.hpp>

#include "core/FTSentenceCards.h"

#include <set>
#include <string>
#include <vector>

namespace
{
    FTTreeSentence MakeSentence(uint32_t Id, std::string Text, int GapStart = -1, int GapLength = 0)
    {
        FTTreeSentence Sentence;
        Sentence.Id = Id;
        Sentence.Text = std::move(Text);
        Sentence.Translation = "translation";
        Sentence.GapStart = GapStart;
        Sentence.GapLength = GapLength;

        return Sentence;
    }

    std::string Words(int Count)
    {
        std::string Text;

        for (int Index = 0; Index < Count; ++Index)
        {
            Text += Index == 0 ? "mot" : " mot";
        }

        return Text + ".";
    }

    std::vector<uint32_t> Ids(const std::vector<FTTreeSentence>& Sentences)
    {
        std::vector<uint32_t> Result;

        for (const FTTreeSentence& Sentence : Sentences)
        {
            Result.push_back(Sentence.Id);
        }

        return Result;
    }
}

TEST_CASE("A sentence becomes a card with the sentence as its word", "[sentencecards]")
{
    FTTreeSentence Sentence = MakeSentence(42, "Je pense avec ma tête.");
    Sentence.Translation = "I think with my head.";

    const FTCard Card = MakeSentenceCard(Sentence);

    CHECK(Card.Id == "sentence:42");
    CHECK(Card.TargetWord == "Je pense avec ma tête.");
    CHECK(Card.Translation == "I think with my head.");
    CHECK_FALSE(Card.Gender.has_value());
    CHECK(Card.PartOfSpeech.empty());
    CHECK(GetDisplayWord(Card, "fr") == "Je pense avec ma tête.");
    CHECK(GetDisplayTranslation(Card, "en") == "I think with my head.");
}

TEST_CASE("A sentence card is told apart from a stored card by its id", "[sentencecards]")
{
    CHECK(IsSentenceCardId(MakeSentenceCardId(7)));
    CHECK(IsSentenceCardId("sentence:123456"));
    CHECK_FALSE(IsSentenceCardId("7"));
    CHECK_FALSE(IsSentenceCardId("b7e2-uuid"));
    CHECK_FALSE(IsSentenceCardId(""));
}

TEST_CASE("A sentence splits into its words without punctuation", "[sentencecards]")
{
    using FWords = std::vector<std::string>;

    CHECK(SplitSentenceWords("Je mange une pomme.") == FWords{"Je", "mange", "une", "pomme"});
    CHECK(SplitSentenceWords("Est-ce qu'il dort ?") == FWords{"Est-ce", "qu'il", "dort"});
    CHECK(SplitSentenceWords("J'ai mangé l'avocat !") == FWords{"J'ai", "mangé", "l'avocat"});
    CHECK(SplitSentenceWords("Oui, bien sûr ; merci.") == FWords{"Oui", "bien", "sûr", "merci"});
    CHECK(SplitSentenceWords("« Bonjour », dit-il.") == FWords{"Bonjour", "dit-il"});
    CHECK(SplitSentenceWords("Il a 20 ans.") == FWords{"Il", "a", "20", "ans"});
    CHECK(SplitSentenceWords("Je gagne 100 € par jour.") == FWords{"Je", "gagne", "100", "par", "jour"});
    CHECK(SplitSentenceWords("Qui est-ce ?") == FWords{"Qui", "est-ce"});
    CHECK(SplitSentenceWords("  ").empty());
    CHECK(SplitSentenceWords("").empty());
}

TEST_CASE("The gap blanks one word of the sentence", "[sentencecards]")
{
    const FTTreeSentence Sentence = MakeSentence(1, "Je mange une pomme.", 3, 5);

    CHECK(HasGap(Sentence));
    CHECK(GetGapWord(Sentence) == "mange");
    CHECK(MakeGapPrompt(Sentence) == "Je ___ une pomme.");
    CHECK(MakeGapSpeech(Sentence) == "Je … une pomme.");

    const FTGapParts Parts = SplitAtGap(Sentence);

    CHECK(Parts.Before == "Je ");
    CHECK(Parts.Word == "mange");
    CHECK(Parts.After == " une pomme.");
}

TEST_CASE("The gap counts accented letters as one character", "[sentencecards]")
{
    const FTTreeSentence Sentence = MakeSentence(1, "L'été était très chaud.", 6, 5);

    CHECK(GetGapWord(Sentence) == "était");
    CHECK(MakeGapPrompt(Sentence) == "L'été ___ très chaud.");
}

TEST_CASE("The gap counts a character outside the basic plane as two units", "[sentencecards]")
{
    const FTTreeSentence Sentence = MakeSentence(1, "Posons 𝑥 égal à 1.", 10, 4);

    CHECK(GetGapWord(Sentence) == "égal");
    CHECK(MakeGapPrompt(Sentence) == "Posons 𝑥 ___ à 1.");
}

TEST_CASE("A sentence without a usable gap is left whole", "[sentencecards]")
{
    CHECK_FALSE(HasGap(MakeSentence(1, "Je mange.")));
    CHECK_FALSE(HasGap(MakeSentence(1, "Je mange.", 3, 0)));
    CHECK_FALSE(HasGap(MakeSentence(1, "Je mange.", 3, 50)));
    CHECK_FALSE(HasGap(MakeSentence(1, "Je mange.", -1, 3)));

    CHECK(GetGapWord(MakeSentence(1, "Je mange.", 3, 50)).empty());
    CHECK(SplitAtGap(MakeSentence(1, "Je mange.", 3, 50)).Before == "Je mange.");
    CHECK(SplitAtGap(MakeSentence(1, "Je mange.", 3, 50)).After.empty());
    CHECK(MakeGapPrompt(MakeSentence(1, "Je mange.", 3, 50)) == "Je mange.");
    CHECK(MakeGapSpeech(MakeSentence(1, "Je mange.", 3, 50)) == "Je mange.");
}

TEST_CASE("A typed sentence is compared without its punctuation and capitals", "[sentencecards]")
{
    const std::string Expected = "Est-ce qu'il dort ?";

    CHECK(IsCorrectGrade(CompareSentence("Est-ce qu'il dort ?", Expected)));
    CHECK(IsCorrectGrade(CompareSentence("est-ce qu'il dort", Expected)));
    CHECK(IsCorrectGrade(CompareSentence("Est-ce qu’il dort?", Expected)));
    CHECK(IsCorrectGrade(CompareSentence("  est-ce   qu'il dort !  ", Expected)));

    CHECK_FALSE(IsCorrectGrade(CompareSentence("Est-ce qu'il mange ?", Expected)));
    CHECK_FALSE(IsCorrectGrade(CompareSentence("Est-ce qu'il", Expected)));
    CHECK_FALSE(IsCorrectGrade(CompareSentence("", Expected)));
}

TEST_CASE("A sentence with a comma is one answer, not a list of them", "[sentencecards]")
{
    const std::string Expected = "Oui, je viens demain.";

    CHECK(IsCorrectGrade(CompareSentence("oui je viens demain", Expected)));
    CHECK_FALSE(IsCorrectGrade(CompareSentence("oui", Expected)));
    CHECK_FALSE(IsCorrectGrade(CompareSentence("je viens demain", Expected)));
}

TEST_CASE("A typed sentence follows the accent rule", "[sentencecards]")
{
    FTAnswerComparisonRules Strict;
    Strict.bRequireExactAccents = true;

    FTAnswerComparisonRules Lenient;
    Lenient.bRequireExactAccents = false;

    CHECK_FALSE(IsCorrectGrade(CompareSentence("il etait la", "Il était là.", Strict)));
    CHECK(IsCorrectGrade(CompareSentence("il etait la", "Il était là.", Lenient)));
    CHECK(IsCorrectGrade(CompareSentence("il était là", "Il était là.", Strict)));
}

TEST_CASE("Picking takes the sentences closest to each other in length", "[sentencecards]")
{
    std::vector<FTTreeSentence> Candidates{
        MakeSentence(1, Words(3)),
        MakeSentence(2, Words(9)),
        MakeSentence(3, Words(6)),
        MakeSentence(4, Words(12)),
        MakeSentence(5, Words(6)),
        MakeSentence(6, Words(7)),
        MakeSentence(7, Words(2)),
    };

    CHECK(Ids(PickBalancedSentences(Candidates, 3, 1)) == std::vector<uint32_t>{3, 5, 6});
    CHECK(Ids(PickBalancedSentences(Candidates, 2, 1)) == std::vector<uint32_t>{3, 5});
}

TEST_CASE("Picking among equally close groups reaches every group", "[sentencecards]")
{
    std::vector<FTTreeSentence> Candidates{
        MakeSentence(1, Words(3)),
        MakeSentence(2, Words(3)),
        MakeSentence(3, Words(6)),
        MakeSentence(4, Words(6)),
        MakeSentence(5, Words(9)),
        MakeSentence(6, Words(9)),
    };

    std::set<std::vector<uint32_t>> Seen;

    for (uint64_t Seed = 0; Seed < 200; ++Seed)
    {
        Seen.insert(Ids(PickBalancedSentences(Candidates, 2, Seed)));
    }

    CHECK(Seen == std::set<std::vector<uint32_t>>{{1, 2}, {3, 4}, {5, 6}});
}

TEST_CASE("Picking from one large group reaches every member", "[sentencecards]")
{
    std::vector<FTTreeSentence> Candidates;

    for (uint32_t Id = 1; Id <= 40; ++Id)
    {
        Candidates.push_back(MakeSentence(Id, Words(3)));
    }

    std::set<uint32_t> Seen;

    for (uint64_t Seed = 0; Seed < 200; ++Seed)
    {
        const std::vector<uint32_t> Picked = Ids(PickBalancedSentences(Candidates, 10, Seed));

        CHECK(Picked.size() == 10);
        Seen.insert(Picked.begin(), Picked.end());
    }

    CHECK(Seen.size() == 40);
}

TEST_CASE("Picking among equal lengths prefers the same number of changed words",
          "[sentencecards]")
{
    std::vector<FTTreeSentence> Candidates{
        MakeSentence(1, Words(5)),
        MakeSentence(2, Words(5)),
        MakeSentence(3, Words(5)),
        MakeSentence(4, Words(5)),
        MakeSentence(5, Words(7)),
    };

    Candidates[0].ChangedCount = 4;
    Candidates[1].ChangedCount = 0;
    Candidates[2].ChangedCount = 1;
    Candidates[3].ChangedCount = 0;

    CHECK(Ids(PickBalancedSentences(Candidates, 3, 1)) == std::vector<uint32_t>{2, 3, 4});
}

TEST_CASE("Picking keeps every candidate when there are too few", "[sentencecards]")
{
    std::vector<FTTreeSentence> Candidates{
        MakeSentence(1, Words(3)),
        MakeSentence(2, Words(9)),
    };

    CHECK(Ids(PickBalancedSentences(Candidates, 10, 1)) == std::vector<uint32_t>{1, 2});
    CHECK(Ids(PickBalancedSentences(Candidates, 2, 1)) == std::vector<uint32_t>{1, 2});
    CHECK(PickBalancedSentences(Candidates, 0, 1).empty());
    CHECK(PickBalancedSentences({}, 10, 1).empty());
}

TEST_CASE("A meaning example gaps the card's word", "[sentencecards]")
{
    FTDictionarySense Meaning;
    Meaning.Example = "Je prends mon parapluie avant de sortir.";
    Meaning.ExampleTranslation = "I take my umbrella before going out.";

    const FTTreeSentence Sentence = MakeMeaningSentence(Meaning, {"prendre", "prends", "pris"});

    REQUIRE(HasGap(Sentence));
    CHECK(GetGapWord(Sentence) == "prends");
    CHECK(Sentence.Translation == "I take my umbrella before going out.");
}

TEST_CASE("A meaning example gap ignores case and elision but not a longer word", "[sentencecards]")
{
    FTDictionarySense Meaning;

    Meaning.Example = "Prends ton temps.";
    CHECK(GetGapWord(MakeMeaningSentence(Meaning, {"prends"})) == "Prends");

    Meaning.Example = "J'ai faim.";
    CHECK(GetGapWord(MakeMeaningSentence(Meaning, {"avoir", "ai"})) == "ai");

    Meaning.Example = "Une faim de loup.";
    CHECK_FALSE(HasGap(MakeMeaningSentence(Meaning, {"ai"})));
}

TEST_CASE("A meaning example gaps the longest spelling it holds", "[sentencecards]")
{
    FTDictionarySense Meaning;
    Meaning.Example = "Il a mangé des pommes de terre.";

    CHECK(GetGapWord(MakeMeaningSentence(Meaning, {"pomme de terre", "pommes de terre"})) == "pommes de terre");
}

TEST_CASE("A noun gap prefers the occurrence after a determiner", "[sentencecards]")
{
    FTTreeSentence Sentence;
    Sentence.Text = "Ils étaient en train de descendre du train.";

    REQUIRE(PlaceGap(Sentence, {"train"}, true));
    CHECK(MakeGapPrompt(Sentence) == "Ils étaient en train de descendre du ___.");

    REQUIRE(PlaceGap(Sentence, {"train"}));
    CHECK(MakeGapPrompt(Sentence) == "Ils étaient en ___ de descendre du train.");

    Sentence.Text = "En train de lire, il a raté l\xE2\x80\x99" "avion.";
    REQUIRE(PlaceGap(Sentence, {"avion"}, true));
    CHECK(GetGapWord(Sentence) == "avion");

    Sentence.Text = "Il est en train de partir.";
    REQUIRE(PlaceGap(Sentence, {"train"}, true));
    CHECK(MakeGapPrompt(Sentence) == "Il est en ___ de partir.");
}
