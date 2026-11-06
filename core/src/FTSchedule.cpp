#include "core/FTSchedule.h"

#include "core/FTLog.h"

#include <QString>

#include <algorithm>

void PlaceInStage(FTCard& Subject, int Stage, FTDayNumber Today)
{
    if (Subject.LeitnerStage != Stage)
    {
        Subject.BoxChangedDate = Today;
    }

    Subject.LeitnerStage = Stage;
    Subject.CompletedDate.reset();
    Subject.DueDate = Today + GetIntervalDays(Stage);
}

namespace
{
    QString DescribeDay(const std::optional<FTDayNumber>& Day)
    {
        return Day.has_value() ? QString::number(*Day) : QStringLiteral("none");
    }

    QString DescribeStage(const FTCard& Subject)
    {
        if (IsInPool(Subject))
        {
            return QStringLiteral("pool");
        }

        if (IsCompleted(Subject))
        {
            return QStringLiteral("completed");
        }

        return QString::number(Subject.LeitnerStage);
    }
}

void LogCardMove(const FTCard& Before, const FTCard& After, const QString& Reason)
{
    qCDebug(LogSchedule).nospace().noquote()
        << "card " << QString::fromStdString(After.Id) << " \""
        << QString::fromStdString(After.TargetWord) << "\" " << Reason << ": stage "
        << DescribeStage(Before) << " -> " << DescribeStage(After) << ", due "
        << DescribeDay(Before.DueDate) << " -> " << DescribeDay(After.DueDate) << ", box changed "
        << DescribeDay(Before.BoxChangedDate) << " -> " << DescribeDay(After.BoxChangedDate);
}

int GetLastStage()
{
    return static_cast<int>(FTSettings::Get().GetScheduleRules().IntervalDays.size());
}

int GetEffectiveStage(const FTCard& Subject)
{
    if (Subject.LeitnerStage <= PoolStage)
    {
        return PoolStage;
    }

    return std::min(Subject.LeitnerStage, GetLastStage());
}

int GetIntervalDays(int Stage)
{
    const FTScheduleRules& Rules = FTSettings::Get().GetScheduleRules();

    if (Rules.IntervalDays.empty())
    {
        return 0;
    }

    const int Clamped = std::clamp(Stage, FirstLadderStage, GetLastStage());

    return Rules.IntervalDays[static_cast<size_t>(Clamped - 1)];
}

bool IsDue(const FTCard& Subject, FTDayNumber Today)
{
    if (IsCompleted(Subject) || !Subject.DueDate.has_value())
    {
        return false;
    }

    return *Subject.DueDate <= Today;
}

bool IsExpired(const FTCard& Subject, FTDayNumber Today)
{
    if (IsCompleted(Subject) || !Subject.DueDate.has_value())
    {
        return false;
    }

    return *Subject.DueDate < Today;
}

void Promote(FTCard& Subject, FTDayNumber Today)
{
    if (IsCompleted(Subject) || GetLastStage() == 0)
    {
        return;
    }

    const int Next = GetEffectiveStage(Subject) + 1;

    if (Next > GetLastStage())
    {
        Subject.CompletedDate = Today;
        Subject.BoxChangedDate = Today;
        Subject.DueDate.reset();

        return;
    }

    PlaceInStage(Subject, Next, Today);
}

void Demote(FTCard& Subject, FTDayNumber Today)
{
    if (GetLastStage() == 0)
    {
        return;
    }

    if (IsCompleted(Subject))
    {
        // PlaceInStage already schedules the new stage's own interval from today; nothing here
        // should override that with an immediate same-day redo.
        PlaceInStage(Subject, GetLastStage(), Today);

        return;
    }

    if (IsInPool(Subject))
    {
        return;
    }

    const int Current = GetEffectiveStage(Subject);
    const FTScheduleRules& Rules = FTSettings::Get().GetScheduleRules();

    if (Current <= FirstLadderStage)
    {
        if (Rules.bFirstStageFailureReturnsToPool)
        {
            ReturnToPool(Subject, Today);

            return;
        }

        PlaceInStage(Subject, FirstLadderStage, Today);

        return;
    }

    PlaceInStage(Subject, Current - 1, Today);
}

void ReturnToPool(FTCard& Subject, FTDayNumber Today)
{
    Subject.LeitnerStage = PoolStage;
    Subject.BoxChangedDate = Today;
    Subject.CompletedDate.reset();
    Subject.DueDate.reset();
}

void MarkCompleted(FTCard& Subject, FTDayNumber Today)
{
    Subject.LeitnerStage = GetLastStage();
    Subject.CompletedDate = Today;
    Subject.BoxChangedDate = Today;
    Subject.DueDate.reset();
}

bool ApplyExpiry(FTCard& Subject, FTDayNumber Today)
{
    if (!IsExpired(Subject, Today))
    {
        return false;
    }

    if (Subject.LeitnerStage == FirstLadderStage && Subject.BoxChangedDate.has_value())
    {
        const int DaysInStage = Today - *Subject.BoxChangedDate;
        const int MaximumDays = FTSettings::Get().GetScheduleRules().MaximumDaysInFirstStage;

        if (DaysInStage > MaximumDays)
        {
            const FTCard Before = Subject;

            ReturnToPool(Subject, Today);

            LogCardMove(Before, Subject,
                        QStringLiteral("stale after %1 days in stage 1").arg(DaysInStage));

            return true;
        }
    }

    if (!FTSettings::Get().GetScheduleRules().bMissedReviewDemotes)
    {
        return false;
    }

    // Punished once, and it stays due until it is actually reviewed. Without this a card waiting
    // in the queue loses a box on every load, so opening the app on five days nobody reviewed
    // costs five boxes for one absence. The box having moved on or after the date it was owed is
    // what says the last thing to happen to this card was a punishment rather than an answer, so
    // the date stays where the punishment left it: moving it forward would erase that.
    if (Subject.BoxChangedDate.has_value() && *Subject.BoxChangedDate >= *Subject.DueDate)
    {
        return false;
    }

    const FTCard Before = Subject;

    Demote(Subject, Today);

    if (!IsInPool(Subject))
    {
        Subject.DueDate = Today;
    }

    LogCardMove(Before, Subject, QStringLiteral("missed its date"));

    return true;
}

bool ApplyExpiryThrough(FTCard& Subject, FTDayNumber Today)
{
    const FTScheduleRules& Rules = FTSettings::Get().GetScheduleRules();

    bool bChanged = false;

    while (IsExpired(Subject, Today))
    {
        const FTDayNumber FirstMissed = *Subject.DueDate + 1;
        const bool bAlreadyPunished = Subject.BoxChangedDate.has_value()
            && *Subject.BoxChangedDate >= *Subject.DueDate;

        FTDayNumber Day = FirstMissed;

        if (bAlreadyPunished || !Rules.bMissedReviewDemotes)
        {
            Day = Today;

            if (Subject.LeitnerStage == FirstLadderStage && Subject.BoxChangedDate.has_value())
            {
                const FTDayNumber Stale = *Subject.BoxChangedDate + Rules.MaximumDaysInFirstStage + 1;

                Day = std::min(Today, std::max(FirstMissed, Stale));
            }
        }

        if (ApplyExpiry(Subject, Day))
        {
            bChanged = true;
        }

        if (Day >= Today)
        {
            break;
        }
    }

    return bChanged;
}

