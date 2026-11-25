#include "core/FTSchedule.h"

#include "core/FTLog.h"

#include <QString>

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double Decay = -0.5;
    constexpr double Factor = 19.0 / 81.0;
    constexpr double MinimumStability = 0.01;
    constexpr double MaximumStability = 36500.0;
    constexpr int GradeAgain = 1;
    constexpr int GradeGood = 3;
    constexpr int GradeEasy = 4;

    double GetTargetRetention()
    {
        return FTSettings::Get().GetScheduleRules().GetIntensity().TargetRetention / 100.0;
    }

    double ClampStability(double Stability)
    {
        return std::clamp(Stability, MinimumStability, MaximumStability);
    }

    double ClampDifficulty(double Difficulty)
    {
        return std::clamp(Difficulty, 1.0, 10.0);
    }

    int GetGrade(bool bRemembered)
    {
        return bRemembered ? GradeGood : GradeAgain;
    }

    double GetInitialDifficulty(const FTScheduleWeights& Weights, int Grade)
    {
        return Weights[4] - std::exp(Weights[5] * (Grade - 1)) + 1.0;
    }

    double GetNextDifficulty(const FTScheduleWeights& Weights, double Difficulty, int Grade)
    {
        const double Change = -Weights[6] * (Grade - GradeGood);
        const double Damped = Difficulty + Change * (10.0 - Difficulty) / 9.0;
        const double Reverted = Weights[7] * GetInitialDifficulty(Weights, GradeEasy) + (1.0 - Weights[7]) * Damped;

        return ClampDifficulty(Reverted);
    }

    double GetRecallStability(const FTScheduleWeights& Weights, double Stability, double Difficulty,
                              double Retrievability)
    {
        const double Growth = std::exp(Weights[8]) * (11.0 - Difficulty) * std::pow(Stability, -Weights[9])
            * (std::exp(Weights[10] * (1.0 - Retrievability)) - 1.0);

        return ClampStability(Stability * (Growth + 1.0));
    }

    double GetForgetStability(const FTScheduleWeights& Weights, double Stability, double Difficulty,
                              double Retrievability)
    {
        const double Relearned = Weights[11] * std::pow(Difficulty, -Weights[12])
            * (std::pow(Stability + 1.0, Weights[13]) - 1.0) * std::exp(Weights[14] * (1.0 - Retrievability));
        const double Ceiling = Stability / std::exp(Weights[17] * Weights[18]);

        return ClampStability(std::min(Relearned, Ceiling));
    }

    double GetSameDayStability(const FTScheduleWeights& Weights, double Stability, int Grade)
    {
        return ClampStability(Stability * std::exp(Weights[17] * (Grade - GradeGood + Weights[18])));
    }

    int GetElapsedDays(const FTCard& Subject, FTDayNumber Today)
    {
        if (Subject.BoxChangedDate.has_value())
        {
            return std::max(0, Today - *Subject.BoxChangedDate);
        }

        return static_cast<int>(std::lround(*Subject.Stability));
    }

    void ApplyAnswer(FTCard& Subject, bool bRemembered, FTDayNumber Today)
    {
        const FTScheduleWeights& Weights = FTSettings::Get().GetScheduleRules().Weights;
        const bool bHasState = Subject.Stability.has_value() && Subject.Difficulty.has_value();
        const FTMemoryState Next = bHasState
            ? GetNextMemoryState(Weights, {*Subject.Stability, *Subject.Difficulty}, bRemembered,
                                 GetElapsedDays(Subject, Today))
            : GetFirstMemoryState(Weights, bRemembered);

        Subject.Stability = Next.Stability;
        Subject.Difficulty = Next.Difficulty;
    }

    void Answer(FTCard& Subject, bool bRemembered, FTDayNumber Today)
    {
        ApplyAnswer(Subject, bRemembered, Today);

        Subject.BoxChangedDate = Today;
        Subject.CompletedDate.reset();

        ScheduleFromStability(Subject, Today);
    }

    QString DescribeDay(const std::optional<FTDayNumber>& Day)
    {
        return Day.has_value() ? QString::number(*Day) : QStringLiteral("none");
    }

    QString DescribeValue(const std::optional<double>& Value)
    {
        return Value.has_value() ? QString::number(*Value, 'f', 2) : QStringLiteral("none");
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
        << "card " << QString::fromStdString(After.Id) << " \"" << QString::fromStdString(After.TargetWord) << "\" "
        << Reason << ": stage " << DescribeStage(Before) << " -> " << DescribeStage(After) << ", due "
        << DescribeDay(Before.DueDate) << " -> " << DescribeDay(After.DueDate) << ", stability "
        << DescribeValue(Before.Stability) << " -> " << DescribeValue(After.Stability) << ", difficulty "
        << DescribeValue(Before.Difficulty) << " -> " << DescribeValue(After.Difficulty);
}

int GetLastStage()
{
    return ProgressStageCount;
}

FTMemoryState GetFirstMemoryState(const FTScheduleWeights& Weights, bool bRemembered)
{
    const int Grade = GetGrade(bRemembered);

    return {ClampStability(Weights[static_cast<size_t>(Grade - 1)]),
            ClampDifficulty(GetInitialDifficulty(Weights, Grade))};
}

