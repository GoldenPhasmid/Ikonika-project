#include "core/FTScheduleManager.h"

#include "core/FTCardRepository.h"
#include "core/FTJournal.h"
#include "core/FTLog.h"
#include "core/FTSchedule.h"

#include <QString>

FTScheduleManager::FTScheduleManager(FTCardRepository& InRepository)
    : Repository(InRepository)
{
}

std::vector<FTCardHandle> FTScheduleManager::LoadDueCardsForToday(FTDayNumber Today)
{
    std::vector<FTCardHandle> Cards = Repository.LoadDueCards(Today);

    qCInfo(LogSchedule) << "Loaded" << static_cast<int>(Cards.size()) << "cards due for"
                        << Today;

    return Cards;
}

int FTScheduleManager::CountDueCardsForToday(FTDayNumber Today)
{
    return static_cast<int>(Repository.LoadDueCards(Today).size());
}

int FTScheduleManager::ReportSessionOutcomes(const std::vector<FTRepetitionOutcome>& Outcomes,
                                             FTDayNumber Today)
{
    int Moved = 0;

    for (const FTRepetitionOutcome& Outcome : Outcomes)
    {
        const std::optional<FTCardHandle> Subject = Repository.LoadCard(Outcome.CardId);

        if (!Subject.has_value())
        {
            continue;
        }

        if (ReportReviewAnswer(**Subject, Outcome.bPromote, Today))
        {
            ++Moved;
        }
    }

    qCInfo(LogSchedule) << "Repetition outcomes applied:" << Moved << "of"
                        << static_cast<int>(Outcomes.size()) << "cards moved";

    return Moved;
}

bool FTScheduleManager::ReportReviewAnswer(FTCard Subject, bool bRemembered, FTDayNumber Today)
{
    return Repository.RecordProgressEvent(Subject.Id, JournalReviewed, bRemembered ? 1 : 0, Today);
}
