#pragma once

#include <QString>

class FTDatabase;

struct AppMergeResult
{
    bool bSuccess = false;
    QString Error;
};

class AppSyncMerger
{
public:
    virtual ~AppSyncMerger() = default;

    virtual AppMergeResult Merge(FTDatabase& Local, FTDatabase& Downloaded) = 0;
};
