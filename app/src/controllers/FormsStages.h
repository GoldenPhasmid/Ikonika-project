#pragma once

#include "ExerciseControllerBase.h"
#include "ExerciseFlowStages.h"
#include "IExerciseFlowStage.h"

#include "core/sessions/FTExerciseSession.h"

#include <QObject>

#include <cstdint>
#include <memory>
#include <vector>

struct FFormsStages
{
    std::vector<std::unique_ptr<IExerciseFlowStage>> Stages;
    std::vector<ExerciseControllerBase*> Controllers;
    FMasteryLoopStage* TypingStage = nullptr;
};

FFormsStages BuildFormsStages(QObject* Parent, const FTSessionStartParams& Params, uint32_t SentenceSeed);
