#pragma once

#include "core/FTSettings.h"

struct FSessionSettingsGuard
{
    FTSessionExerciseRules Training = FTSettings::Get().GetSessionExercises(TrainingSessionGroup);
    FTSessionExerciseRules Review = FTSettings::Get().GetSessionExercises(ReviewSessionGroup);

    ~FSessionSettingsGuard()
    {
        FTSettings::Get().SetSessionExercises(TrainingSessionGroup, Training);
        FTSettings::Get().SetSessionExercises(ReviewSessionGroup, Review);
    }
};
