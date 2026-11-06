#include "AppDatabaseService.h"

#include "AppWorkspace.h"
#include "AppWorkspaceHost.h"

#include "core/FTLog.h"

AppDatabaseService::AppDatabaseService(AppWorkspaceHost& Host)
    : Host{&Host}
{
}

bool AppDatabaseService::Initialise()
{
    if (Host == nullptr)
    {
        LastError = QStringLiteral("no workspace host is installed");
        qCCritical(LogDatabase) << LastError;

        return false;
    }

    if (Host->GetActive() != nullptr)
    {
        return true;
    }

    if (!Host->OpenActive())
    {
        LastError = QStringLiteral("could not open the active workspace %1")
                        .arg(Host->GetRegistry().GetActiveFolder().Directory);

        return false;
    }

    LastError.clear();

    return true;
}

bool AppDatabaseService::IsReady()
{
    if (Host != nullptr && Host->GetActive() != nullptr)
    {
        return true;
    }

    return Initialise();
}

FTCardRepository& AppDatabaseService::GetCardRepository()
{
    return GetWorkspace().GetCardRepository();
}

FTReaderRepository& AppDatabaseService::GetReaderLibrary()
{
    return GetWorkspace().GetReaderLibrary();
}

FTCloudBookRepository& AppDatabaseService::GetCloudBooks()
{
    return GetWorkspace().GetCloudBooks();
}

FTYoutubeRepository& AppDatabaseService::GetYoutubeLibrary()
{
    return GetWorkspace().GetYoutubeLibrary();
}

void AppDatabaseService::ApplyActiveLanguages()
{
    if (Host == nullptr || Host->GetActive() == nullptr)
    {
        return;
    }

    Host->GetActive()->ApplyActiveLanguages();
}

FTDatabase& AppDatabaseService::GetConnection()
{
    return GetWorkspace().GetConnection();
}

QString AppDatabaseService::GetFilePath() const
{
    if (Host == nullptr || Host->GetActive() == nullptr)
    {
        return {};
    }

    return Host->GetActive()->GetFolder().GetDatabaseFilePath();
}

QString AppDatabaseService::GetLastError() const
{
    return LastError;
}

AppWorkspace& AppDatabaseService::GetWorkspace() const
{
    return *Host->GetActive();
}
