#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "core/FTRepetitionBatch.h"
#include "core/FTSettings.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <string>
#include <utility>

namespace
{
    struct ReviewWordCountGuard
    {
        ~ReviewWordCountGuard()
        {
            const FTRepetitionRules Defaults;
            FTSettings::Get().SetReviewWordCount(Defaults.ReviewWordCount);
        }
    };

    std::vector<FTCardHandle> MakeCards(int Count)
    {
        std::vector<FTCardHandle> Result;

        for (int Index = 0; Index < Count; ++Index)
        {
            FTCard Card;
            Card.Id = "id" + std::to_string(Index);
            Card.TargetWord = "mot" + std::to_string(Index);

            Result.push_back(MakeCardHandle(std::move(Card)));
        }

        return Result;
    }

    /** Count of a spelling, per batch. */
    std::vector<int> CountsOf(const std::vector<std::vector<FTCardHandle>>& Batches,
                             const std::string& Spelling)
    {
        std::vector<int> Counts;
        Counts.reserve(Batches.size());

        for (const std::vector<FTCardHandle>& Batch : Batches)
        {
            int Count = 0;

            for (const FTCardHandle& Card : Batch)
            {
                if (Card->TargetWord == Spelling)
                {
                    ++Count;
                }
            }

            Counts.push_back(Count);
        }

        return Counts;
    }
}

TEST_CASE("No due cards means no batches", "[repetition]")
{
    CHECK(ComputeRepetitionBatchSizes(0).empty());
    CHECK(ComputeRepetitionBatchSizes(-5).empty());
}

TEST_CASE("A total at or under the cap is always one batch", "[repetition]")
{
    CHECK(ComputeRepetitionBatchSizes(1) == std::vector<int>{1});
    CHECK(ComputeRepetitionBatchSizes(25) == std::vector<int>{25});
    CHECK(ComputeRepetitionBatchSizes(30) == std::vector<int>{30});
}

TEST_CASE("A total splits into as few batches as land near the target", "[repetition]")
{
    // Three batches at the default target of 20, not two of 29: the split aims at the target
    // rather than filling batches up to the cap.
    CHECK(ComputeRepetitionBatchSizes(58) == std::vector<int>{20, 19, 19});
    CHECK(ComputeRepetitionBatchSizes(59) == std::vector<int>{20, 20, 19});
}

TEST_CASE("The review word count is what the batch size follows", "[repetition]")
{
    ReviewWordCountGuard Guard;

    FTSettings::Get().SetReviewWordCount(10);
    CHECK(GetTargetRepetitionBatchSize() == 10);
    CHECK(GetMaximumRepetitionBatchSize() == 15);
    CHECK(ComputeRepetitionBatchSizes(58) == std::vector<int>{10, 10, 10, 10, 9, 9});

    FTSettings::Get().SetReviewWordCount(25);
    CHECK(GetTargetRepetitionBatchSize() == 25);
    CHECK(GetMaximumRepetitionBatchSize() == 38);
    CHECK(ComputeRepetitionBatchSizes(58) == std::vector<int>{29, 29});
}

TEST_CASE("A total at or under the cap stays one batch whatever the review word count",
          "[repetition]")
{
    ReviewWordCountGuard Guard;

    FTSettings::Get().SetReviewWordCount(10);
    CHECK(ComputeRepetitionBatchSizes(15) == std::vector<int>{15});

    FTSettings::Get().SetReviewWordCount(25);
    CHECK(ComputeRepetitionBatchSizes(38) == std::vector<int>{38});
}

TEST_CASE("No multi-batch split ever reaches the cap, across a wide range of totals",
          "[repetition]")
{
    // Starts just above the cap: a total at or under it is always one batch of exactly that
    // size by design (see "A total at or under the cap is always one batch"), which can
    // legitimately equal the cap itself. The invariant under test only holds once splitting
    // actually happens.
    for (int Total = GetMaximumRepetitionBatchSize() + 1; Total <= 500; ++Total)
    {
        const std::vector<int> Sizes = ComputeRepetitionBatchSizes(Total);

        for (const int Size : Sizes)
        {
            REQUIRE(Size < GetMaximumRepetitionBatchSize());
        }
    }
}

