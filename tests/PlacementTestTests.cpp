#include <catch2/catch_test_macros.hpp>

#include "core/FTCoreCards.h"
#include "core/FTPlacementTest.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace
{
    constexpr int FirstRank = 100;
    constexpr int BandCount = 8;
    constexpr int WordsPerBand = 15;

    constexpr double CutoffLevelOffset = 0.6 * 0.4055;

    std::vector<FTPlacementWord> MakeWords()
    {
        std::vector<FTPlacementWord> Words;

        int Lower = FirstRank;

        for (int Band = 0; Band < BandCount; ++Band)
        {
            for (int Index = 0; Index < WordsPerBand; ++Index)
            {
                FTPlacementWord Word;
                Word.TeachingRank = Lower + Lower * Index / WordsPerBand;
                Word.Word = "mot" + std::to_string(Word.TeachingRank);
                Word.Translation = "word" + std::to_string(Word.TeachingRank);

                Words.push_back(std::move(Word));
            }

            Lower *= 2;
        }

        return Words;
    }

    struct FTSimulatedRun
    {
        int CutoffRank = 0;
        int AnsweredCount = 0;
    };

    FTSimulatedRun RunLearner(int HalfKnownRank, uint32_t Seed)
    {
        const double Level = std::log(static_cast<double>(HalfKnownRank));

        std::mt19937 Luck(Seed);
        std::uniform_real_distribution<double> Roll(0.0, 1.0);

        FTPlacementTest Test(MakeWords(), Seed + 1, FTPlacementTest::BeginnerStartingRank);

        while (!Test.IsFinished())
        {
            Test.Answer(Roll(Luck) < GetPlacementKnownChance(Test.GetCurrentWord()->TeachingRank, Level));
        }

        return { Test.GetCutoffRank(), Test.GetAnsweredCount() };
    }
}

TEST_CASE("Placement words are read from the tab-separated list", "[placement]")
{
    const std::vector<FTPlacementWord> Words = ParsePlacementWords(
        "word\tpart_of_speech\tgender\ttranslation\tteaching_rank\r\n"
        "chien\tnoun\t0\tdog\t121\r\n"
        "penser\tverb\t\tthink\t250\n"
        "cassé\tadj\t\tbroken\n"
        "vide\tadj\t\tempty\t0\n"
        "\n"
        "rue\tnoun\t1\tstreet\t101\n");

    REQUIRE(Words.size() == 3);

    CHECK(Words[0].Word == "chien");
    CHECK(Words[0].PartOfSpeech == "noun");
    CHECK(Words[0].Gender == 0);
    CHECK(Words[0].Translation == "dog");
    CHECK(Words[0].TeachingRank == 121);

    CHECK(Words[1].Word == "penser");
    CHECK_FALSE(Words[1].Gender.has_value());

    CHECK(Words[2].Gender == 1);
}

TEST_CASE("A placement test with no words is finished at once", "[placement]")
{
    FTPlacementTest Test({}, 1, FTPlacementTest::BeginnerStartingRank);

    CHECK(Test.IsFinished());
    CHECK(Test.GetCurrentWord() == nullptr);
    CHECK(Test.GetCutoffRank() == 0);
}

TEST_CASE("A learner who knows nothing is let go after a few words with no cutoff", "[placement]")
{
    FTPlacementTest Test(MakeWords(), 7, FTPlacementTest::BeginnerStartingRank);

    while (!Test.IsFinished())
    {
        Test.Answer(false);
    }

    CHECK(Test.GetAnsweredCount() == 5);
    CHECK(Test.GetCutoffRank() == 0);
}

TEST_CASE("A placement test never asks more than its limit or the same word twice", "[placement]")
{
    FTPlacementTest Test(MakeWords(), 11, FTPlacementTest::BeginnerStartingRank);

    std::set<std::string> Asked;
    bool bKnown = true;

    while (!Test.IsFinished())
    {
        CHECK(Asked.insert(Test.GetCurrentWord()->Word).second);

        Test.Answer(bKnown);
        bKnown = !bKnown;
    }

    CHECK(Test.GetAnsweredCount() <= FTPlacementTest::MaximumQuestions);
}

