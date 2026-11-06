#pragma once

#include "core/FTAnswerComparison.h"

#include <chrono>

struct FTScoringRules
{
    int MaximumCardScore = 30;

    int MinimumCorrectScore = 0;

    int IncorrectScore = -30;
    int SkippedScore = 0;
};

int CalculateCardScore(EFTAnswerGrade Grade,
                       std::chrono::milliseconds Elapsed,
                       std::chrono::milliseconds Buffer,
                       const FTScoringRules& Rules = {});
