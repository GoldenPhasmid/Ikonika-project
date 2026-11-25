#pragma once

#include "core/FTSettings.h"
#include "core/FTTypes.h"

#include <QObject>

#include <optional>

class AppWorkspaceHost;

class AppScheduleOptimizer : public QObject
{
    Q_OBJECT

public:
    static AppScheduleOptimizer* GetInstance();

    explicit AppScheduleOptimizer(AppWorkspaceHost& Host, QObject* Parent = nullptr);
    ~AppScheduleOptimizer() override;

    void Start();
    void OnApplicationStateChanged(Qt::ApplicationState State);

signals:
    void weightsChanged();

private:
    void OnWorkspaceOpened();
    void Finish(const std::optional<FTScheduleWeights>& Weights, int ReviewCount, quint64 Serial);

    quint64 Serial = 0;
    bool bRunning = false;
    std::optional<FTDayNumber> CheckedDay;
};
