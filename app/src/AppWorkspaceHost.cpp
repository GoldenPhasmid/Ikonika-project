#include "AppWorkspaceHost.h"

#include "AppFiles.h"
#include "AppWorkspace.h"
#include "AppWorkspaceMerger.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QDir>
#include <QSaveFile>
#include <QScopeGuard>

AppWorkspaceHost::AppWorkspaceHost(const QString& DataRoot, AppCredentialStore& Credentials, QObject* Parent)
    : QObject(Parent)
    , Registry{DataRoot, Credentials}
{
}

AppWorkspaceHost::~AppWorkspaceHost()
{
    Close();
}

void AppWorkspaceHost::SetCloudStore(AppCloudStore* Store, const AppSyncTimings& Timings)
{
    CloudStore = Store;
    SyncTimings = Timings;
}

bool AppWorkspaceHost::OpenActive()
{
    Close();

    Active = AppWorkspace::Open(Registry.GetActiveFolder());

    if (Active == nullptr)
    {
        qCCritical(LogApp) << "Could not open the active workspace"
                           << Registry.GetActiveFolder().Directory;

        return false;
    }

    FTSettings::SetActive(&Active->GetSettings());

    if (CloudStore != nullptr && Registry.GetActiveAccount().has_value())
    {
        Merger = std::make_unique<AppWorkspaceMerger>(Active->GetSettings());
        Sync = std::make_unique<AppWorkspaceSync>(*Active, Registry, *CloudStore, *Merger, SyncTimings);
    }

    emit workspaceOpened();

    if (Sync != nullptr)
    {
        Sync->SyncNow();
    }

    return true;
}

void AppWorkspaceHost::Close()
{
    if (Active == nullptr)
    {
        return;
    }

    Sync.reset();
    Merger.reset();

    FTSettings::SetActive(nullptr);
    Active.reset();
}

bool AppWorkspaceHost::SignIn(const AppAccount& Account, const QString& Credential)
{
    const std::optional<AppAccount> Previous{Registry.GetActiveAccount()};

    if (!Registry.SignIn(Account, Credential))
    {
        return false;
    }

    if (Active != nullptr && Previous.has_value() && Previous->Id == Account.Id)
    {
        qCInfo(LogApp) << "Reconnected" << Account.Email << ", its workspace stays open";

        if (Sync != nullptr)
        {
            Sync->SyncNow();
        }

        return true;
    }

    SwitchToActive();

    return true;
}

void AppWorkspaceHost::SignOut()
{
    if (!Registry.GetActiveAccount().has_value())
    {
        return;
    }

    Registry.SignOut();

    SwitchToActive();
}

void AppWorkspaceHost::SetSwitchGate(std::function<void(std::function<void()>)> Gate)
{
    SwitchGate = std::move(Gate);
}

void AppWorkspaceHost::SwitchToActive()
{
    if (!SwitchGate)
    {
        CompleteSwitch();

        return;
    }

    if (bSwitchPending)
    {
        return;
    }

    bSwitchPending = true;

    Sync.reset();
    Merger.reset();

    qCInfo(LogApp) << "The workspace switch waits for the screens to be covered";

    SwitchGate([this]
               {
                   bSwitchPending = false;
                   CompleteSwitch();
               });
}

void AppWorkspaceHost::CompleteSwitch()
{
    emit aboutToSwitch();

    if (!OpenActive())
    {
        qCCritical(LogApp) << "The workspace switch left no workspace open";
    }

    emit switched();
}

AppWorkspace* AppWorkspaceHost::GetActive() const
{
    return Active.get();
}

AppWorkspaceSync* AppWorkspaceHost::GetSync() const
{
    return Sync.get();
}

AppAccountRegistry& AppWorkspaceHost::GetRegistry()
{
    return Registry;
}

std::optional<AppIncomingProblem> AppWorkspaceHost::ImportDatabase(const QString& SourcePath)
{
    if (Active == nullptr)
    {
        return AppIncomingProblem{.Reason = QStringLiteral("no workspace is open")};
    }

    if (Sync != nullptr && Sync->IsRunning())
    {
        return AppIncomingProblem{.Reason = QStringLiteral("a sync is running"), .bBusy = true};
    }

    const std::optional<QByteArray> Contents{ReadWholeFile(SourcePath)};

    if (!Contents.has_value())
    {
        qCWarning(LogSync) << "Could not read" << SourcePath << "for import";

        return AppIncomingProblem{.Reason = QStringLiteral("the file could not be read")};
    }

    const QString StagedPath{QDir(Active->GetFolder().Directory).filePath(QStringLiteral("import.db"))};
    RemoveDatabaseFiles(StagedPath);

    const auto RemoveStaged = qScopeGuard([&StagedPath] { RemoveDatabaseFiles(StagedPath); });

    QSaveFile Staged(StagedPath);

    if (!Staged.open(QIODevice::WriteOnly) || Staged.write(*Contents) != Contents->size() || !Staged.commit())
    {
        qCWarning(LogSync) << "Could not stage" << SourcePath << "at" << StagedPath;

        return AppIncomingProblem{.Reason = QStringLiteral("the file could not be staged")};
    }

    std::optional<FTDatabase> Incoming{FTDatabase::Open(StagedPath.toStdString())};

    if (!Incoming.has_value())
    {
        return AppIncomingProblem{.Reason = QStringLiteral("it could not be opened")};
    }

    if (std::optional<AppIncomingProblem> Problem{PrepareIncomingDatabase(*Incoming)})
    {
        qCWarning(LogSync) << "Import refused:" << Problem->Reason;

        return Problem;
    }

    AppWorkspaceMerger ImportMerger{Active->GetSettings()};
    const AppMergeResult Result{ImportMerger.Merge(Active->GetConnection(), *Incoming)};

    if (!Result.bSuccess)
    {
        qCWarning(LogSync) << "Import merge failed:" << Result.Error;

        return AppIncomingProblem{.Reason = Result.Error};
    }

    qCInfo(LogSync) << "Imported" << SourcePath << "into" << Active->GetFolder().Directory;

    if (Sync != nullptr)
    {
        Sync->SyncNow();
    }

    return std::nullopt;
}
