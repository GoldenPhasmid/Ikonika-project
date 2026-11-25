#pragma once

#include "core/FTJournal.h"
#include "core/FTSettings.h"
#include "core/FTTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

class FTDatabase;

inline constexpr int MinimumOptimizerReviews = 1000;
inline constexpr int OptimizerRefitReviews = 500;

struct FTReviewAnswer
{
    FTDayNumber Day = 0;
    bool bRemembered = true;
};

using FTReviewHistory = std::vector<FTReviewAnswer>;

struct FTScheduleFit
{
    FTScheduleWeights Weights = DefaultScheduleWeights;
    int ReviewCount = 0;
    double DefaultLoss = 0.0;
    double FittedLoss = 0.0;
};

std::vector<FTReviewHistory> BuildReviewHistories(
    const std::unordered_map<std::string, std::vector<FTJournalEvent>>& EventsByCard);

std::vector<FTReviewHistory> LoadReviewHistories(FTDatabase& Source);

int CountTrainingReviews(const std::vector<FTReviewHistory>& Histories);

bool ShouldOptimizeSchedule(int TrainingReviews, int FittedReviewCount);

double GetScheduleLoss(const FTScheduleWeights& Weights, const std::vector<FTReviewHistory>& Histories);

FTScheduleFit OptimizeScheduleWeights(const std::vector<FTReviewHistory>& Histories);
