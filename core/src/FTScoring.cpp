#include "core/FTScoring.h"

#include <algorithm>

int CalculateCardScore(EFTAnswerGrade Grade,
                       std::chrono::milliseconds Elapsed,
                       std::chrono::milliseconds Buffer,
                       const FTScoringRules& Rules)
{
    if (Grade == EFTAnswerGrade::Skipped)
    {
        return Rules.SkippedScore;
    }

    if (Grade == EFTAnswerGrade::Wrong)
    {
        return Rules.IncorrectScore;
    }

    if (Elapsed <= Buffer)
    {
        return Rules.MaximumCardScore;
    }

    const std::chrono::milliseconds Overrun = Elapsed - Buffer;

    // Any started second past the buffer costs a full point, so round up rather than down.
    const long long PenaltySeconds = std::chrono::ceil<std::chrono::seconds>(Overrun).count();
    const long long Score = Rules.MaximumCardScore - PenaltySeconds;

    return static_cast<int>(std::max<long long>(Score, Rules.MinimumCorrectScore));
}
