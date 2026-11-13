#include "core/FTLevelBelief.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
    constexpr int LowestLevelRank = 20;
    constexpr int HighestLevelRank = 60000;
    constexpr int LevelStepCount = 200;

    constexpr int StartingRank = 400;
    constexpr double StartingSpread = 2.0;

    constexpr double CutoffSpread = 0.8;

    double GetLevelAt(int Step)
    {
        const double Lowest = std::log(static_cast<double>(LowestLevelRank));
        const double Highest = std::log(static_cast<double>(HighestLevelRank));

        return Lowest + (Highest - Lowest) * Step / (LevelStepCount - 1);
    }

    double GetCutoffOffset()
    {
        return FTLevelBelief::Steepness
            * std::log(FTLevelBelief::KnownShareAtCutoff / (1.0 - FTLevelBelief::KnownShareAtCutoff));
    }
}

double GetPlacementKnownChance(int TeachingRank, double Level)
{
    const double Distance = (std::log(static_cast<double>(std::max(TeachingRank, 1))) - Level) / FTLevelBelief::Steepness;

    return 1.0 / (1.0 + std::exp(Distance));
}

FTLevelBelief::FTLevelBelief()
    : FTLevelBelief(std::log(static_cast<double>(StartingRank)), StartingSpread)
{
}

FTLevelBelief::FTLevelBelief(double Centre, double Spread)
    : Weights(LevelStepCount, 0.0)
{
    double Total = 0.0;

    for (int Step = 0; Step < LevelStepCount; ++Step)
    {
        const double Distance = (GetLevelAt(Step) - Centre) / Spread;

        Weights[static_cast<std::size_t>(Step)] = std::exp(-0.5 * Distance * Distance);
        Total += Weights[static_cast<std::size_t>(Step)];
    }

    for (double& Weight : Weights)
    {
        Weight /= Total;
    }
}

FTLevelBelief FTLevelBelief::FromCutoffRank(int CutoffRank)
{
    if (CutoffRank <= 0)
    {
        return FTLevelBelief{};
    }

    return FTLevelBelief{std::log(static_cast<double>(CutoffRank)) + GetCutoffOffset(), CutoffSpread};
}

FTLevelBelief FTLevelBelief::FromStartingRank(int InStartingRank)
{
    return FTLevelBelief{std::log(static_cast<double>(std::max(InStartingRank, 1))), StartingSpread};
}

void FTLevelBelief::Update(int TeachingRank, double ChanceIfKnown, double ChanceIfUnknown)
{
    double Total = 0.0;

    for (int Step = 0; Step < LevelStepCount; ++Step)
    {
        const double Known = GetPlacementKnownChance(TeachingRank, GetLevelAt(Step));

        Weights[static_cast<std::size_t>(Step)] *= ChanceIfKnown * Known + ChanceIfUnknown * (1.0 - Known);
        Total += Weights[static_cast<std::size_t>(Step)];
    }

    if (Total <= 0.0)
    {
        return;
    }

    for (double& Weight : Weights)
    {
        Weight /= Total;
    }
}

double FTLevelBelief::GetKnownChance(int TeachingRank) const
{
    double Chance = 0.0;

    for (int Step = 0; Step < LevelStepCount; ++Step)
    {
        Chance += Weights[static_cast<std::size_t>(Step)] * GetPlacementKnownChance(TeachingRank, GetLevelAt(Step));
    }

    return Chance;
}

double FTLevelBelief::GetMean() const
{
    double Mean = 0.0;

    for (int Step = 0; Step < LevelStepCount; ++Step)
    {
        Mean += Weights[static_cast<std::size_t>(Step)] * GetLevelAt(Step);
    }

    return Mean;
}

double FTLevelBelief::GetDeviation() const
{
    const double Mean = GetMean();

    double Variance = 0.0;

    for (int Step = 0; Step < LevelStepCount; ++Step)
    {
        const double Distance = GetLevelAt(Step) - Mean;

        Variance += Weights[static_cast<std::size_t>(Step)] * Distance * Distance;
    }

    return std::sqrt(Variance);
}

int FTLevelBelief::GetCutoffRank() const
{
    return static_cast<int>(std::lround(std::exp(GetMean() - GetCutoffOffset())));
}
