#include "core/FTScheduleOptimizer.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"
#include "core/FTSchedule.h"

#include <QElapsedTimer>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace
{
    constexpr std::array<double, ScheduleWeightCount> WeightDeviations{6.61, 9.52, 17.69, 27.74, 0.55, 0.28, 0.67,
                                                                       0.12, 0.4,  0.18,  0.34,  0.27, 0.08, 0.14,
                                                                       0.57, 0.25, 1.03,  0.27,  0.39};

    constexpr std::array<size_t, 13> TrainedWeights{4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17, 18};
    constexpr size_t FirstRecallStabilityWeight = 2;

    constexpr int EpochCount = 5;
    constexpr int BatchReviewCount = 512;
    constexpr double LearningRate = 0.04;
    constexpr double FirstMomentDecay = 0.9;
    constexpr double SecondMomentDecay = 0.999;
    constexpr double AdamEpsilon = 1e-8;
    constexpr double GradientStep = 1e-4;
    constexpr double PriorStrength = 1.0;
    constexpr double ProbabilityMargin = 1e-4;
    constexpr int SearchIterations = 80;
    constexpr unsigned ShuffleSeed = 2023;

    struct FLossTotal
    {
        double Sum = 0.0;
        int Count = 0;
    };

    struct FFirstReview
    {
        int ElapsedDays = 0;
        bool bRemembered = true;
    };

    double GetAnswerLoss(double Retrievability, bool bRemembered)
    {
        const double Clamped = std::clamp(Retrievability, ProbabilityMargin, 1.0 - ProbabilityMargin);

        return -std::log(bRemembered ? Clamped : 1.0 - Clamped);
    }

    void AddHistoryLoss(const FTScheduleWeights& Weights, const FTReviewHistory& History, FLossTotal& Total)
    {
        FTMemoryState State = GetFirstMemoryState(Weights, History.front().bRemembered);

        for (size_t Index = 1; Index < History.size(); ++Index)
        {
            const FTReviewAnswer& Answer = History[Index];
            const int ElapsedDays = Answer.Day - History[Index - 1].Day;

            if (ElapsedDays > 0)
            {
                Total.Sum += GetAnswerLoss(GetRetrievability(State.Stability, ElapsedDays), Answer.bRemembered);
                ++Total.Count;
            }

            State = GetNextMemoryState(Weights, State, Answer.bRemembered, ElapsedDays);
        }
    }

    int CountHistoryReviews(const FTReviewHistory& History)
    {
        int Count = 0;

        for (size_t Index = 1; Index < History.size(); ++Index)
        {
            if (History[Index].Day > History[Index - 1].Day)
            {
                ++Count;
            }
        }

        return Count;
    }

    double GetWeightPenalty(double Weight, size_t Index)
    {
        const double Distance = (Weight - DefaultScheduleWeights[Index]) / WeightDeviations[Index];

        return PriorStrength * Distance * Distance;
    }

    double GetPriorPenalty(const FTScheduleWeights& Weights)
    {
        double Penalty = 0.0;

        for (size_t Index = 0; Index < ScheduleWeightCount; ++Index)
        {
            Penalty += GetWeightPenalty(Weights[Index], Index);
        }

        return Penalty;
    }

    double GetBatchObjective(const FTScheduleWeights& Weights, const std::vector<FTReviewHistory>& Histories,
                             const std::vector<size_t>& Batch, int TotalReviews)
    {
        FLossTotal Total;

        for (const size_t Index : Batch)
        {
            AddHistoryLoss(Weights, Histories[Index], Total);
        }

        if (Total.Count == 0)
        {
            return 0.0;
        }

        return Total.Sum / Total.Count + GetPriorPenalty(Weights) / TotalReviews;
    }

    std::vector<FFirstReview> CollectFirstReviews(const std::vector<FTReviewHistory>& Histories)
    {
        std::vector<FFirstReview> Result;

        for (const FTReviewHistory& History : Histories)
        {
            if (History.size() < 2 || !History.front().bRemembered)
            {
                continue;
            }

            const int ElapsedDays = History[1].Day - History[0].Day;

            if (ElapsedDays > 0)
            {
                Result.push_back({ElapsedDays, History[1].bRemembered});
            }
        }

        return Result;
    }

    double GetFirstReviewObjective(double Stability, const std::vector<FFirstReview>& Reviews)
    {
        double Sum = GetWeightPenalty(Stability, FirstRecallStabilityWeight);

        for (const FFirstReview& Review : Reviews)
        {
            Sum += GetAnswerLoss(GetRetrievability(Stability, Review.ElapsedDays), Review.bRemembered);
        }

        return Sum;
    }

    double FitFirstRecallStability(const std::vector<FTReviewHistory>& Histories)
    {
        const std::vector<FFirstReview> Reviews = CollectFirstReviews(Histories);

        if (Reviews.empty())
        {
            return DefaultScheduleWeights[FirstRecallStabilityWeight];
        }

        const double GoldenRatio = (std::sqrt(5.0) - 1.0) / 2.0;
        double Low = std::log(0.1);
        double High = std::log(100.0);

        for (int Iteration = 0; Iteration < SearchIterations; ++Iteration)
        {
            const double Left = High - GoldenRatio * (High - Low);
            const double Right = Low + GoldenRatio * (High - Low);

            if (GetFirstReviewObjective(std::exp(Left), Reviews) < GetFirstReviewObjective(std::exp(Right), Reviews))
            {
                High = Right;
            }
            else
            {
                Low = Left;
            }
        }

        return std::exp((Low + High) / 2.0);
    }

    void Shuffle(std::vector<size_t>& Items, std::mt19937& Engine)
    {
        for (size_t Index = Items.size(); Index > 1; --Index)
        {
            std::swap(Items[Index - 1], Items[Engine() % Index]);
        }
    }

    std::vector<std::vector<size_t>> SplitBatches(const std::vector<size_t>& Order,
                                                  const std::vector<int>& ReviewCounts)
    {
        std::vector<std::vector<size_t>> Batches;
        std::vector<size_t> Current;
        int CurrentReviews = 0;

        for (const size_t Index : Order)
        {
            Current.push_back(Index);
            CurrentReviews += ReviewCounts[Index];

            if (CurrentReviews >= BatchReviewCount)
            {
                Batches.push_back(std::move(Current));
                Current.clear();
                CurrentReviews = 0;
            }
        }

        if (!Current.empty())
        {
            Batches.push_back(std::move(Current));
        }

        return Batches;
    }

    QString DescribeWeights(const FTScheduleWeights& Weights)
    {
        QStringList Parts;

        for (const double Weight : Weights)
        {
            Parts.append(QString::number(Weight, 'f', 4));
        }

        return Parts.join(QStringLiteral(", "));
    }
}