TEST_CASE("Batch sizes always sum back to the total", "[repetition]")
{
    for (const int Total : {1, 5, 24, 25, 26, 30, 31, 49, 50, 51, 58, 59, 74, 75, 76, 200, 347})
    {
        const std::vector<int> Sizes = ComputeRepetitionBatchSizes(Total);
        const int Sum = std::accumulate(Sizes.begin(), Sizes.end(), 0);

        CHECK(Sum == Total);
    }
}

TEST_CASE("Batch sizes differ from each other by at most one card", "[repetition]")
{
    for (const int Total : {58, 59, 74, 76, 101, 347})
    {
        const std::vector<int> Sizes = ComputeRepetitionBatchSizes(Total);

        const auto [Least, Most] = std::minmax_element(Sizes.begin(), Sizes.end());

        CHECK(*Most - *Least <= 1);
    }
}

TEST_CASE("Splitting preserves every card exactly once", "[repetition]")
{
    const std::vector<std::vector<FTCardHandle>> Batches = SplitIntoRepetitionBatches(MakeCards(59), 1u);

    std::set<std::string> SeenIds;
    int TotalCards = 0;

    for (const std::vector<FTCardHandle>& Batch : Batches)
    {
        for (const FTCardHandle& Card : Batch)
        {
            SeenIds.insert(Card->Id);
            ++TotalCards;
        }
    }

    CHECK(TotalCards == 59);
    CHECK(SeenIds.size() == 59);
}

TEST_CASE("Splitting produces batches matching the computed sizes", "[repetition]")
{
    const std::vector<std::vector<FTCardHandle>> Batches = SplitIntoRepetitionBatches(MakeCards(59), 1u);
    const std::vector<int> ExpectedSizes = ComputeRepetitionBatchSizes(59);

    REQUIRE(Batches.size() == ExpectedSizes.size());

    for (size_t Index = 0; Index < Batches.size(); ++Index)
    {
        CHECK(Batches[Index].size() == static_cast<size_t>(ExpectedSizes[Index]));
    }
}

TEST_CASE("The same seed splits the same way", "[repetition]")
{
    const std::vector<std::vector<FTCardHandle>> First = SplitIntoRepetitionBatches(MakeCards(59), 42u);
    const std::vector<std::vector<FTCardHandle>> Second = SplitIntoRepetitionBatches(MakeCards(59), 42u);

    REQUIRE(First.size() == Second.size());

    for (size_t Index = 0; Index < First.size(); ++Index)
    {
        REQUIRE(First[Index].size() == Second[Index].size());

        for (size_t Row = 0; Row < First[Index].size(); ++Row)
        {
            CHECK(First[Index][Row]->Id == Second[Index][Row]->Id);
        }
    }
}

namespace
{
    std::vector<std::string> IdsOf(const std::vector<FTCardHandle>& Batch)
    {
        std::vector<std::string> Ids;
        Ids.reserve(Batch.size());

        for (const FTCardHandle& Card : Batch)
        {
            Ids.push_back(Card->Id);
        }

        return Ids;
    }
}

TEST_CASE("A different seed can produce a different split", "[repetition]")
{
    // Not a strict guarantee for any single pair of seeds, but across enough due cards two
    // independent shuffles landing on an identical order is not something to rely on.
    const std::vector<std::vector<FTCardHandle>> First = SplitIntoRepetitionBatches(MakeCards(59), 1u);
    const std::vector<std::vector<FTCardHandle>> Second = SplitIntoRepetitionBatches(MakeCards(59), 2u);

    CHECK(IdsOf(First[0]) != IdsOf(Second[0]));
}

TEST_CASE("An empty due list produces no batches", "[repetition]")
{
    CHECK(SplitIntoRepetitionBatches({}, 1u).empty());
}

TEST_CASE("Identical spellings spread across batches when there is room", "[repetition]")
{
    ReviewWordCountGuard Guard;
    FTSettings::Get().SetReviewWordCount(20);

    // 80 due cards, four batches of 20 by the default target - and among them, four readings of
    // the same spelling, exactly one per batch if spread out perfectly.
    std::vector<FTCardHandle> Cards = MakeCards(76);

    for (int Index = 0; Index < 4; ++Index)
    {
        FTCard Card;
        Card.Id = "dup" + std::to_string(Index);
        Card.TargetWord = "identique";

        Cards.push_back(MakeCardHandle(std::move(Card)));
    }

    const std::vector<std::vector<FTCardHandle>> Batches = SplitIntoRepetitionBatches(Cards, 1u);
    REQUIRE(Batches.size() == 4);

    const std::vector<int> Counts = CountsOf(Batches, "identique");

    CHECK(std::accumulate(Counts.begin(), Counts.end(), 0) == 4);

    for (const int Count : Counts)
    {
        CHECK(Count == 1);
    }
}

