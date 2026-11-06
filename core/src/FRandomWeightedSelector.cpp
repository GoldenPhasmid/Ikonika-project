#include "core/FRandomWeightedSelector.h"

#include <algorithm>

FRandomWeightedSelector::FRandomWeightedSelector(int InCardCount, uint32_t Seed,
                                                 const FTSelectionWeights& InWeights)
    : CardCount(std::max(InCardCount, 0))
    , Weights(InWeights)
    , Generator(Seed)
{
    CurrentWeights.assign(static_cast<size_t>(CardCount), Weights.StartingWeight);
}

std::optional<int> FRandomWeightedSelector::Next()
{
    if (CardCount <= 0)
    {
        return std::nullopt;
    }

    // The previous word is excluded so nothing appears twice in a row. With a single word there
    // is nothing else to offer, so it does repeat.
    const bool bExcludePrevious = CardCount > 1 && PreviousIndex >= 0;

    const auto IsCandidate = [this, bExcludePrevious](int Index)
    {
        return !bExcludePrevious || Index != PreviousIndex;
    };

    double TotalWeight = 0.0;
    int LastCandidate = -1;

    for (int Index = 0; Index < CardCount; ++Index)
    {
        if (!IsCandidate(Index))
        {
            continue;
        }

        TotalWeight += CurrentWeights[static_cast<size_t>(Index)];
        LastCandidate = Index;
    }

    if (LastCandidate < 0)
    {
        return std::nullopt;
    }

    // Falling back to the last candidate covers floating point rounding leaving the walk below
    // without a hit.
    int Selected = LastCandidate;

    if (TotalWeight > 0.0)
    {
        std::uniform_real_distribution<double> Distribution(0.0, TotalWeight);
        double Remaining = Distribution(Generator);

        for (int Index = 0; Index < CardCount; ++Index)
        {
            if (!IsCandidate(Index))
            {
                continue;
            }

            Remaining -= CurrentWeights[static_cast<size_t>(Index)];

            if (Remaining <= 0.0)
            {
                Selected = Index;
                break;
            }
        }
    }
    else
    {
        // Every candidate sits at zero weight, which only happens if MinimumWeight is set to
        // zero. Pick uniformly so the session cannot stall.
        std::vector<int> Candidates;
        Candidates.reserve(static_cast<size_t>(CardCount));

        for (int Index = 0; Index < CardCount; ++Index)
        {
            if (IsCandidate(Index))
            {
                Candidates.push_back(Index);
            }
        }

        std::uniform_int_distribution<size_t> Pick(0, Candidates.size() - 1);
        Selected = Candidates[Pick(Generator)];
    }

    for (int Index = 0; Index < CardCount; ++Index)
    {
        double& Weight = CurrentWeights[static_cast<size_t>(Index)];

        if (Index == Selected)
        {
            Weight = std::max(Weight * Weights.SelectionDecay, Weights.MinimumWeight);
            continue;
        }

        Weight = std::min(Weight + Weights.RecoveryStep, Weights.MaximumWeight);
    }

    PreviousIndex = Selected;
    ++SelectedCount;

    return Selected;
}

bool FRandomWeightedSelector::IsFinished() const
{
    return CardCount <= 0;
}

int FRandomWeightedSelector::GetCardCount() const
{
    return CardCount;
}

int FRandomWeightedSelector::GetSelectedCount() const
{
    return SelectedCount;
}

void FRandomWeightedSelector::AdjustWeight(int Index, double Delta)
{
    if (Index < 0 || Index >= static_cast<int>(CurrentWeights.size()))
    {
        return;
    }

    double& Weight = CurrentWeights[static_cast<size_t>(Index)];
    Weight = std::clamp(Weight + Delta, Weights.MinimumWeight, Weights.MaximumWeight);
}

double FRandomWeightedSelector::GetWeight(int Index) const
{
    if (Index < 0 || Index >= static_cast<int>(CurrentWeights.size()))
    {
        return 0.0;
    }

    return CurrentWeights[static_cast<size_t>(Index)];
}