std::vector<FTReviewHistory> BuildReviewHistories(
    const std::unordered_map<std::string, std::vector<FTJournalEvent>>& EventsByCard)
{
    std::vector<std::string> CardIds;
    CardIds.reserve(EventsByCard.size());

    for (const auto& Entry : EventsByCard)
    {
        CardIds.push_back(Entry.first);
    }

    std::sort(CardIds.begin(), CardIds.end());

    std::vector<FTReviewHistory> Histories;

    for (const std::string& CardId : CardIds)
    {
        std::vector<FTJournalEvent> Events = EventsByCard.at(CardId);
        SortJournalEvents(Events);

        FTReviewHistory Current;
        bool bOpen = false;

        const auto Close = [&Histories, &Current, &bOpen]()
        {
            if (Current.size() >= 2)
            {
                Histories.push_back(std::move(Current));
            }

            Current.clear();
            bOpen = false;
        };

        for (const FTJournalEvent& Event : Events)
        {
            if (Event.EventId == JournalLearned)
            {
                Close();
                Current.push_back({Event.Day, true});
                bOpen = true;
            }
            else if (Event.EventId == JournalReviewed)
            {
                if (bOpen)
                {
                    Current.push_back({Event.Day, Event.EventValue != 0});
                }
            }
            else
            {
                Close();
            }
        }

        Close();
    }

    return Histories;
}

std::vector<FTReviewHistory> LoadReviewHistories(FTDatabase& Source)
{
    std::unordered_map<std::string, std::vector<FTJournalEvent>> EventsByCard;

    if (!LoadJournalByCard(Source, EventsByCard))
    {
        qCWarning(LogSchedule) << "Could not read the journal to fit the schedule";

        return {};
    }

    return BuildReviewHistories(EventsByCard);
}

int CountTrainingReviews(const std::vector<FTReviewHistory>& Histories)
{
    int Count = 0;

    for (const FTReviewHistory& History : Histories)
    {
        Count += CountHistoryReviews(History);
    }

    return Count;
}

bool ShouldOptimizeSchedule(int TrainingReviews, int FittedReviewCount)
{
    return TrainingReviews >= MinimumOptimizerReviews && TrainingReviews >= FittedReviewCount + OptimizerRefitReviews;
}

