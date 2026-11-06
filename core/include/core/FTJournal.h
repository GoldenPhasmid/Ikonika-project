#pragma once

#include "core/FTCard.h"
#include "core/FTTypes.h"

#include <optional>
#include <string>
#include <vector>

class FTDatabase;

inline constexpr const char* JournalLearned = "learned";
inline constexpr const char* JournalReviewed = "reviewed";
inline constexpr const char* JournalPooled = "pooled";
inline constexpr const char* JournalCompleted = "completed";
inline constexpr const char* JournalPlaced = "placed";

struct FTJournalEvent
{
    std::string Id;
    std::string EventId;
    std::string CardId;
    int EventValue = 0;
    FTDayNumber Day = 0;
    std::optional<FTDayNumber> DueDate;
};

FTJournalEvent MakeJournalEvent(const std::string& CardId, const char* EventId, int EventValue, FTDayNumber Day);

void ApplyJournalEvent(FTCard& Subject, const FTJournalEvent& Event);

void ReplayJournal(FTCard& Subject, std::vector<FTJournalEvent> Events, FTDayNumber Today);

bool AppendJournalEvent(FTDatabase& Target, const FTJournalEvent& Event);

std::vector<FTJournalEvent> LoadJournal(FTDatabase& Target, const std::string& CardId);

bool RebuildCardProgress(FTDatabase& Target, FTDayNumber Today);

bool SeedStartingEvents(FTDatabase& Target);
