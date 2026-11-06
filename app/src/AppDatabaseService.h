#pragma once

#include "AppDatabase.h"

class AppWorkspace;
class AppWorkspaceHost;

class AppDatabaseService : public AppDatabase
{
public:
    AppDatabaseService() = default;
    explicit AppDatabaseService(AppWorkspaceHost& Host);

    bool Initialise() override;

    bool IsReady() override;

    FTCardRepository& GetCardRepository() override;

    FTReaderRepository& GetReaderLibrary() override;

    FTCloudBookRepository& GetCloudBooks() override;

    FTYoutubeRepository& GetYoutubeLibrary() override;

    void ApplyActiveLanguages() override;

    FTDatabase& GetConnection() override;

    QString GetFilePath() const override;
    QString GetLastError() const override;

private:
    AppWorkspace& GetWorkspace() const;

    AppWorkspaceHost* Host = nullptr;
    QString LastError;
};
