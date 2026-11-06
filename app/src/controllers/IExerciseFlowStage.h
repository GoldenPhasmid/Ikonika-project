#pragma once

#include "ExerciseControllerBase.h"

#include <QString>

#include <optional>
#include <vector>

class IExerciseFlowStage
{
public:
    virtual ~IExerciseFlowStage() = default;

    virtual ExerciseControllerBase* GetActiveController() const = 0;
    virtual std::vector<ExerciseControllerBase*> GetControllers() const = 0;

    virtual void Start() = 0;

    virtual void OnControllerStateChanged() = 0;

    virtual bool IsFinished() const = 0;

    virtual void ForceComplete() = 0;
    virtual void ForceRestart() = 0;

    virtual bool SkipPhase() { return false; }

    virtual void RestoreActiveExercise(std::optional<FExerciseCheckpoint> Checkpoint) { Q_UNUSED(Checkpoint); }
};
