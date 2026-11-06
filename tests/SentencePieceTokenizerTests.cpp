#include <catch2/catch_test_macros.hpp>

#include "core/FTSentencePieceTokenizer.h"

#include <string>
#include <vector>

namespace
{
    const std::string ModelDirectory = OPUS_MT_MODEL_DIRECTORY;

    const std::optional<FTSentencePieceTokenizer> Loaded()
    {
        static const std::optional<FTSentencePieceTokenizer> Tokenizer =
            FTSentencePieceTokenizer::Create(ModelDirectory);

        return Tokenizer;
    }
}

TEST_CASE("The tokenizer is loaded from the OPUS-MT model directory", "[translation][slow]")
{
    if (!Loaded().has_value())
    {
        WARN("the OPUS-MT model is absent, so the tokenizer is untested here");

        return;
    }

    // The merged vocabulary size, matching the width of the decoder's logits.
    CHECK(Loaded()->GetVocabularySize() == 59514);
    CHECK(Loaded()->GetDecoderStartToken() == 59513);
    CHECK(Loaded()->GetEndOfSentenceToken() == 0);
    CHECK(Loaded()->GetUnknownToken() == 1);
}

TEST_CASE("Common French text encodes to the expected ids, eos included", "[translation][slow]")
{
    if (!Loaded().has_value())
    {
        return;
    }

    const std::vector<std::pair<std::string, std::vector<int64_t>>> Vectors = {
        {"Je mange une pomme verte.", {131, 17575, 38, 25624, 14858, 3, 0}},
        {"Bonjour, comment allez-vous aujourd'hui ?", {8703, 2, 1027, 5682, 21, 682, 1416, 6, 1051, 99, 0}},
        {"Le chat dort sur le canapé.", {60, 9801, 20, 259, 75, 36, 19, 20936, 3, 0}},
        {"Nous allons au cinéma ce soir.", {326, 6904, 39, 10930, 66, 3939, 3, 0}},
        {"Été, hiver, printemps, automne.", {1948, 2096, 2, 15041, 2, 9032, 2, 25106, 3, 0}},
        {"Cœur, œuf, sœur.", {49, 40751, 2, 49, 33268, 2, 16088, 3, 0}},
        {"À bientôt !", {410, 8157, 291, 0}},
        {"Ça va bien, merci.", {1617, 740, 229, 2, 9614, 3, 0}},
        {"Hôtel de ville.", {7893, 5, 1235, 3, 0}},
        {"Qu'est-ce que c'est ?", {1276, 6, 82, 21, 429, 29, 137, 6, 82, 99, 0}},
        {"Le café était servi à midi.", {60, 6017, 295, 6695, 17, 15353, 3, 0}},
    };

    for (const auto& [Text, Expected] : Vectors)
    {
        CHECK(Loaded()->Encode(Text) == Expected);
    }
}

TEST_CASE("Whitespace runs collapse and trim exactly like the reference", "[translation]")
{
    if (!Loaded().has_value())
    {
        return;
    }

    // Newlines and repeated spaces become single spaces before the metaspace rewrite.
    CHECK(Loaded()->Encode("a\nb  c") == std::vector<int64_t>{15, 383, 137, 0});

    // Trailing whitespace is stripped, so the two forms encode identically.
    CHECK(Loaded()->Encode("À bientôt ! ") == Loaded()->Encode("À bientôt !"));
}

TEST_CASE("Characters absent from the spm merge into surface spans", "[translation]")
{
    if (!Loaded().has_value())
    {
        return;
    }

    // The apostrophe is unknown to the model: it becomes a merged unk surface that the vocab does
    // not hold either, so the whole span collapses to the unk token.
    CHECK(Loaded()->Encode("d’enfants") == std::vector<int64_t>{20, 1, 6989, 0});

    // The moon glyph is unknown to the spm but present in the merged vocab, so its merged surface
    // keeps a real id instead of the unk token.
    CHECK(Loaded()->Encode("☽x") == std::vector<int64_t>{49, 53492, 623, 0});
}

TEST_CASE("Empty and whitespace-only input still produce the eos token", "[translation]")
{
    if (!Loaded().has_value())
    {
        return;
    }

    CHECK(Loaded()->Encode("") == std::vector<int64_t>{0});
    CHECK(Loaded()->Encode("   ") == std::vector<int64_t>{0});
}

TEST_CASE("Decode maps ids back to text with spaces restored", "[translation]")
{
    if (!Loaded().has_value())
    {
        return;
    }

    CHECK(Loaded()->Decode({131, 17575, 38, 25624, 14858, 3, 0}) == "Je mange une pomme verte.");
    CHECK(Loaded()->Decode({410, 8157, 291}) == "À bientôt !");
    CHECK(Loaded()->Decode({15, 383, 137}) == "a b c");

    // Special tokens are dropped, matching skip_special_tokens=True.
    CHECK(Loaded()->Decode({1, 131, 17575, 59513, 0}) == "Je mange");
}

TEST_CASE("A missing model directory is refused rather than crashing", "[translation]")
{
    CHECK_FALSE(FTSentencePieceTokenizer::Create("this/path/does/not/exist").has_value());
    CHECK_FALSE(FTSentencePieceTokenizer::Create("").has_value());
}