double GetScheduleLoss(const FTScheduleWeights& Weights, const std::vector<FTReviewHistory>& Histories)
{
    FLossTotal Total;

    for (const FTReviewHistory& History : Histories)
    {
        AddHistoryLoss(Weights, History, Total);
    }

    return Total.Count > 0 ? Total.Sum / Total.Count : 0.0;
}

FTScheduleFit OptimizeScheduleWeights(const std::vector<FTReviewHistory>& Histories)
{
    QElapsedTimer Timer;
    Timer.start();

    FTScheduleFit Fit;
    Fit.ReviewCount = CountTrainingReviews(Histories);
    Fit.DefaultLoss = GetScheduleLoss(DefaultScheduleWeights, Histories);
    Fit.FittedLoss = Fit.DefaultLoss;

    if (Fit.ReviewCount == 0)
    {
        return Fit;
    }

    FTScheduleWeights Weights = DefaultScheduleWeights;
    Weights[FirstRecallStabilityWeight] = FitFirstRecallStability(Histories);
    Sanitise(Weights);

    std::vector<int> ReviewCounts;
    std::vector<size_t> Order;
    ReviewCounts.reserve(Histories.size());

    for (size_t Index = 0; Index < Histories.size(); ++Index)
    {
        ReviewCounts.push_back(CountHistoryReviews(Histories[Index]));

        if (ReviewCounts.back() > 0)
        {
            Order.push_back(Index);
        }
    }

    const int BatchesPerEpoch = (Fit.ReviewCount + BatchReviewCount - 1) / BatchReviewCount;
    const int TotalSteps = std::max(1, EpochCount * BatchesPerEpoch);

    std::array<double, TrainedWeights.size()> FirstMoment{};
    std::array<double, TrainedWeights.size()> SecondMoment{};
    std::mt19937 Engine{ShuffleSeed};
    int Step = 0;

    for (int Epoch = 0; Epoch < EpochCount; ++Epoch)
    {
        Shuffle(Order, Engine);

        for (const std::vector<size_t>& Batch : SplitBatches(Order, ReviewCounts))
        {
            ++Step;

            const double Progress = std::min(1.0, static_cast<double>(Step - 1) / TotalSteps);
            const double StepRate = LearningRate * 0.5 * (1.0 + std::cos(Progress * 3.14159265358979323846));

            std::array<double, TrainedWeights.size()> Gradient{};

            for (size_t Slot = 0; Slot < TrainedWeights.size(); ++Slot)
            {
                const size_t Index = TrainedWeights[Slot];
                FTScheduleWeights Above = Weights;
                FTScheduleWeights Below = Weights;
                Above[Index] += GradientStep;
                Below[Index] -= GradientStep;

                Gradient[Slot] = (GetBatchObjective(Above, Histories, Batch, Fit.ReviewCount)
                                  - GetBatchObjective(Below, Histories, Batch, Fit.ReviewCount))
                    / (2.0 * GradientStep);
            }

            const double FirstCorrection = 1.0 - std::pow(FirstMomentDecay, Step);
            const double SecondCorrection = 1.0 - std::pow(SecondMomentDecay, Step);

            for (size_t Slot = 0; Slot < TrainedWeights.size(); ++Slot)
            {
                FirstMoment[Slot] = FirstMomentDecay * FirstMoment[Slot] + (1.0 - FirstMomentDecay) * Gradient[Slot];
                SecondMoment[Slot] = SecondMomentDecay * SecondMoment[Slot]
                    + (1.0 - SecondMomentDecay) * Gradient[Slot] * Gradient[Slot];

                const double Mean = FirstMoment[Slot] / FirstCorrection;
                const double Variance = SecondMoment[Slot] / SecondCorrection;

                Weights[TrainedWeights[Slot]] -= StepRate * Mean / (std::sqrt(Variance) + AdamEpsilon);
            }

            Sanitise(Weights);
        }
    }

    const double FittedLoss = GetScheduleLoss(Weights, Histories);

    if (FittedLoss < Fit.DefaultLoss)
    {
        Fit.Weights = Weights;
        Fit.FittedLoss = FittedLoss;
    }

    qCInfo(LogSchedule).noquote() << "Fitted the schedule to" << Fit.ReviewCount << "reviews in" << Timer.elapsed()
                                  << "ms, loss" << Fit.DefaultLoss << "->" << Fit.FittedLoss << ", weights"
                                  << DescribeWeights(Fit.Weights);

    return Fit;
}
