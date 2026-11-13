#pragma once

#include "core/FTLevelBelief.h"

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

constexpr int BeginnerPlacementLevel = 1;
constexpr int CoreCardsCutoffLimit = 500;

bool PlacementWantsCoreCards(int PickedLevel, int CutoffRank);

class FTPlacementTest
{
public:
    static constexpr int MaximumQuestions = 30;

    static constexpr int BeginnerStartingRank = 400;

    static constexpr int IntermediateStartingRank = 2000;

    static constexpr int AdvancedStartingRank = 6000;

    FTPlacementTest() = default;

    FTPlacementTest(std::vector<FTPlacementWord> InWords, uint32_t Seed, int StartingRank);

    const FTPlacementWord* GetCurrentWord() const;

    void Answer(bool bKnown);

    bool IsFinished() const;

    int GetAnsweredCount() const;

    int GetKnownCount() const;

    int GetCutoffRank() const;

    std::vector<FTPlacementAnswer> GetAnswersByRank() const;

private:
    void PickNextWord();

    int GetHighestRank() const;

    std::vector<FTPlacementWord> Words;

    std::vector<bool> Asked;

    std::vector<FTPlacementAnswer> Answers;

    FTLevelBelief Belief;

    std::mt19937 Random;

    int CurrentIndex = -1;

    int AnsweredCount = 0;

    int KnownCount = 0;
};
