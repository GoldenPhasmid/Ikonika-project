#include "core/FTJournal.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"
#include "core/FTSchedule.h"
#include "core/FTSchema.h"
#include "core/FTUuid.h"

#include <algorithm>
#include <unordered_map>

namespace
{
    void ApplyEvent(FTCard& Subject, const FTJournalEvent& Event)
    {
        if (Event.EventId == JournalLearned)
        {
            Promote(Subject, Event.Day);
        }
        else if (Event.EventId == JournalReviewed)
        {
            if (Event.EventValue != 0)
            {
                Promote(Subject, Event.Day);
            }
            else
            {
                Demote(Subject, Event.Day);
            }
        }
        else if (Event.EventId == JournalPooled)
        {
            ReturnToPool(Subject, Event.Day);
        }
        else if (Event.EventId == JournalCompleted)
        {
            MarkCompleted(Subject, Event.Day);
        }
        else if (Event.EventId == JournalPlaced && Event.EventValue >= FirstLadderStage)
        {
            PlaceInStage(Subject, Event.EventValue, Event.Day);
        }
    }

    FTJournalEvent ReadEvent(const FTStatement& Query)
    {
        FTJournalEvent Event;
        Event.Id = Query.GetText(0);
        Event.EventId = Query.GetText(1);
        Event.CardId = Query.GetText(2);
        Event.EventValue = Query.GetInt(3);
        Event.Day = Query.GetInt(4);

        if (!Query.IsNull(5))
        {
            Event.DueDate = Query.GetInt(5);
        }

        return Event;
    }

    void BindOptionalDay(FTStatement& Statement, int Index, const std::optional<FTDayNumber>& Day)
    {
        if (Day.has_value())
        {
            Statement.BindInt(Index, *Day);

            return;
        }

        Statement.BindNull(Index);
    }

    FTStatement PrepareProgressWrite(FTDatabase& Target)
    {
        return Target.Prepare(
            "UPDATE Cards SET LeitnerStage = ?2, DueDate = ?3, BoxChangedDate = ?4, CompletedDate = ?5 "
            "WHERE Id = ?1");
    }

    bool WriteProgress(FTStatement& Update, const std::string& CardId, const FTCard& Progress)
    {
        if (!Update.IsValid())
        {
            return false;
        }

        Update.Reset();
        Update.BindText(1, CardId);
        Update.BindInt(2, Progress.LeitnerStage);
        BindOptionalDay(Update, 3, Progress.DueDate);
        BindOptionalDay(Update, 4, Progress.BoxChangedDate);
        BindOptionalDay(Update, 5, Progress.CompletedDate);

        return Update.Run();
    }
}

FTJournalEvent MakeJournalEvent(const std::string& CardId, const char* EventId, int EventValue, FTDayNumber Day)
{
    FTJournalEvent Event;
    Event.Id = MakeTimeOrderedUuid();
    Event.EventId = EventId;
    Event.CardId = CardId;
    Event.EventValue = EventValue;
    Event.Day = Day;

    return Event;
}

void ApplyJournalEvent(FTCard& Subject, const FTJournalEvent& Event)
{
    ApplyExpiryThrough(Subject, Event.Day);
    ApplyEvent(Subject, Event);

    if (Event.DueDate.has_value() && Subject.DueDate.has_value())
    {
        Subject.DueDate = Event.DueDate;
    }
}

void ReplayJournal(FTCard& Subject, std::vector<FTJournalEvent> Events, FTDayNumber Today)
{
    std::sort(Events.begin(), Events.end(),
              [](const FTJournalEvent& Left, const FTJournalEvent& Right)
              {
                  if (Left.Day != Right.Day)
                  {
                      return Left.Day < Right.Day;
                  }

                  return Left.Id < Right.Id;
              });

    Subject.LeitnerStage = PoolStage;
    Subject.DueDate.reset();
    Subject.BoxChangedDate.reset();
    Subject.CompletedDate.reset();

    for (const FTJournalEvent& Event : Events)
    {
        ApplyJournalEvent(Subject, Event);
    }

    ApplyExpiryThrough(Subject, Today);
}

bool AppendJournalEvent(FTDatabase& Target, const FTJournalEvent& Event)
{
    FTStatement Insert = Target.Prepare(
        "INSERT INTO Journal (Id, EventId, CardId, EventValue, Day, DueDate) VALUES (?1, ?2, ?3, ?4, ?5, ?6)");

    if (!Insert.IsValid())
    {
        return false;
    }

    Insert.BindText(1, Event.Id);
    Insert.BindText(2, Event.EventId);
    Insert.BindText(3, Event.CardId);
    Insert.BindInt(4, Event.EventValue);
    Insert.BindInt(5, Event.Day);
    BindOptionalDay(Insert, 6, Event.DueDate);

    if (!Insert.Run())
    {
        qCWarning(LogSchedule) << "Could not record" << QString::fromStdString(Event.EventId) << "for card"
                               << QString::fromStdString(Event.CardId);

        return false;
    }

    return true;
}

