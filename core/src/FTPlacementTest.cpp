#include "core/FTPlacementTest.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <numeric>
#include <sstream>
#include <utility>

namespace
{
    constexpr double Steepness = 0.6;

    constexpr double LapseChance = 0.03;

    constexpr double KnownShareAtCutoff = 0.6;

    constexpr int LowestLevelRank = 20;
    constexpr int HighestLevelRank = 60000;
    constexpr int LevelStepCount = 200;

    constexpr int LowestCutoffRank = 50;

    constexpr int StartingRank = 400;
    constexpr double StartingSpread = 2.0;

    constexpr int CefrKnownWordCounts[] = { 1500, 4000, 9000 };

    constexpr int MinimumQuestions = 12;
    constexpr double SettledDeviation = 0.35;

    constexpr int QuestionsBeforeGivingUp = 5;

    constexpr std::size_t NearestWordChoices = 3;

    constexpr int WordColumnCount = 5;

    double GetLevelAt(int Step)
    {
        const double Lowest = std::log(static_cast<double>(LowestLevelRank));
        const double Highest = std::log(static_cast<double>(HighestLevelRank));

        return Lowest + (Highest - Lowest) * Step / (LevelStepCount - 1);
    }

    std::vector<std::string> SplitColumns(const std::string& Line)
    {
        std::vector<std::string> Columns;
        std::string Column;

        std::istringstream Stream(Line);

        while (std::getline(Stream, Column, '\t'))
        {
            Columns.push_back(Column);
        }

        return Columns;
    }
}

std::vector<FTPlacementWord> ParsePlacementWords(const std::string& Text)
{
    std::vector<FTPlacementWord> Words;

    std::istringstream Stream(Text);
    std::string Line;

    bool bHeaderSkipped = false;

    while (std::getline(Stream, Line))
    {
        if (!Line.empty() && Line.back() == '\r')
        {
            Line.pop_back();
        }

        if (!bHeaderSkipped)
        {
            bHeaderSkipped = true;

            continue;
        }

        const std::vector<std::string> Columns = SplitColumns(Line);

        if (static_cast<int>(Columns.size()) < WordColumnCount || Columns[0].empty() || Columns[3].empty())
        {
            continue;
        }

        FTPlacementWord Word;
        Word.Word = Columns[0];
        Word.PartOfSpeech = Columns[1];
        Word.Gender = Columns[2].empty() ? std::nullopt : std::optional(std::atoi(Columns[2].c_str()));
        Word.Translation = Columns[3];
        Word.TeachingRank = std::atoi(Columns[4].c_str());

        if (Word.TeachingRank <= 0)
        {
            continue;
        }

        Words.push_back(std::move(Word));
    }

    return Words;
}

int GetPlacementCefrLevel(int KnownWordCount)
{
    int Level = 0;

    for (const int Count : CefrKnownWordCounts)
    {
        if (KnownWordCount < Count)
        {
            break;
        }

        ++Level;
    }

    return Level;
}

double GetPlacementKnownChance(int TeachingRank, double Level)
{
    const double Distance = (std::log(static_cast<double>(std::max(TeachingRank, 1))) - Level) / Steepness;

    return 1.0 / (1.0 + std::exp(Distance));
}

FTPlacementTest::FTPlacementTest(std::vector<FTPlacementWord> InWords, uint32_t Seed)
    : Words(std::move(InWords))
    , Asked(Words.size(), false)
    , Belief(LevelStepCount, 0.0)
    , Random(Seed)
{
    double Total = 0.0;

    for (int Step = 0; Step < LevelStepCount; ++Step)
    {
        const double Distance = (GetLevelAt(Step) - std::log(static_cast<double>(StartingRank))) / StartingSpread;

        Belief[static_cast<std::size_t>(Step)] = std::exp(-0.5 * Distance * Distance);
        Total += Belief[static_cast<std::size_t>(Step)];
    }

    for (double& Weight : Belief)
    {
        Weight /= Total;
    }

    PickNextWord();
}

const FTPlacementWord* FTPlacementTest::GetCurrentWord() const
{
    if (CurrentIndex < 0)
    {
        return nullptr;
    }

    return &Words[static_cast<std::size_t>(CurrentIndex)];
}