TEST_CASE("Case alone does not make two spellings count as different", "[repetition]")
{
    ReviewWordCountGuard Guard;
    FTSettings::Get().SetReviewWordCount(20);

    const auto [LowerWord, UpperWord] = GENERATE(std::pair<std::string, std::string>{"identique", "Identique"},
                                                 std::pair<std::string, std::string>{"été", "Été"});

    std::vector<FTCardHandle> Cards = MakeCards(76);

    FTCard Lower;
    Lower.Id = "dup0";
    Lower.TargetWord = LowerWord;
    Cards.push_back(MakeCardHandle(std::move(Lower)));

    FTCard Upper;
    Upper.Id = "dup1";
    Upper.TargetWord = UpperWord;
    Cards.push_back(MakeCardHandle(std::move(Upper)));

    const std::vector<std::vector<FTCardHandle>> Batches = SplitIntoRepetitionBatches(Cards, 1u);

    for (const std::vector<FTCardHandle>& Batch : Batches)
    {
        int Count = 0;

        for (const FTCardHandle& Card : Batch)
        {
            if (Card->Id == "dup0" || Card->Id == "dup1")
            {
                ++Count;
            }
        }

        CHECK(Count <= 1);
    }
}

TEST_CASE("A spelling group bigger than the batch count doubles up only where it must",
         "[repetition]")
{
    ReviewWordCountGuard Guard;
    FTSettings::Get().SetReviewWordCount(20);

    // Same shape as the spread-out test, but five readings of the same spelling against only
    // four batches: one batch has no choice but to hold two of them.
    std::vector<FTCardHandle> Cards = MakeCards(75);

    for (int Index = 0; Index < 5; ++Index)
    {
        FTCard Card;
        Card.Id = "dup" + std::to_string(Index);
        Card.TargetWord = "identique";

        Cards.push_back(MakeCardHandle(std::move(Card)));
    }

    const std::vector<std::vector<FTCardHandle>> Batches = SplitIntoRepetitionBatches(Cards, 1u);
    REQUIRE(Batches.size() == 4);

    const std::vector<int> Counts = CountsOf(Batches, "identique");

    CHECK(std::accumulate(Counts.begin(), Counts.end(), 0) == 5);

    // Every batch gets at least one before any batch gets a second: doubling up is the last
    // resort, not spread evenly across whichever batches happen to need it least.
    for (const int Count : Counts)
    {
        CHECK(Count >= 1);
        CHECK(Count <= 2);
    }
}

TEST_CASE("Earlier boxes fill the earlier batches first", "[repetition]")
{
    ReviewWordCountGuard Guard;
    FTSettings::Get().SetReviewWordCount(20);

    // 80 cards, 20 in each of boxes 1 through 4 - divides evenly into four batches of 20, so each
    // batch ends up holding exactly one box's worth.
    std::vector<FTCardHandle> Cards;

    for (int Stage = 1; Stage <= 4; ++Stage)
    {
        for (int Index = 0; Index < 20; ++Index)
        {
            FTCard Card;
            Card.Id = "stage" + std::to_string(Stage) + "-" + std::to_string(Index);
            Card.TargetWord = "mot" + std::to_string(Stage) + "-" + std::to_string(Index);
            Card.LeitnerStage = Stage;

            Cards.push_back(MakeCardHandle(std::move(Card)));
        }
    }

    const std::vector<std::vector<FTCardHandle>> Batches = SplitIntoRepetitionBatches(Cards, 1u);
    REQUIRE(Batches.size() == 4);

    for (size_t Index = 0; Index < Batches.size(); ++Index)
    {
        for (const FTCardHandle& Card : Batches[Index])
        {
            CHECK(Card->LeitnerStage == static_cast<int>(Index) + 1);
        }
    }
}