TEST_CASE("A learner who knows every word gets the highest rank of the list", "[placement]")
{
    FTPlacementTest Test(MakeWords(), 3, FTPlacementTest::BeginnerStartingRank);

    while (!Test.IsFinished())
    {
        Test.Answer(true);
    }

    const std::vector<FTPlacementWord> Words = MakeWords();

    CHECK(Test.GetCutoffRank() > Words.back().TeachingRank / 4);
    CHECK(Test.GetCutoffRank() <= Words.back().TeachingRank);
}

TEST_CASE("The cutoff lands near the rank a simulated learner knows most words below", "[placement][slow]")
{
    constexpr int LearnerCount = 300;

    for (const int HalfKnownRank : { 600, 1500, 4000, 10000 })
    {
        const double Expected = std::exp(std::log(static_cast<double>(HalfKnownRank)) - CutoffLevelOffset);

        std::vector<double> Ratios;
        int WithinDouble = 0;

        for (int Learner = 0; Learner < LearnerCount; ++Learner)
        {
            const FTSimulatedRun Run = RunLearner(HalfKnownRank, static_cast<uint32_t>(HalfKnownRank + Learner * 17));

            CHECK(Run.AnsweredCount <= FTPlacementTest::MaximumQuestions);

            const double Ratio = std::max(Run.CutoffRank, 1) / Expected;

            Ratios.push_back(Ratio);

            if (Ratio > 0.5 && Ratio < 2.0)
            {
                ++WithinDouble;
            }
        }

        std::sort(Ratios.begin(), Ratios.end());

        const double Median = Ratios[Ratios.size() / 2];

        INFO("half-known rank " << HalfKnownRank << ", median ratio " << Median
             << ", within a factor of two " << WithinDouble << " of " << LearnerCount);

        CHECK(Median > 0.75);
        CHECK(Median < 1.35);
        CHECK(WithinDouble >= LearnerCount * 85 / 100);
    }
}

TEST_CASE("A beginner who knows a few hundred words is not sent back to the first words", "[placement]")
{
    constexpr int LearnerCount = 300;
    constexpr int HalfKnownRank = 400;

    int Placed = 0;
    std::vector<int> Cutoffs;

    for (int Learner = 0; Learner < LearnerCount; ++Learner)
    {
        const FTSimulatedRun Run = RunLearner(HalfKnownRank, static_cast<uint32_t>(9000 + Learner * 13));

        Cutoffs.push_back(Run.CutoffRank);

        if (Run.CutoffRank > 0)
        {
            ++Placed;
        }
    }

    std::sort(Cutoffs.begin(), Cutoffs.end());

    INFO("placed " << Placed << " of " << LearnerCount << ", median cutoff " << Cutoffs[Cutoffs.size() / 2]);

    CHECK(Placed >= LearnerCount * 90 / 100);
    CHECK(Cutoffs[Cutoffs.size() / 2] > HalfKnownRank / 5);
    CHECK(Cutoffs[Cutoffs.size() / 2] < HalfKnownRank);
}

TEST_CASE("The first word asked is a common one", "[placement]")
{
    for (uint32_t Seed = 1; Seed <= 20; ++Seed)
    {
        const FTPlacementTest Test(MakeWords(), Seed, FTPlacementTest::BeginnerStartingRank);

        CHECK(Test.GetCurrentWord()->TeachingRank < 1000);
    }
}

