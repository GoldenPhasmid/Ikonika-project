#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

struct FTPlacementWord
{
    std::string Word;

    std::string PartOfSpeech;

    std::optional<int> Gender;

    std::string Translation;

    int TeachingRank = 0;
};

struct FTPlacementAnswer
{
    int TeachingRank = 0;

    bool bKnown = false;
};

std::vector<FTPlacementWord> ParsePlacementWords(const std::string& Text);

int GetPlacementCefrLevel(int KnownWordCount);

double GetPlacementKnownChance(int TeachingRank, double Level);

class FTPlacementTest
{
public:
    static constexpr int MaximumQuestions = 30;

    FTPlacementTest() = default;

    FTPlacementTest(std::vector<FTPlacementWord> InWords, uint32_t Seed);

    const FTPlacementWord* GetCurrentWord() const;

    void Answer(bool bKnown);

    bool IsFinished() const;

    int GetAnsweredCount() const;

    int GetKnownCount() const;

    int GetCutoffRank() const;

    std::vector<FTPlacementAnswer> GetAnswersByRank() const;

private:
    void PickNextWord();

    double GetLevelMean() const;

    double GetLevelDeviation() const;

    int GetHighestRank() const;

    std::vector<FTPlacementWord> Words;

    std::vector<bool> Asked;

    std::vector<FTPlacementAnswer> Answers;

    std::vector<double> Belief;

    std::mt19937 Random;

    int CurrentIndex = -1;

    int AnsweredCount = 0;

    int KnownCount = 0;
};
