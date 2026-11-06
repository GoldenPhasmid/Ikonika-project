#include "core/FCardSelectorBase.h"

#include <algorithm>
#include <random>

uint32_t MakeRandomSeed()
{
    std::random_device Device;

    return static_cast<uint32_t>(Device());
}

std::optional<int> FSequenceSelector::Next()
{
    while (Current < Selectors.size())
    {
        if (auto Result = Selectors[Current]->Next(); Result.has_value())
        {
            return Result;
        }

        ++Current;
    }

    return std::nullopt;
}

FEachOnceSelector::FEachOnceSelector(int32 Count, bool bInfinite)
{
    Reset(Count, bInfinite);
}

void FEachOnceSelector::Reset(int32 InCount, bool bInInfinite)
{
    Count = InCount;
    bInfinite = bInInfinite;
    Entries.resize(0);
    Init();
}

FEachOnceSelector::~FEachOnceSelector()
{
    
}

std::optional<int> FEachOnceSelector::Next()
{
    if (Count <= 0)
    {
        return std::nullopt;
    }
    
    if (bInfinite == false && Entries.empty())
    {
        return std::nullopt;
    }

    if (Entries.empty())
    {
        Init();
    }

    const int32 Value = Entries.back();
    Entries.pop_back();
    
    return Value;
}

void FEachOnceSelector::Init()
{
    Entries.reserve(Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
       Entries.push_back(Index);
    }

    std::mt19937 Generator{MakeRandomSeed()};
    std::shuffle(Entries.begin(), Entries.end(), Generator);
}

static int32 CountGroupedEntries(const std::vector<std::vector<int32>>& Groups)
{
    int32 Total = 0;

    for (const std::vector<int32>& Group : Groups)
    {
        Total += static_cast<int32>(Group.size());
    }

    return Total;
}

FGroupedSelector::FGroupedSelector(std::vector<std::vector<int32>> InGroups, bool bInInfinite, bool bInShuffled)
    : FEachOnceSelector(CountGroupedEntries(InGroups), bInInfinite)
    , Groups(std::move(InGroups))
    , bShuffled(bInShuffled)
{
    Entries.clear();
    Init();
}

void FGroupedSelector::Init()
{
    std::vector<std::vector<int32>> Ordered = Groups;

    if (bShuffled)
    {
        std::mt19937 Generator{MakeRandomSeed()};

        std::shuffle(Ordered.begin(), Ordered.end(), Generator);

        for (std::vector<int32>& Group : Ordered)
        {
            std::shuffle(Group.begin(), Group.end(), Generator);
        }
    }

    Entries.clear();
    Entries.reserve(static_cast<size_t>(Count));

    for (auto Group = Ordered.rbegin(); Group != Ordered.rend(); ++Group)
    {
        Entries.insert(Entries.end(), Group->rbegin(), Group->rend());
    }
}

FWeightedSelector::FWeightedSelector(int32 Count, const FTSelectionWeights& InWeights)
{
    Reset(Count, InWeights);
}

void FWeightedSelector::Reset(int32 Count, const FTSelectionWeights& InWeights)
{
    Params = InWeights;
    CurrentWeights.assign(Count, DefaultWeight);
}

void FWeightedSelector::SetWeight(int32 Index, double Weight)
{
    if (Index >= 0 && Index < CurrentWeights.size())
    {
        CurrentWeights[Index] = Weight;
    }
}

std::optional<int> FWeightedSelector::Next()
{
    if (CurrentWeights.empty())
    {
        return std::nullopt;
    }

    double TotalWeight = 0;
    for (double Weight : CurrentWeights)
    {
        TotalWeight += Weight;
    }
    
    std::uniform_real_distribution<double> Distribution(0.0, TotalWeight);

    static thread_local std::default_random_engine engine{MakeRandomSeed()};
    double Value = Distribution(engine);
    
    int32 Index = 0;
    for (double CurrentWeight = 0.f; Index < CurrentWeights.size(); ++Index)
    {
        CurrentWeight += CurrentWeights[Index];
        if (CurrentWeight > Value)
        {
            if (Index != LastSelectedIndex && LastSelectedIndex >= 0 && LastSelectedIndex < CurrentWeights.size())
            {
                // restore to original weight
                CurrentWeights[LastSelectedIndex] = DefaultWeight;                
            }

            LastSelectedIndex = Index;
            CurrentWeights[Index] /= Params.ReductionFactor;
            
            break;
        }
    }

    if (Index >= CurrentWeights.size())
    {
        return std::nullopt;
    }

    return Index;
}
