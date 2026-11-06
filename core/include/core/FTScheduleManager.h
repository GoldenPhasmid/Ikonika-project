#pragma once

#include "core/FTCardStore.h"
#include "core/FTTypes.h"

#include <string>
#include <vector>

class FTCardRepository;

struct FTRepetitionOutcome
{
    std::string CardId;
    bool bPromote = false;
};

class FTScheduleManager
{
public:
    explicit FTScheduleManager(FTCardRepository& InRepository);

    std::vector<FTCardHandle> LoadDueCardsForToday(FTDayNumber Today);

    bool ReportReviewAnswer(FTCard Subject, bool bRemembered, FTDayNumber Today);
    
    int ReportSessionOutcomes(const std::vector<FTRepetitionOutcome>& Outcomes, FTDayNumber Today);

private:
    FTCardRepository& Repository;
};
