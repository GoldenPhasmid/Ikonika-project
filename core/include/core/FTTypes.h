#pragma once

#include <cstdint>

using FTDayNumber = int32_t;
using FTTimestamp = int64_t;

constexpr int SHOW_WORD = 0;
constexpr int SHOW_RANDOM = 1;
constexpr int SHOW_TRANSLATION = 2;
constexpr int QuestionSideCount = 3;

struct FTMixedExerciseKind
{
    bool bAudio = false;
    bool bAskWord = false;

    bool operator==(const FTMixedExerciseKind&) const = default;
};
