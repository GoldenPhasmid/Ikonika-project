#pragma once

#include "core/FTCard.h"
#include "core/FTSettings.h"

#include <QString>

#include <vector>

inline constexpr int MaximumIntervalDays = 1000;
inline constexpr int ProgressStageCount = 4;
inline constexpr int SchedulePreviewForgottenReview = 2;

struct FTMemoryState
{
    double Stability = 0.0;
    double Difficulty = 0.0;
};

struct FTSchedulePreview
{
    std::vector<int> ReviewDays;
    std::vector<bool> QuickReviews;
    int CompletedDay = 0;
};

int GetLastStage();

FTMemoryState GetFirstMemoryState(const FTScheduleWeights& Weights, bool bRemembered);
FTMemoryState GetNextMemoryState(const FTScheduleWeights& Weights, const FTMemoryState& State, bool bRemembered,
                                 int ElapsedDays);

double GetRetrievability(double Stability, int ElapsedDays);
int GetNextIntervalDays(double Stability);
int GetProgressStage(double Stability);

bool IsDue(const FTCard& Subject, FTDayNumber Today);
bool IsQuickReviewCard(const FTCard& Subject);

void LogCardMove(const FTCard& Before, const FTCard& After, const QString& Reason);

void LearnCard(FTCard& Subject, FTDayNumber Today);
void AnswerReview(FTCard& Subject, bool bRemembered, FTDayNumber Today);
void PlaceWithStability(FTCard& Subject, double Stability, FTDayNumber Today);
void ReturnToPool(FTCard& Subject, FTDayNumber Today);
void MarkCompleted(FTCard& Subject, FTDayNumber Today);
void ScheduleFromStability(FTCard& Subject, FTDayNumber Today);

FTSchedulePreview PreviewSchedule(int ForgottenReview);
