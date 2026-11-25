#include "core/FTJournal.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"
#include "core/FTSchedule.h"
#include "core/FTSchema.h"
#include "core/FTUuid.h"

#include <algorithm>
#include <array>
#include <unordered_map>

namespace
{
    constexpr std::array<int, 6> LegacyIntervalDays{1, 2, 4, 14, 40, 120};

    constexpr const char* JournalColumns = "Id, EventId, CardId, EventValue, Day, DueDate, Stability, Difficulty";

    int GetLegacyIntervalDays(int Stage)
    {
        const int Clamped = std::clamp(Stage, FirstLadderStage, static_cast<int>(LegacyIntervalDays.size()));

        return LegacyIntervalDays[static_cast<size_t>(Clamped - 1)];
    }

    void ApplyEvent(FTCard& Subject, const FTJournalEvent& Event)
    {
        if (Event.EventId == JournalLearned)
        {
            LearnCard(Subject, Event.Day);
        }
        else if (Event.EventId == JournalReviewed)
        {
            AnswerReview(Subject, Event.EventValue != 0, Event.Day);
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
            PlaceWithStability(Subject, GetLegacyIntervalDays(Event.EventValue), Event.Day);
        }
    }

    bool IsAnswerEvent(const FTJournalEvent& Event)
    {
        return Event.EventId == JournalLearned || Event.EventId == JournalReviewed || Event.EventId == JournalPlaced;
    }

    void AdoptRecordedSchedule(FTCard& Subject, const FTJournalEvent& Event)
    {
        Subject.Stability = Event.Stability;
        Subject.Difficulty = Event.Difficulty;

        if (!IsAnswerEvent(Event))
        {
            return;
        }

        if (!Event.DueDate.has_value())
        {
            Subject.LeitnerStage = ProgressStageCount;
            Subject.CompletedDate = Event.Day;
            Subject.DueDate.reset();

            return;
        }

        Subject.LeitnerStage = GetProgressStage(*Subject.Stability);
        Subject.CompletedDate.reset();
        Subject.DueDate = Event.DueDate;
    }

    std::optional<double> ReadOptionalReal(const FTStatement& Query, int Column)
    {
        if (Query.IsNull(Column))
        {
            return std::nullopt;
        }

        return Query.GetDouble(Column);
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

        Event.Stability = ReadOptionalReal(Query, 6);
        Event.Difficulty = ReadOptionalReal(Query, 7);

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

    void BindOptionalReal(FTStatement& Statement, int Index, const std::optional<double>& Value)
    {
        if (Value.has_value())
        {
            Statement.BindDouble(Index, *Value);

            return;
        }

        Statement.BindNull(Index);
    }

    FTStatement PrepareProgressWrite(FTDatabase& Target)
    {
        return Target.Prepare(
            "UPDATE Cards SET LeitnerStage = ?2, DueDate = ?3, BoxChangedDate = ?4, CompletedDate = ?5, "
            "Stability = ?6, Difficulty = ?7 WHERE Id = ?1");
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
        BindOptionalReal(Update, 6, Progress.Stability);
        BindOptionalReal(Update, 7, Progress.Difficulty);

        return Update.Run();
    }
}

void SortJournalEvents(std::vector<FTJournalEvent>& Events)
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
}

bool LoadJournalByCard(FTDatabase& Target,
                       std::unordered_map<std::string, std::vector<FTJournalEvent>>& EventsByCard)
{
    FTStatement Query = Target.Prepare(std::string("SELECT ") + JournalColumns + " FROM Journal");

    if (!Query.IsValid())
    {
        return false;
    }

    while (Query.Step())
    {
        FTJournalEvent Event = ReadEvent(Query);
        EventsByCard[Event.CardId].push_back(std::move(Event));
    }

    return !Query.HasFailed();
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
    ApplyEvent(Subject, Event);

    if (IsInPool(Subject))
    {
        return;
    }

    if (Event.Stability.has_value() && Event.Difficulty.has_value())
    {
        AdoptRecordedSchedule(Subject, Event);

        return;
    }

    if (Event.DueDate.has_value() && Subject.DueDate.has_value())
    {
        Subject.DueDate = Event.DueDate;
    }
}

void ReplayJournal(FTCard& Subject, std::vector<FTJournalEvent> Events)
{
    SortJournalEvents(Events);

    Subject.LeitnerStage = PoolStage;
    Subject.DueDate.reset();
    Subject.BoxChangedDate.reset();
    Subject.CompletedDate.reset();
    Subject.Stability.reset();
    Subject.Difficulty.reset();

    for (const FTJournalEvent& Event : Events)
    {
        ApplyJournalEvent(Subject, Event);
    }
}

bool AppendJournalEvent(FTDatabase& Target, const FTJournalEvent& Event)
{
    FTStatement Insert = Target.Prepare(std::string("INSERT INTO Journal (") + JournalColumns
                                        + ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");

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
    BindOptionalReal(Insert, 7, Event.Stability);
    BindOptionalReal(Insert, 8, Event.Difficulty);

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
    FTStatement Query = Target.Prepare(std::string("SELECT ") + JournalColumns
                                       + " FROM Journal WHERE CardId = ?1 ORDER BY Day, Id");

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

bool RebuildCardProgress(FTDatabase& Target)
{
    const FTQueryTimer Timer("RebuildCardProgress");

    std::unordered_map<std::string, std::vector<FTJournalEvent>> EventsByCard;

    if (!LoadJournalByCard(Target, EventsByCard))
    {
        return false;
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

        ReplayJournal(Progress, Found != EventsByCard.end() ? Found->second : std::vector<FTJournalEvent>{});

        if (!WriteProgress(Update, CardId, Progress))
        {
            return false;
        }
    }

    qCInfo(LogSchedule) << "Rebuilt the progress of" << static_cast<int>(CardIds.size()) << "cards from"
                        << static_cast<int>(EventsByCard.size()) << "journals";

    return true;
}

bool FillJournalStability(FTDatabase& Target)
{
    const FTQueryTimer Timer("FillJournalStability");

    std::unordered_map<std::string, std::vector<FTJournalEvent>> EventsByCard;

    if (!LoadJournalByCard(Target, EventsByCard))
    {
        return false;
    }

    FTStatement Update = Target.Prepare(
        "UPDATE Journal SET Stability = ?2, Difficulty = ?3, DueDate = COALESCE(DueDate, ?4) WHERE Id = ?1");
    int Filled = 0;

    for (auto& [CardId, Events] : EventsByCard)
    {
        SortJournalEvents(Events);

        FTCard Progress;

        for (const FTJournalEvent& Event : Events)
        {
            ApplyJournalEvent(Progress, Event);

            if (Event.Stability.has_value() || !Progress.Stability.has_value() || !Progress.Difficulty.has_value())
            {
                continue;
            }

            if (!Update.IsValid())
            {
                return false;
            }

            Update.Reset();
            Update.BindText(1, Event.Id);
            Update.BindDouble(2, *Progress.Stability);
            Update.BindDouble(3, *Progress.Difficulty);
            BindOptionalDay(Update, 4, Progress.DueDate);

            if (!Update.Run())
            {
                return false;
            }

            ++Filled;
        }
    }

    qCInfo(LogSchedule) << "Gave" << Filled << "journal events the stability they left their card with";

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
                    Event.Day = Query.GetInt(2) - GetLegacyIntervalDays(Stage);
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
