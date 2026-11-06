#pragma once

#include "core/FTCard.h"
#include "core/FTSettings.h"

#include <QString>

inline constexpr int MaximumLadderStages = 10;
inline constexpr int MaximumIntervalDays = 1000;

int GetLastStage();
int GetEffectiveStage(const FTCard& Subject);
int GetIntervalDays(int Stage);

bool IsDue(const FTCard& Subject, FTDayNumber Today);
bool IsExpired(const FTCard& Subject, FTDayNumber Today);

/**
 * Records one card's move: which card, why it moved, and what it moved from.
 *
 * Called by whatever decided the move rather than by the ladder functions themselves, since only
 * the caller knows whether the card was answered or punished for missing its date. The before
 * state is what makes the line reversible.
 */
void LogCardMove(const FTCard& Before, const FTCard& After, const QString& Reason);

void PlaceInStage(FTCard& Subject, int Stage, FTDayNumber Today);
void Promote(FTCard& Subject, FTDayNumber Today);
void Demote(FTCard& Subject, FTDayNumber Today);
void ReturnToPool(FTCard& Subject, FTDayNumber Today);
void MarkCompleted(FTCard& Subject, FTDayNumber Today);
bool ApplyExpiry(FTCard& Subject, FTDayNumber Today);
bool ApplyExpiryThrough(FTCard& Subject, FTDayNumber Today);
