#include "DailySessionController.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"

#include "core/FCardSelectorBase.h"
#include "core/FTWordForms.h"
#include "core/FTCardStore.h"
#include "core/FTLog.h"
#include "core/FTRepetitionBatch.h"
#include "core/FTSchedule.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"

#include <QVariantMap>
#include <QTimer>

#include <algorithm>
#include <random>
#include <unordered_map>

namespace
{
    constexpr int ReviewTimelineDays = 5;

    constexpr int RecentProgressDays = 7;
}

namespace
{
    QVariantMap MakeLadderEntry(const QString& Kind, int Stage, int Count)
    {
        QVariantMap Entry;
        Entry[QStringLiteral("kind")] = Kind;
        Entry[QStringLiteral("stage")] = Stage;
        Entry[QStringLiteral("count")] = Count;

        return Entry;
    }

    QVariantList BuildLadder(const std::vector<FTStageCount>& Counts)
    {
        const int LastStage = std::max(GetLastStage(), 1);

        int Pool = 0;
        int Learned = 0;
        std::vector<int> Boxes(static_cast<size_t>(LastStage), 0);

        for (const FTStageCount& Entry : Counts)
        {
            if (Entry.bCompleted)
            {
                Learned += Entry.Count;
            }
            else if (Entry.Stage <= 0)
            {
                Pool += Entry.Count;
            }
            else
            {
                // A box above the last one only exists after the box count was lowered in settings.
                // Those words are still owed reviews, so they count toward the last box.
                Boxes[static_cast<size_t>(std::min(Entry.Stage, LastStage) - 1)] += Entry.Count;
            }
        }

        QVariantList Result;
        Result.append(MakeLadderEntry(QStringLiteral("pool"), 0, Pool));

        for (size_t Box = 0; Box < Boxes.size(); ++Box)
        {
            Result.append(MakeLadderEntry(QStringLiteral("box"), static_cast<int>(Box) + 1, Boxes[Box]));
        }

        Result.append(MakeLadderEntry(QStringLiteral("learned"), LastStage + 1, Learned));

        return Result;
    }
}

DailySessionController::DailySessionController(QObject* Parent)
    : QObject(Parent)
{
    refresh();

    QTimer* DayTimer = new QTimer(this);
    connect(DayTimer, &QTimer::timeout, this, [this]()
    {
        if (LearningDate != QDate{1970, 1, 1}.addDays(GetTodayDayNumber()))
        {
            refresh();
        }
    });
    DayTimer->start(1000);
}

QDate DailySessionController::GetLearningDate() const
{
    return LearningDate;
}

int DailySessionController::GetWeekday() const
{
    return LearningDate.dayOfWeek();
}

void DailySessionController::refresh()
{
    const FTDayNumber Today = GetTodayDayNumber();
    LearningDate = QDate{1970, 1, 1}.addDays(Today);
    DueCount = 0;
    PoolCount = 0;
    LearnedToday = 0;
    ReviewedToday = 0;
    ReviewTimeline.clear();
    RecentProgress.clear();
    Ladder.clear();

    if (AppDatabase::Get().IsReady())
    {
        FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

        DueCount = CardRepository.CountDueCards(Today);
        PoolCount = CardRepository.CountUnlearnedCards();

        const std::vector<FTDailyStats> TodayStats = CardRepository.LoadDailyStats(Today, Today);

        if (!TodayStats.empty())
        {
            LearnedToday = TodayStats.front().LearnedCount;
            ReviewedToday = TodayStats.front().ReviewedCount;
        }

        std::unordered_map<int, int> DueByDay;

        for (const FTDueCountForDay& Entry :
            CardRepository.CountDueCardsByDay(Today + 1, Today + ReviewTimelineDays - 1))
        {
            DueByDay[static_cast<int>(Entry.Day)] = Entry.Count;
        }

        for (int Offset = 0; Offset < ReviewTimelineDays; ++Offset)
        {
            QVariantMap Entry;
            Entry[QStringLiteral("daysFromToday")] = Offset;

            // Today counts everything owed, overdue included, which is why it is asked for
            // separately rather than read off the exact-day count everything after it uses.
            if (Offset == 0)
            {
                Entry[QStringLiteral("count")] = DueCount;
            }
            else
            {
                const auto Found = DueByDay.find(static_cast<int>(Today) + Offset);

                Entry[QStringLiteral("count")] = Found != DueByDay.end() ? Found->second : 0;
            }

            ReviewTimeline.append(Entry);
        }

        std::unordered_map<int, FTDailyStats> StatsByDay;

        for (const FTDailyStats& Stats :
            CardRepository.LoadDailyStats(Today - RecentProgressDays + 1, Today))
        {
            StatsByDay[static_cast<int>(Stats.Day)] = Stats;
        }

        for (int Offset = RecentProgressDays - 1; Offset >= 0; --Offset)
        {
            QVariantMap Entry;
            Entry[QStringLiteral("daysAgo")] = Offset;

            const auto Found = StatsByDay.find(static_cast<int>(Today) - Offset);
            const bool bRecorded = Found != StatsByDay.end();

            double Review = 0.0;
            double Learn = 0.0;

            if (bRecorded)
            {
                const FTDailyStats& Stats = Found->second;
                const int Owed = Stats.ReviewedCount + Stats.DueCount;
                const int Goal = Stats.GoalCount > 0 ? Stats.GoalCount : GetDailyGoal();

                // A day with nothing owed counts as a finished review.
                Review = Owed > 0 ? static_cast<double>(Stats.ReviewedCount) / Owed : 1.0;
                Learn = Goal > 0 ? std::min(static_cast<double>(Stats.LearnedCount) / Goal, 1.0) : 0.0;
            }

            Entry[QStringLiteral("recorded")] = bRecorded;
            Entry[QStringLiteral("review")] = Review;
            Entry[QStringLiteral("learn")] = Learn;
            Entry[QStringLiteral("learnedCount")] = bRecorded ? Found->second.LearnedCount : 0;

            RecentProgress.append(Entry);
        }

        Ladder = BuildLadder(CardRepository.CountCardsByStage());
    }

    emit countsChanged();
}

