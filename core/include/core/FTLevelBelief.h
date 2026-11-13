#pragma once

#include <vector>

double GetPlacementKnownChance(int TeachingRank, double Level);

class FTLevelBelief
{
public:
    static constexpr double Steepness = 0.6;

    static constexpr double KnownShareAtCutoff = 0.6;

    FTLevelBelief();

    static FTLevelBelief FromCutoffRank(int CutoffRank);

    static FTLevelBelief FromStartingRank(int StartingRank);

    void Update(int TeachingRank, double ChanceIfKnown, double ChanceIfUnknown);

    double GetKnownChance(int TeachingRank) const;

    double GetMean() const;

    double GetDeviation() const;

    int GetCutoffRank() const;

private:
    FTLevelBelief(double Centre, double Spread);

    std::vector<double> Weights;
};
