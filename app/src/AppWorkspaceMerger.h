#pragma once

#include "AppSyncMerger.h"

#include <QString>

#include <optional>

class FTDatabase;
class FTSettings;

struct AppIncomingProblem
{
    QString Reason;
    bool bFromNewerApp = false;
    bool bRetryLater = false;
    bool bBusy = false;
};

std::optional<AppIncomingProblem> PrepareIncomingDatabase(FTDatabase& Incoming);

class AppWorkspaceMerger : public AppSyncMerger
{
public:
    explicit AppWorkspaceMerger(FTSettings& Settings);

    AppMergeResult Merge(FTDatabase& Local, FTDatabase& Downloaded) override;

private:
    FTSettings* Settings = nullptr;
};