namespace
{
    std::vector<FTCardHandle> LoadWaitingFormsCards(const std::string& Training)
    {
        std::vector<FTCardHandle> Waiting;

        for (const FTCardHandle& Card : AppDatabase::Get().GetCardRepository().LoadFormsCards())
        {
            const std::string Tense = GetFormsTense(Card->Id);

            if (!IsCompleted(*Card) && GetFormsTraining(Card->Id) == Training
                && (Tense.empty() || Tense == FormsTensePresent))
            {
                Waiting.push_back(Card);
            }
        }

        return Waiting;
    }
}

QStringList DailySessionController::collectSessionCards() const
{
    QStringList Result;

    if (!AppDatabase::Get().IsReady())
    {
        return Result;
    }

    const std::vector<FTCardHandle> Picked = PickTrainingCards(AppDatabase::Get().GetCardRepository().LoadUnlearnedCards(),
                                                               static_cast<size_t>(std::max(GetTrainingWordCount(), 0)));

    for (const FTCardHandle& Word : Picked)
    {
        Result.append(QString::fromStdString(Word->Id));
    }

    qCDebug(LogSession) << "Training picks" << Result.size() << "cards from the pool";

    return Result;
}

int DailySessionController::shortfall() const
{
    return std::max(GetTrainingWordCount() - static_cast<int>(collectSessionCards().size()), 0);
}

QString DailySessionController::formsTrainingId(const QString& Training) const
{
    const FTLanguageRules& Languages = FTSettings::Get().GetLanguageRules();

    if (!SupportsFormsTraining(Training.toStdString(), Languages.TargetLanguage))
    {
        return {};
    }

    return QString::fromStdString(
        MakeFormsTrainingId(Training.toStdString(), Languages.TargetLanguage, Languages.NativeLanguage));
}

int DailySessionController::formsTrainingSize(const QString& TrainingId) const
{
    return GetFormsTrainingSize(GetFormsTraining(TrainingId.toStdString()));
}

void DailySessionController::DropStaleFormsCache() const
{
    const std::string Revision = AppDatabase::Get().GetDataRevision() + "/"
        + std::to_string(AppDictionary::Get().GetGeneration());

    if (Revision == FormsCacheRevision)
    {
        return;
    }

    FormsCacheRevision = Revision;
    FormsWordCounts.clear();
    FormsTenses.clear();
}

int DailySessionController::formsWordCount(const QString& TrainingId) const
{
    if (!AppDatabase::Get().IsReady() || !IsFormsTrainingId(TrainingId.toStdString()))
    {
        return 0;
    }

    DropStaleFormsCache();

    const std::string Id = TrainingId.toStdString();
    const auto Cached = FormsWordCounts.find(Id);

    if (Cached != FormsWordCounts.end())
    {
        return Cached->second;
    }

    const std::string Training = GetFormsTraining(Id);
    const int Count = static_cast<int>(LoadWaitingFormsCards(Training).size()
                                       + AppCards::ListFormsCandidates(Training).size());

    FormsWordCounts.emplace(Id, Count);

    return Count;
}

bool DailySessionController::canStartFormsTraining(const QString& TrainingId) const
{
    const QVariantList Tenses = formsTenses(TrainingId);

    if (Tenses.isEmpty())
    {
        return formsWordCount(TrainingId) >= formsTrainingSize(TrainingId);
    }

    return std::any_of(Tenses.begin(), Tenses.end(), [](const QVariant& Tense)
    {
        return Tense.toMap().value(QStringLiteral("enabled")).toBool();
    });
}