void FTPlacementTest::Answer(bool bKnown)
{
    if (CurrentIndex < 0)
    {
        return;
    }

    const int Rank = Words[static_cast<std::size_t>(CurrentIndex)].TeachingRank;

    double Total = 0.0;

    for (int Step = 0; Step < LevelStepCount; ++Step)
    {
        const double Chance = LapseChance + (1.0 - 2.0 * LapseChance) * GetPlacementKnownChance(Rank, GetLevelAt(Step));

        Belief[static_cast<std::size_t>(Step)] *= bKnown ? Chance : 1.0 - Chance;
        Total += Belief[static_cast<std::size_t>(Step)];
    }

    for (double& Weight : Belief)
    {
        Weight /= Total;
    }

    Answers.push_back({ Rank, bKnown });

    ++AnsweredCount;

    if (bKnown)
    {
        ++KnownCount;
    }

    PickNextWord();
}

bool FTPlacementTest::IsFinished() const
{
    return CurrentIndex < 0;
}

int FTPlacementTest::GetAnsweredCount() const
{
    return AnsweredCount;
}

int FTPlacementTest::GetKnownCount() const
{
    return KnownCount;
}

int FTPlacementTest::GetCutoffRank() const
{
    if (KnownCount == 0 || Words.empty())
    {
        return 0;
    }

    const double CutoffLevel = GetLevelMean() - Steepness * std::log(KnownShareAtCutoff / (1.0 - KnownShareAtCutoff));
    const int Cutoff = static_cast<int>(std::lround(std::exp(CutoffLevel)));

    if (Cutoff < LowestCutoffRank)
    {
        return 0;
    }

    return std::min(Cutoff, GetHighestRank());
}

std::vector<FTPlacementAnswer> FTPlacementTest::GetAnswersByRank() const
{
    std::vector<FTPlacementAnswer> Sorted = Answers;

    std::sort(Sorted.begin(), Sorted.end(), [](const FTPlacementAnswer& Left, const FTPlacementAnswer& Right)
    {
        return Left.TeachingRank < Right.TeachingRank;
    });

    return Sorted;
}

int FTPlacementTest::GetHighestRank() const
{
    const auto Highest = std::max_element(Words.begin(), Words.end(),
        [](const FTPlacementWord& Left, const FTPlacementWord& Right)
        {
            return Left.TeachingRank < Right.TeachingRank;
        });

    return Highest != Words.end() ? Highest->TeachingRank : 0;
}

void FTPlacementTest::PickNextWord()
{
    CurrentIndex = -1;

    if (AnsweredCount >= MaximumQuestions)
    {
        return;
    }

    if (AnsweredCount >= QuestionsBeforeGivingUp && KnownCount == 0)
    {
        return;
    }

    if (AnsweredCount >= MinimumQuestions && GetLevelDeviation() < SettledDeviation)
    {
        return;
    }

    const double Wanted = GetLevelMean();

    std::vector<std::pair<double, int>> Nearest;

    for (std::size_t Index = 0; Index < Words.size(); ++Index)
    {
        if (Asked[Index])
        {
            continue;
        }

        const double Distance = std::abs(std::log(static_cast<double>(Words[Index].TeachingRank)) - Wanted);

        Nearest.emplace_back(Distance, static_cast<int>(Index));
    }

    if (Nearest.empty())
    {
        return;
    }

    const std::size_t ChoiceCount = std::min(NearestWordChoices, Nearest.size());

    std::partial_sort(Nearest.begin(), Nearest.begin() + static_cast<std::ptrdiff_t>(ChoiceCount), Nearest.end());

    CurrentIndex = Nearest[Random() % ChoiceCount].second;
    Asked[static_cast<std::size_t>(CurrentIndex)] = true;
}

double FTPlacementTest::GetLevelMean() const
{
    double Mean = 0.0;

    for (int Step = 0; Step < LevelStepCount; ++Step)
    {
        Mean += Belief[static_cast<std::size_t>(Step)] * GetLevelAt(Step);
    }

    return Mean;
}

double FTPlacementTest::GetLevelDeviation() const
{
    const double Mean = GetLevelMean();

    double Variance = 0.0;

    for (int Step = 0; Step < LevelStepCount; ++Step)
    {
        const double Distance = GetLevelAt(Step) - Mean;

        Variance += Belief[static_cast<std::size_t>(Step)] * Distance * Distance;
    }

    return std::sqrt(Variance);
}
