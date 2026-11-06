#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <utility>
#include <vector>

using int32 = int;

uint32_t MakeRandomSeed();

class FRandomSelectorBase
{
public:
    virtual ~FRandomSelectorBase() = default;
    
    virtual std::optional<int> Next() = 0;
};

class FSequenceSelector: public FRandomSelectorBase
{
public:
    FSequenceSelector(std::vector<std::unique_ptr<FRandomSelectorBase>> InSelectors)
        : Selectors(std::move(InSelectors))
    {}

    virtual std::optional<int> Next() override;
protected:
    std::vector<std::unique_ptr<FRandomSelectorBase>> Selectors;
    int32 Current = 0;
};

class FEachOnceSelector: public FRandomSelectorBase
{
public:
    FEachOnceSelector(int32 Count, bool bInfinite);

    void Reset(int32 InCount, bool bInInfinite);

    virtual ~FEachOnceSelector() override;
    virtual std::optional<int> Next() override;

    const std::vector<int32>& GetEntries() const { return Entries; }
    void SetEntries(std::vector<int32> InEntries) { Entries = std::move(InEntries); }

protected:
    virtual void Init();
    
    int32 Count = 0;
    bool bInfinite = false;
    std::vector<int32> Entries;
};

class FGroupedSelector: public FEachOnceSelector
{
public:
    FGroupedSelector(std::vector<std::vector<int32>> InGroups, bool bInInfinite, bool bInShuffled);

protected:
    void Init() override;

private:
    std::vector<std::vector<int32>> Groups;
    bool bShuffled = false;
};

struct FTSelectionWeights
{
    double ReductionFactor = 2;
};

class FWeightedSelector: public FRandomSelectorBase
{
public:
    FWeightedSelector(int32 Count, const FTSelectionWeights& InWeights = {});
    void Reset(int32 Count, const FTSelectionWeights& InWeights = {});

    void SetWeight(int32 Index, double Weight);

    virtual std::optional<int> Next() override;

protected:
    FTSelectionWeights Params;
    std::vector<double> CurrentWeights;
    double DefaultWeight = 1.0;
    int32 LastSelectedIndex = -1;
};