std::vector<FTJournalEvent> LoadJournal(FTDatabase& Target, const std::string& CardId)
{
    FTStatement Query = Target.Prepare(
        "SELECT Id, EventId, CardId, EventValue, Day, DueDate FROM Journal WHERE CardId = ?1 ORDER BY Day, Id");

    if (!Query.IsValid())
    {
        return {};
    }

    Query.BindText(1, CardId);

    std::vector<FTJournalEvent> Events;

    while (Query.Step())
    {
        Events.push_back(ReadEvent(Query));
    }

    return Events;
}

bool RebuildCardProgress(FTDatabase& Target, FTDayNumber Today)
{
    const FTQueryTimer Timer("RebuildCardProgress");

    std::unordered_map<std::string, std::vector<FTJournalEvent>> EventsByCard;

    {
        FTStatement Query = Target.Prepare("SELECT Id, EventId, CardId, EventValue, Day, DueDate FROM Journal");

        if (!Query.IsValid())
        {
            return false;
        }

        while (Query.Step())
        {
            FTJournalEvent Event = ReadEvent(Query);
            EventsByCard[Event.CardId].push_back(std::move(Event));
        }

        if (Query.HasFailed())
        {
            return false;
        }
    }

    std::vector<std::string> CardIds;

    {
        FTStatement Query = Target.Prepare("SELECT Id FROM Cards");

        if (!Query.IsValid())
        {
            return false;
        }

        while (Query.Step())
        {
            CardIds.push_back(Query.GetText(0));
        }

        if (Query.HasFailed())
        {
            return false;
        }
    }

    FTStatement Update = PrepareProgressWrite(Target);

    for (const std::string& CardId : CardIds)
    {
        FTCard Progress;
        const auto Found = EventsByCard.find(CardId);

        ReplayJournal(Progress, Found != EventsByCard.end() ? Found->second : std::vector<FTJournalEvent>{},
                      Today);

        if (!WriteProgress(Update, CardId, Progress))
        {
            return false;
        }
    }

    qCInfo(LogSchedule) << "Rebuilt the progress of" << static_cast<int>(CardIds.size()) << "cards from"
                        << static_cast<int>(EventsByCard.size()) << "journals";

    return true;
}

bool SeedStartingEvents(FTDatabase& Target)
{
    const FTQueryTimer Timer("SeedStartingEvents");

    std::vector<FTJournalEvent> Events;

    {
        FTStatement Query = Target.Prepare(
            "SELECT Id, LeitnerStage, DueDate, BoxChangedDate, CompletedDate, CreatedDate FROM Cards "
            "WHERE (LeitnerStage > 0 OR CompletedDate IS NOT NULL) "
            "  AND Id NOT IN (SELECT CardId FROM Journal)");

        if (!Query.IsValid())
        {
            return false;
        }

        while (Query.Step())
        {
            FTJournalEvent Event;
            Event.CardId = Query.GetText(0);

            if (!Query.IsNull(4))
            {
                Event.EventId = JournalCompleted;
                Event.Day = Query.GetInt(4);
                Event.Id = "start-" + Event.CardId + "-completed-" + std::to_string(Event.Day);
            }
            else
            {
                const int Stage = Query.GetInt(1);

                if (!Query.IsNull(3))
                {
                    Event.Day = Query.GetInt(3);
                }
                else if (!Query.IsNull(2))
                {
                    Event.Day = Query.GetInt(2) - GetIntervalDays(Stage);
                }
                else
                {
                    Event.Day = DayNumberForMilliseconds(Query.GetInt64(5));
                }

                Event.EventId = JournalPlaced;
                Event.EventValue = Stage;
                Event.Id = "start-" + Event.CardId + "-" + std::to_string(Stage) + "-" + std::to_string(Event.Day);

                if (!Query.IsNull(2))
                {
                    Event.DueDate = Query.GetInt(2);
                    Event.Id += "-" + std::to_string(*Event.DueDate);
                }
            }

            Events.push_back(std::move(Event));
        }

        if (Query.HasFailed())
        {
            return false;
        }
    }

    FTStatement Insert = Target.Prepare(
        "INSERT OR IGNORE INTO Journal (Id, EventId, CardId, EventValue, Day, DueDate) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6)");

    for (const FTJournalEvent& Event : Events)
    {
        if (!Insert.IsValid())
        {
            return false;
        }

        Insert.Reset();
        Insert.BindText(1, Event.Id);
        Insert.BindText(2, Event.EventId);
        Insert.BindText(3, Event.CardId);
        Insert.BindInt(4, Event.EventValue);
        Insert.BindInt(5, Event.Day);
        BindOptionalDay(Insert, 6, Event.DueDate);

        if (!Insert.Run())
        {
            return false;
        }
    }

    if (!Events.empty())
    {
        qCInfo(LogSchedule) << "Gave" << static_cast<int>(Events.size())
                            << "cards a starting event recording their progress";
    }

    return true;
}