TEST_CASE("A box that straddles two batches still keeps earlier boxes ahead of later ones",
         "[repetition]")
{
    ReviewWordCountGuard Guard;
    FTSettings::Get().SetReviewWordCount(20);

    // 30 cards in box 1, 30 in box 2: 60 total, three batches of 20. Box 1 fills the first batch
    // and part of the second; box 2 never appears before box 1 has run out.
    std::vector<FTCardHandle> Cards;

    for (int Index = 0; Index < 30; ++Index)
    {
        FTCard Card;
        Card.Id = "a" + std::to_string(Index);
        Card.TargetWord = "mota" + std::to_string(Index);
        Card.LeitnerStage = 1;

        Cards.push_back(MakeCardHandle(std::move(Card)));
    }

    for (int Index = 0; Index < 30; ++Index)
    {
        FTCard Card;
        Card.Id = "b" + std::to_string(Index);
        Card.TargetWord = "motb" + std::to_string(Index);
        Card.LeitnerStage = 2;

        Cards.push_back(MakeCardHandle(std::move(Card)));
    }

    const std::vector<std::vector<FTCardHandle>> Batches = SplitIntoRepetitionBatches(Cards, 1u);
    REQUIRE(Batches.size() == 3);

    int LastBox1Batch = -1;
    int FirstBox2Batch = static_cast<int>(Batches.size());

    for (size_t Index = 0; Index < Batches.size(); ++Index)
    {
        for (const FTCardHandle& Card : Batches[Index])
        {
            if (Card->LeitnerStage == 1)
            {
                LastBox1Batch = std::max(LastBox1Batch, static_cast<int>(Index));
            }
            else
            {
                FirstBox2Batch = std::min(FirstBox2Batch, static_cast<int>(Index));
            }
        }
    }

    CHECK(LastBox1Batch <= FirstBox2Batch);
}

TEST_CASE("Splitting still preserves every card exactly once with repeated spellings",
         "[repetition]")
{
    std::vector<FTCardHandle> Cards = MakeCards(55);

    for (int Index = 0; Index < 4; ++Index)
    {
        FTCard Card;
        Card.Id = "dup" + std::to_string(Index);
        Card.TargetWord = "identique";

        Cards.push_back(MakeCardHandle(std::move(Card)));
    }

    const std::vector<std::vector<FTCardHandle>> Batches = SplitIntoRepetitionBatches(Cards, 1u);

    std::set<std::string> SeenIds;
    int TotalCards = 0;

    for (const std::vector<FTCardHandle>& Batch : Batches)
    {
        for (const FTCardHandle& Card : Batch)
        {
            SeenIds.insert(Card->Id);
            ++TotalCards;
        }
    }

    CHECK(TotalCards == 59);
    CHECK(SeenIds.size() == 59);
}

namespace
{
    FTCardHandle MakePoolCard(const std::string& Id, const std::string& Word, const std::string& PartOfSpeech)
    {
        FTCard Card;
        Card.Id = Id;
        Card.TargetWord = Word;
        Card.PartOfSpeech = PartOfSpeech;

        return MakeCardHandle(std::move(Card));
    }
}

TEST_CASE("A training session takes one card per spelling", "[repetition]")
{
    const std::vector<FTCardHandle> Pool{
        MakePoolCard("mer1", "mer", "noun"), MakePoolCard("mer2", "Mer", "noun"), MakePoolCard("chat", "chat", "noun"),
        MakePoolCard("ete1", "été", "noun"), MakePoolCard("ete2", "Été", "noun")};

    CHECK(IdsOf(PickTrainingCards(Pool, 5)) == std::vector<std::string>{"mer1", "chat", "ete1"});
}

TEST_CASE("A training session mixes the parts of speech of the pool", "[repetition]")
{
    const std::vector<FTCardHandle> Pool{
        MakePoolCard("n1", "chat", "noun"), MakePoolCard("n2", "chien", "noun"), MakePoolCard("n3", "maison", "noun"),
        MakePoolCard("v1", "manger", "verb"), MakePoolCard("a1", "grand", "adj"), MakePoolCard("d1", "vite", "adv"),
        MakePoolCard("p1", "sous", "prep"), MakePoolCard("v2", "boire", "verb")};

    CHECK(IdsOf(PickTrainingCards(Pool, 5)) == std::vector<std::string>{"n1", "v1", "a1", "d1", "p1"});
    CHECK(IdsOf(PickTrainingCards(Pool, 8)) == std::vector<std::string>{"n1", "v1", "a1", "d1", "p1", "n2", "v2", "n3"});
}
