#include <catch2/catch_test_macros.hpp>

#include "core/FCardSelectorBase.h"

#include <memory>
#include <random>
#include <set>
#include <vector>

namespace
{
    std::vector<int> DrainAll(FRandomSelectorBase& Selector)
    {
        std::vector<int> Picked;

        while (const std::optional<int> Index = Selector.Next())
        {
            Picked.push_back(*Index);
        }

        return Picked;
    }

    std::vector<int> TakeCount(FRandomSelectorBase& Selector, int Count)
    {
        std::vector<int> Picked;

        for (int Step = 0; Step < Count; ++Step)
        {
            const std::optional<int> Index = Selector.Next();
            REQUIRE(Index.has_value());

            Picked.push_back(*Index);
        }

        return Picked;
    }
}

TEST_CASE("Each once covers every card exactly once before finishing", "[selector]")
{
    FEachOnceSelector Selector(10, false);

    const std::vector<int> Picked = DrainAll(Selector);

    REQUIRE(Picked.size() == 10);

    const std::set<int> Distinct(Picked.begin(), Picked.end());
    CHECK(Distinct.size() == 10);
    CHECK(*Distinct.begin() == 0);
    CHECK(*Distinct.rbegin() == 9);

    CHECK_FALSE(Selector.Next().has_value());
    CHECK_FALSE(Selector.Next().has_value());
}

TEST_CASE("Each once shuffles rather than counting up", "[selector]")
{
    FEachOnceSelector Selector(10, false);

    const std::vector<int> Picked = DrainAll(Selector);
    const std::vector<int> InOrder{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    CHECK(Picked != InOrder);
}

TEST_CASE("Each once yields nothing for an empty group", "[selector]")
{
    FEachOnceSelector Selector(0, false);

    CHECK_FALSE(Selector.Next().has_value());
}

TEST_CASE("Each once recycles into a freshly shuffled pass when infinite", "[selector]")
{
    FEachOnceSelector Selector(4, true);

    const std::vector<int> Picked = TakeCount(Selector, 20);

    CHECK(Picked.size() == 20);

    const std::set<int> FirstPass(Picked.begin(), Picked.begin() + 4);
    CHECK(FirstPass.size() == 4);
}

TEST_CASE("Each once stops drawing when not infinite", "[selector]")
{
    FEachOnceSelector Selector(4, false);

    TakeCount(Selector, 4);

    CHECK_FALSE(Selector.Next().has_value());
}

TEST_CASE("Reset reshuffles a fresh pass over a new count", "[selector]")
{
    FEachOnceSelector Selector(4, false);

    DrainAll(Selector);

    Selector.Reset(6, false);

    const std::vector<int> Picked = DrainAll(Selector);

    REQUIRE(Picked.size() == 6);

    const std::set<int> Distinct(Picked.begin(), Picked.end());
    CHECK(Distinct.size() == 6);
}

TEST_CASE("Weighted selector always returns an in-range index", "[selector]")
{
    FWeightedSelector Selector(5);

    for (int Step = 0; Step < 500; ++Step)
    {
        const std::optional<int> Index = Selector.Next();

        REQUIRE(Index.has_value());
        CHECK(*Index >= 0);
        CHECK(*Index < 5);
    }
}

TEST_CASE("Weighted selector yields nothing for an empty group", "[selector]")
{
    FWeightedSelector Selector(0);

    CHECK_FALSE(Selector.Next().has_value());
}

TEST_CASE("A picked index is less likely to repeat immediately", "[selector]")
{
    int SameAgain = 0;

    for (int Trial = 0; Trial < 500; ++Trial)
    {
        FWeightedSelector Selector(2);

        const std::optional<int> A = Selector.Next();
        const std::optional<int> B = Selector.Next();

        if (A == B)
        {
            ++SameAgain;
        }
    }

    CHECK(SameAgain < 250);
}

TEST_CASE("Reset restores every weight to default", "[selector]")
{
    FWeightedSelector Selector(3);

    for (int Step = 0; Step < 20; ++Step)
    {
        Selector.Next();
    }

    Selector.Reset(3);

    std::vector<int> Counts(3, 0);

    for (int Step = 0; Step < 300; ++Step)
    {
        const std::optional<int> Index = Selector.Next();
        REQUIRE(Index.has_value());
        ++Counts[static_cast<size_t>(*Index)];
    }

    for (const int Count : Counts)
    {
        CHECK(Count > 0);
    }
}

TEST_CASE("Sequence selector draws from each selector in turn", "[selector]")
{
    std::vector<std::unique_ptr<FRandomSelectorBase>> Selectors;
    Selectors.push_back(std::make_unique<FEachOnceSelector>(2, false));
    Selectors.push_back(std::make_unique<FEachOnceSelector>(3, false));

    FSequenceSelector Selector(std::move(Selectors));

    const std::vector<int> Picked = DrainAll(Selector);

    CHECK(Picked.size() == 5);
    CHECK_FALSE(Selector.Next().has_value());
}

TEST_CASE("A system seed is available for real sessions", "[selector]")
{
    const uint32_t Seed = MakeRandomSeed();

    std::mt19937 Generator{Seed};

    CHECK(Generator() != Generator());
}

TEST_CASE("A grouped selector in order walks each group front to back", "[selector]")
{
    FGroupedSelector Selector({{0, 1, 2}, {3, 4, 5}}, false, false);

    CHECK(DrainAll(Selector) == std::vector<int>{0, 1, 2, 3, 4, 5});
    CHECK_FALSE(Selector.Next().has_value());
}

TEST_CASE("A shuffled grouped selector finishes one group before starting the next", "[selector]")
{
    for (int Run = 0; Run < 20; ++Run)
    {
        FGroupedSelector Selector({{0, 1, 2}, {3, 4, 5}, {6, 7, 8}}, false, true);

        const std::vector<int> Picked = DrainAll(Selector);

        REQUIRE(Picked.size() == 9);
        CHECK(std::set<int>(Picked.begin(), Picked.end()).size() == 9);

        for (size_t Start = 0; Start < Picked.size(); Start += 3)
        {
            CHECK(Picked[Start] / 3 == Picked[Start + 1] / 3);
            CHECK(Picked[Start] / 3 == Picked[Start + 2] / 3);
        }
    }
}

TEST_CASE("A grouped selector stays grouped when it recycles", "[selector]")
{
    FGroupedSelector Selector({{0, 1}, {2, 3}}, true, true);

    const std::vector<int> Picked = TakeCount(Selector, 12);

    for (size_t Start = 0; Start < Picked.size(); Start += 2)
    {
        CHECK(Picked[Start] / 2 == Picked[Start + 1] / 2);
        CHECK(Picked[Start] != Picked[Start + 1]);
    }
}

TEST_CASE("A grouped selector with no groups yields nothing", "[selector]")
{
    FGroupedSelector Selector({}, false, true);

    CHECK_FALSE(Selector.Next().has_value());
}
