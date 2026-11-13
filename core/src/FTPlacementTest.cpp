#include "core/FTPlacementTest.h"

#include "core/FTTsv.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <numeric>
#include <utility>

namespace
{
    constexpr double LapseChance = 0.03;

    constexpr int LowestCutoffRank = 50;

    constexpr int CefrKnownWordCounts[] = { 1500, 4000, 9000 };

    constexpr int MinimumQuestions = 12;
    constexpr double SettledDeviation = 0.35;

    constexpr int QuestionsBeforeGivingUp = 5;

    constexpr std::size_t NearestWordChoices = 3;

    constexpr int WordColumnCount = 5;
}

std::vector<FTPlacementWord> ParsePlacementWords(const std::string& Text)
{
    std::vector<FTPlacementWord> Words;

    for (const std::vector<std::string>& Columns : ParseTsvRows(Text))
    {
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

bool PlacementWantsCoreCards(int PickedLevel, int CutoffRank)
{
    return PickedLevel == BeginnerPlacementLevel && CutoffRank < CoreCardsCutoffLimit;
}

FTPlacementTest::FTPlacementTest(std::vector<FTPlacementWord> InWords, uint32_t Seed, int StartingRank)
    : Words(std::move(InWords))
    , Asked(Words.size(), false)
    , Belief(FTLevelBelief::FromStartingRank(StartingRank))
    , Random(Seed)
{
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

    Belief.Update(Rank, bKnown ? 1.0 - LapseChance : LapseChance, bKnown ? LapseChance : 1.0 - LapseChance);

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

    const int Cutoff = Belief.GetCutoffRank();

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

    if (AnsweredCount >= MinimumQuestions && Belief.GetDeviation() < SettledDeviation)
    {
        return;
    }

    const double Wanted = Belief.GetMean();

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