TEST_CASE("The cutoff maps onto four levels and never reaches C1", "[placement]")
{
    CHECK(GetPlacementCefrLevel(0) == 0);
    CHECK(GetPlacementCefrLevel(1499) == 0);
    CHECK(GetPlacementCefrLevel(1500) == 1);
    CHECK(GetPlacementCefrLevel(3999) == 1);
    CHECK(GetPlacementCefrLevel(4000) == 2);
    CHECK(GetPlacementCefrLevel(8999) == 2);
    CHECK(GetPlacementCefrLevel(9000) == 3);
    CHECK(GetPlacementCefrLevel(60000) == 3);
}

TEST_CASE("Answers are reported from the most common word to the rarest", "[placement]")
{
    FTPlacementTest Test(MakeWords(), 5, FTPlacementTest::BeginnerStartingRank);

    bool bKnown = true;

    while (!Test.IsFinished())
    {
        Test.Answer(bKnown);
        bKnown = !bKnown;
    }

    const std::vector<FTPlacementAnswer> Answers = Test.GetAnswersByRank();

    REQUIRE(static_cast<int>(Answers.size()) == Test.GetAnsweredCount());
    CHECK(std::is_sorted(Answers.begin(), Answers.end(), [](const FTPlacementAnswer& Left, const FTPlacementAnswer& Right)
    {
        return Left.TeachingRank < Right.TeachingRank;
    }));
}

TEST_CASE("The placement test asks its first word near the chosen level's starting rank", "[placement]")
{
    for (uint32_t Seed = 1; Seed <= 10; ++Seed)
    {
        const FTPlacementTest Beginner(MakeWords(), Seed, FTPlacementTest::BeginnerStartingRank);
        const FTPlacementTest Intermediate(MakeWords(), Seed, FTPlacementTest::IntermediateStartingRank);
        const FTPlacementTest Advanced(MakeWords(), Seed, FTPlacementTest::AdvancedStartingRank);

        REQUIRE(Beginner.GetCurrentWord() != nullptr);
        REQUIRE(Intermediate.GetCurrentWord() != nullptr);
        REQUIRE(Advanced.GetCurrentWord() != nullptr);

        CHECK(Beginner.GetCurrentWord()->TeachingRank < 600);
        CHECK(Intermediate.GetCurrentWord()->TeachingRank > 1400);
        CHECK(Intermediate.GetCurrentWord()->TeachingRank < 3000);
        CHECK(Advanced.GetCurrentWord()->TeachingRank > Intermediate.GetCurrentWord()->TeachingRank);
    }
}

TEST_CASE("Core cards are wanted by a beginner who ended below the limit", "[placement]")
{
    CHECK(PlacementWantsCoreCards(BeginnerPlacementLevel, 0));
    CHECK(PlacementWantsCoreCards(BeginnerPlacementLevel, CoreCardsCutoffLimit - 1));
    CHECK_FALSE(PlacementWantsCoreCards(BeginnerPlacementLevel, CoreCardsCutoffLimit));
    CHECK_FALSE(PlacementWantsCoreCards(BeginnerPlacementLevel + 1, 0));
}

TEST_CASE("Core cards are read from the rows that name a word, a reading and a translation", "[placement]")
{
    const std::string Text =
        "word\tpart_of_speech\ttranslation\tmeaning_gist\n"
        "je\tpron\tI\tsujet\r\n"
        "\n"
        "\tpron\tI\tsujet\n"
        "tu\tpron\t\tfamilier\n"
        "parce que\tconj\tbecause\tcause\n"
        "il\tpron\the\n"
        "elle\tpron\n";

    const std::vector<FTCoreCard> Cards = ParseCoreCards(Text);

    REQUIRE(Cards.size() == 3);
    CHECK(Cards[0].Word == "je");
    CHECK(Cards[0].PartOfSpeech == "pron");
    CHECK(Cards[0].Translation == "I");
    CHECK(Cards[0].MeaningGist == "sujet");
    CHECK(Cards[1].Word == "parce que");
    CHECK(Cards[1].MeaningGist == "cause");
    CHECK(Cards[2].Word == "il");
    CHECK(Cards[2].MeaningGist.empty());
}