QStringList DailySessionController::collectFormsCards(const QString& TrainingId) const
{
    QStringList Result;

    if (!AppDatabase::Get().IsReady() || !IsFormsTrainingId(TrainingId.toStdString()))
    {
        return Result;
    }

    const std::string Training = GetFormsTraining(TrainingId.toStdString());
    const int Wanted = formsTrainingSize(TrainingId);

    for (const FTCardHandle& Card : TakeFormsSession(Training, LoadWaitingFormsCards(Training)))
    {
        Result.append(QString::fromStdString(Card->Id));
    }

    std::vector<FTCard> Candidates = AppCards::ListFormsCandidates(Training);

    if (Result.size() + static_cast<int>(Candidates.size()) < Wanted)
    {
        return {};
    }

    std::mt19937 Generator{MakeRandomSeed()};
    std::shuffle(Candidates.begin(), Candidates.end(), Generator);

    AppCards::PrioritizeIrregularVerbs(Candidates);

    Candidates.resize(static_cast<size_t>(Wanted - Result.size()));

    if (!Candidates.empty() && !AppDatabase::Get().GetCardRepository().AddFormsCards(Candidates))
    {
        qCWarning(LogApp) << "Could not add forms cards for" << TrainingId;

        return {};
    }

    for (const FTCard& Card : Candidates)
    {
        Result.append(QString::fromStdString(Card.Id));
    }

    qCInfo(LogApp) << "Forms training" << TrainingId << "takes" << Result.size() << "words," << Candidates.size()
                   << "of them new";

    return Result;
}

QVariantList DailySessionController::formsTenses(const QString& TrainingId) const
{
    QVariantList Result;

    if (GetFormsTraining(TrainingId.toStdString()) != FormsTrainingConjugation)
    {
        return Result;
    }

    DropStaleFormsCache();

    const auto Cached = FormsTenses.find(TrainingId.toStdString());

    if (Cached != FormsTenses.end())
    {
        return Cached->second;
    }

    const int Wanted = formsTrainingSize(TrainingId);

    for (const FTenseOffer& Offer : AppCards::ListTenseOffers())
    {
        const bool bFromTriage = Offer.Tense == FormsTensePresent;
        const int Count = bFromTriage ? formsWordCount(TrainingId)
                                      : static_cast<int>(Offer.WaitingCardIds.size() + Offer.ReadyCards.size());

        QVariantMap Entry;
        Entry[QStringLiteral("key")] = QString::fromStdString(Offer.Tense);
        Entry[QStringLiteral("label")] = QString::fromStdString(Offer.Label);
        Entry[QStringLiteral("count")] = Count;
        Entry[QStringLiteral("wanted")] = Wanted;
        Entry[QStringLiteral("fromTriage")] = bFromTriage;
        Entry[QStringLiteral("enabled")] = Count >= Wanted;

        Result.append(Entry);
    }

    FormsTenses.emplace(TrainingId.toStdString(), Result);

    return Result;
}

QStringList DailySessionController::collectTenseCards(const QString& TrainingId, const QString& Tense) const
{
    QStringList Result;

    const int Wanted = formsTrainingSize(TrainingId);

    for (const FTenseOffer& Offer : AppCards::ListTenseOffers())
    {
        const bool bFull = static_cast<int>(Offer.WaitingCardIds.size() + Offer.ReadyCards.size()) >= Wanted;

        if (Offer.Tense != Tense.toStdString() || !bFull)
        {
            continue;
        }

        for (const std::string& CardId : Offer.WaitingCardIds)
        {
            if (Result.size() < Wanted)
            {
                Result.append(QString::fromStdString(CardId));
            }
        }

        std::vector<FTCard> Ready = Offer.ReadyCards;

        AppCards::PrioritizeIrregularVerbs(Ready);

        std::vector<FTCard> Created;

        for (const FTCard& Card : Ready)
        {
            if (Result.size() + static_cast<int>(Created.size()) < Wanted)
            {
                Created.push_back(Card);
            }
        }

        if (Created.empty())
        {
            continue;
        }

        if (!AppDatabase::Get().GetCardRepository().AddFormsCards(Created))
        {
            qCWarning(LogApp) << "Could not add" << Tense << "forms cards";

            return {};
        }

        for (const FTCard& Card : Created)
        {
            Result.append(QString::fromStdString(Card.Id));
        }
    }

    return Result;
}

int DailySessionController::GetDueCount() const
{
    return DueCount;
}

int DailySessionController::GetNewWordCount() const
{
    return std::min(PoolCount, GetTrainingWordCount());
}

int DailySessionController::GetPoolCount() const
{
    return PoolCount;
}

int DailySessionController::GetWantedFromTriage() const
{
    return std::max(GetTrainingWordCount() - PoolCount, 0);
}

int DailySessionController::GetDailyGoal() const
{
    return FTSettings::Get().GetIntensiveLearningRules().GetDailyGoal();
}

int DailySessionController::GetWordsLeftForGoal() const
{
    return std::max(GetDailyGoal() - LearnedToday, 0);
}

int DailySessionController::GetLearnedToday() const
{
    return LearnedToday;
}

int DailySessionController::GetReviewedToday() const
{
    return ReviewedToday;
}

int DailySessionController::GetTrainingWordCount() const
{
    return FTSettings::Get().GetIntensiveLearningRules().TrainingWordCount;
}

int DailySessionController::GetTotalCount() const
{
    return DueCount + GetNewWordCount();
}

QVariantList DailySessionController::GetReviewTimeline() const
{
    return ReviewTimeline;
}

QVariantList DailySessionController::GetRecentProgress() const
{
    return RecentProgress;
}

QVariantList DailySessionController::GetLadder() const
{
    return Ladder;
}
