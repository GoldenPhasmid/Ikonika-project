#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class AppAccountRegistry;
class AppCloudStore;
class AppSyncMerger;
class AppWorkspace;
struct AppCloudCopy;
struct AppCloudFailure;

struct AppSyncTimings
{
    int QuietPauseMilliseconds = 6000;
    int ForegroundCooldownMilliseconds = 300000;
    int RetryBaseMilliseconds = 30000;
    int MaximumRetries = 5;
    int MaximumPulls = 3;
    std::function<qint64()> Clock;
};

class AppWorkspaceSync : public QObject
{
    Q_OBJECT

public:
    AppWorkspaceSync(AppWorkspace& Workspace, AppAccountRegistry& Registry, AppCloudStore& Store,
                     AppSyncMerger& Merger, const AppSyncTimings& Timings,
                     QObject* Parent = nullptr);
    ~AppWorkspaceSync() override;

    void SyncNow();
    void Stop();
    void HoldMerges(bool bHold);

    void OnBackground();
    void OnForeground();
    void OnNetworkBack();

    bool IsRunning() const;
    bool IsRestoring() const;
    bool HasUnsentWrites() const;

signals:
    void cycleStarted();
    void cycleFinished(bool bSuccess);
    void restoringChanged();
    void dataMerged();

private:
    struct FCycle
    {
        QString CopyId;
        QString PulledVersion;
        int PullAttempts = 0;
        int SnapshotWriteCount = 0;
        std::vector<QString> ExtraIds;
        size_t NextExtra = 0;
        std::vector<QString> DeleteIds;
    };

    struct FPullOutcome
    {
        bool bMerged = false;
        bool bReplaceCopy = false;
        bool bRetry = false;
    };

    using FDownloadHandler = void (AppWorkspaceSync::*)(const std::optional<AppCloudFailure>&);

    bool CanSync() const;
    bool HoldDownloadedCopy(FDownloadHandler Resume);
    void StartCycle();
    void OnFound(const std::optional<AppCloudFailure>& Failure,
                 const std::vector<AppCloudCopy>& Copies);
    void Pull(const QString& Version);
    void OnDownloaded(const std::optional<AppCloudFailure>& Failure);
    void MergeNextExtra();
    void OnExtraDownloaded(const std::optional<AppCloudFailure>& Failure);
    void DeleteNextExtra();
    void OnExtraDeleted(const std::optional<AppCloudFailure>& Failure);
    void OnVersionFetched(const std::optional<AppCloudFailure>& Failure, const QString& Version);
    void UploadSnapshot(const QString& CopyId);
    void OnUploaded(const std::optional<AppCloudFailure>& Failure, const AppCloudCopy& Copy);

    FPullOutcome MergeDownloadedCopy();
    QString CreateSnapshotArchive();
    QString GetSyncDirectory() const;
    QString GetUnsentMarkerPath() const;
    void UpdateUnsentMarker();

    void Fail(const AppCloudFailure& Failure);
    void Finish(bool bSuccess);
    void ScheduleRetry();
    void OnLocalWrite();
    qint64 Now() const;

    template <typename FCallback>
    auto Guard(FCallback Callback);

    AppWorkspace* Workspace = nullptr;
    AppAccountRegistry* Registry = nullptr;
    AppCloudStore* Store = nullptr;
    AppSyncMerger* Merger = nullptr;
    AppSyncTimings Timings;

    std::shared_ptr<FCycle> Cycle;
    std::function<void()> HeldMerge;
    QTimer QuietTimer;
    QTimer RetryTimer;
    qint64 LastCycleStart = 0;

    int WriteCount = 0;
    int SentWriteCount = 0;
    int RetryAttempt = 0;
    bool bIgnoreWrites = false;
    bool bUnsentMarked = false;
    bool bAnotherCycleRequested = false;
    bool bLastCycleFailedToConnect = false;
    bool bRestoring = false;
    bool bMergesHeld = false;
};