FTMemoryState GetNextMemoryState(const FTScheduleWeights& Weights, const FTMemoryState& State, bool bRemembered,
                                 int ElapsedDays)
{
    const int Grade = GetGrade(bRemembered);
    const double Difficulty = GetNextDifficulty(Weights, State.Difficulty, Grade);

    if (ElapsedDays <= 0)
    {
        return {GetSameDayStability(Weights, State.Stability, Grade), Difficulty};
    }

    const double Retrievability = GetRetrievability(State.Stability, ElapsedDays);
    const double Stability = bRemembered
        ? GetRecallStability(Weights, State.Stability, State.Difficulty, Retrievability)
        : GetForgetStability(Weights, State.Stability, State.Difficulty, Retrievability);

    return {Stability, Difficulty};
}

double GetRetrievability(double Stability, int ElapsedDays)
{
    return std::pow(1.0 + Factor * std::max(ElapsedDays, 0) / ClampStability(Stability), Decay);
}

int GetNextIntervalDays(double Stability)
{
    const double Days = ClampStability(Stability) / Factor * (std::pow(GetTargetRetention(), 1.0 / Decay) - 1.0);

    return std::clamp(static_cast<int>(std::lround(Days)), 1, MaximumIntervalDays);
}

int GetProgressStage(double Stability)
{
    const double LearnedStability = FTSettings::Get().GetScheduleRules().GetIntensity().LearnedStability;
    const double Progress = std::log(std::max(Stability, 1.0)) / std::log(LearnedStability);
    const int Stage = FirstLadderStage + static_cast<int>(std::floor(Progress * ProgressStageCount));

    return std::clamp(Stage, FirstLadderStage, ProgressStageCount);
}

bool IsDue(const FTCard& Subject, FTDayNumber Today)
{
    if (IsCompleted(Subject) || !Subject.DueDate.has_value())
    {
        return false;
    }

    return *Subject.DueDate <= Today;
}

bool IsQuickReviewCard(const FTCard& Subject)
{
    if (!Subject.Stability.has_value())
    {
        return false;
    }

    return *Subject.Stability >= FTSettings::Get().GetScheduleRules().GetIntensity().QuickReviewStability;
}

void ScheduleFromStability(FTCard& Subject, FTDayNumber Today)
{
    if (!Subject.Stability.has_value())
    {
        return;
    }

    if (*Subject.Stability >= FTSettings::Get().GetScheduleRules().GetIntensity().LearnedStability)
    {
        Subject.LeitnerStage = ProgressStageCount;
        Subject.CompletedDate = Today;
        Subject.DueDate.reset();

        return;
    }

    Subject.LeitnerStage = GetProgressStage(*Subject.Stability);
    Subject.DueDate = Today + GetNextIntervalDays(*Subject.Stability);
}

void LearnCard(FTCard& Subject, FTDayNumber Today)
{
    Subject.Stability.reset();
    Subject.Difficulty.reset();

    Answer(Subject, true, Today);
}

void AnswerReview(FTCard& Subject, bool bRemembered, FTDayNumber Today)
{
    Answer(Subject, bRemembered, Today);
}

void PlaceWithStability(FTCard& Subject, double Stability, FTDayNumber Today)
{
    Subject.Stability = ClampStability(Stability);

    if (!Subject.Difficulty.has_value())
    {
        Subject.Difficulty = GetFirstMemoryState(FTSettings::Get().GetScheduleRules().Weights, true).Difficulty;
    }

    Subject.BoxChangedDate = Today;
    Subject.CompletedDate.reset();

    ScheduleFromStability(Subject, Today);
}

void ReturnToPool(FTCard& Subject, FTDayNumber Today)
{
    Subject.LeitnerStage = PoolStage;
    Subject.BoxChangedDate = Today;
    Subject.CompletedDate.reset();
    Subject.DueDate.reset();
    Subject.Stability.reset();
    Subject.Difficulty.reset();
}

void MarkCompleted(FTCard& Subject, FTDayNumber Today)
{
    Subject.LeitnerStage = ProgressStageCount;
    Subject.CompletedDate = Today;
    Subject.BoxChangedDate = Today;
    Subject.DueDate.reset();
}

FTSchedulePreview PreviewSchedule(int ForgottenReview)
{
    constexpr int MaximumReviews = 50;

    FTSchedulePreview Preview;
    FTCard Subject;

    LearnCard(Subject, 0);

    while (Subject.DueDate.has_value() && static_cast<int>(Preview.ReviewDays.size()) < MaximumReviews)
    {
        const int Day = *Subject.DueDate;
        const bool bRemembered = static_cast<int>(Preview.ReviewDays.size()) != ForgottenReview;

        Preview.ReviewDays.push_back(Day);
        Preview.QuickReviews.push_back(IsQuickReviewCard(Subject));

        AnswerReview(Subject, bRemembered, Day);
    }

    Preview.CompletedDay = Subject.CompletedDate.value_or(0);

    return Preview;
}
