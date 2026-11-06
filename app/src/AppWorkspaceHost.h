#pragma once

#include "AppAccountRegistry.h"
#include "AppWorkspaceMerger.h"
#include "AppWorkspaceSync.h"

#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

class AppCloudStore;
class AppCredentialStore;
class AppWorkspace;

class AppWorkspaceHost : public QObject
{
    Q_OBJECT

public:
    AppWorkspaceHost(const QString& DataRoot, AppCredentialStore& Credentials, QObject* Parent = nullptr);
    ~AppWorkspaceHost() override;

    void SetCloudStore(AppCloudStore* Store, const AppSyncTimings& Timings);

    bool OpenActive();
    void Close();

    bool SignIn(const AppAccount& Account, const QString& Credential);

    std::optional<AppIncomingProblem> ImportDatabase(const QString& SourcePath);

    void SetSwitchGate(std::function<void(std::function<void()>)> Gate);
    void SignOut();

    AppWorkspace* GetActive() const;
    AppWorkspaceSync* GetSync() const;
    AppAccountRegistry& GetRegistry();

signals:
    void workspaceOpened();
    void aboutToSwitch();
    void switched();

private:
    void SwitchToActive();
    void CompleteSwitch();

    AppAccountRegistry Registry;
    AppCloudStore* CloudStore = nullptr;
    AppSyncTimings SyncTimings;
    std::unique_ptr<AppWorkspace> Active;
    std::unique_ptr<AppWorkspaceMerger> Merger;
    std::unique_ptr<AppWorkspaceSync> Sync;
    std::function<void(std::function<void()>)> SwitchGate;
    bool bSwitchPending = false;
};
