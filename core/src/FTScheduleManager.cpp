#include "core/FTScheduleManager.h"

#include "core/FTCardRepository.h"
#include "core/FTJournal.h"
#include "core/FTLog.h"
#include "core/FTSchedule.h"

#include <QString>

#include <algorithm>

FTScheduleManager::FTScheduleManager(FTCardRepository& InRepository)
    : Repository(InRepository)
{
}

namespace
{
    /** Shared by both loaders: nothing due should reach a caller still carrying a date it should
        already have been punished for missing. */
    void ApplyExpiryToAll(FTCardRepository& Repository, std::vector<FTCardHandle>& Cards,
                          FTDayNumber Today)
    {
        int Punished = 0;
        int Pooled = 0;

        // The queries only read the due date already on the card; a card overdue past the expiry
        // cutoff is still owed a real demotion first, or it would be reviewed as if nothing had
        // happened since it fell due.
        for (const FTCardHandle& Held : Cards)
        {
            FTCard Subject = *Held;

            if (!ApplyExpiryThrough(Subject, Today))
            {
                continue;
            }

            ++Punished;

            if (IsInPool(Subject))
            {
                ++Pooled;
            }

            Repository.UpdateCardSchedule(Subject);
        }

        // Said before the count of what loaded, because this is what changed the cards rather
        // than anything the user did. Every card it touched has its own line at debug.
        if (Punished > 0)
        {
            qCInfo(LogSchedule) << "Loading punished" << Punished << "of"
                                << static_cast<int>(Cards.size())
                                << "cards for missing their date," << Pooled
                                << "of them back to the pool";
        }

        // A card the expiry rule sent all the way back to the pool is not due any more: it belongs
        // to Intensive Learning's pool pass now, not today's review.
        Cards.erase(std::remove_if(Cards.begin(), Cards.end(),
            [](const FTCardHandle& Held) { return IsInPool(*Held); }), Cards.end());
    }
}

std::vector<FTCardHandle> FTScheduleManager::LoadDueCardsForToday(FTDayNumber Today)
{
    std::vector<FTCardHandle> Cards = Repository.LoadDueCards(Today);

    ApplyExpiryToAll(Repository, Cards, Today);

    qCInfo(LogSchedule) << "Loaded" << static_cast<int>(Cards.size()) << "cards due for"
                        << Today;

    return Cards;
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
