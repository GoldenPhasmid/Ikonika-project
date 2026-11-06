#include "AppWorkspaceSync.h"

#include "AppAccountRegistry.h"
#include "AppCloudStore.h"
#include "AppSyncMerger.h"
#include "AppFiles.h"
#include "AppWorkspace.h"
#include "AppWorkspaceMerger.h"

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"

#include <QDir>
#include <QFile>
#include <QRandomGenerator>

#include "miniz.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <utility>

namespace
{
    const char* DatabaseEntryName = "Ikonika.db";
    const QByteArray SqliteHeader{"SQLite format 3", 16};

    bool WriteFile(const QString& Path, const QByteArray& Contents)
    {
        QFile File(Path);

        return File.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && File.write(Contents) == Contents.size();
    }

    std::optional<QByteArray> PackArchive(const QByteArray& Database)
    {
        mz_zip_archive Zip;
        std::memset(&Zip, 0, sizeof(Zip));

        if (!mz_zip_writer_init_heap(&Zip, 0, 0))
        {
            return std::nullopt;
        }

        void* Buffer{nullptr};
        size_t Size{0};

        const bool bPacked{
            mz_zip_writer_add_mem(&Zip, DatabaseEntryName, Database.constData(),
                                  static_cast<size_t>(Database.size()),
                                  static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION))
            && mz_zip_writer_finalize_heap_archive(&Zip, &Buffer, &Size)};

        std::optional<QByteArray> Archive;

        if (bPacked)
        {
            Archive = QByteArray(static_cast<const char*>(Buffer), static_cast<qsizetype>(Size));
        }

        mz_zip_writer_end(&Zip);

        return Archive;
    }

    std::optional<QByteArray> UnpackDatabase(const QByteArray& Archive)
    {
        mz_zip_archive Zip;
        std::memset(&Zip, 0, sizeof(Zip));

        if (!mz_zip_reader_init_mem(&Zip, Archive.constData(), static_cast<size_t>(Archive.size()), 0))
        {
            return std::nullopt;
        }

        size_t Size{0};
        void* Buffer{mz_zip_reader_extract_file_to_heap(&Zip, DatabaseEntryName, &Size, 0)};

        std::optional<QByteArray> Database;

        if (Buffer != nullptr)
        {
            Database = QByteArray(static_cast<const char*>(Buffer), static_cast<qsizetype>(Size));
            mz_free(Buffer);
        }

        mz_zip_reader_end(&Zip);

        return Database;
    }

    bool IsArchive(const QByteArray& Bytes)
    {
        mz_zip_archive Zip;
        std::memset(&Zip, 0, sizeof(Zip));

        if (!mz_zip_reader_init_mem(&Zip, Bytes.constData(), static_cast<size_t>(Bytes.size()), 0))
        {
            return false;
        }

        mz_zip_reader_end(&Zip);

        return true;
    }

}

template <typename FCallback>
auto AppWorkspaceSync::Guard(FCallback Callback)
{
    return [this, Weak = std::weak_ptr<FCycle>(Cycle), Callback](auto&&... Arguments)
    {
        if (Weak.expired())
        {
            return;
        }

        (this->*Callback)(std::forward<decltype(Arguments)>(Arguments)...);
    };
}

AppWorkspaceSync::AppWorkspaceSync(AppWorkspace& Workspace, AppAccountRegistry& Registry,
                                   AppCloudStore& Store, AppSyncMerger& Merger,
                                   const AppSyncTimings& Timings, QObject* Parent)
    : QObject(Parent)
    , Workspace{&Workspace}
    , Registry{&Registry}
    , Store{&Store}
    , Merger{&Merger}
    , Timings{Timings}
{
    QuietTimer.setParent(this);
    QuietTimer.setObjectName(QStringLiteral("quietTimer"));
    RetryTimer.setParent(this);
    RetryTimer.setObjectName(QStringLiteral("retryTimer"));

    QuietTimer.setSingleShot(true);
    QuietTimer.setInterval(Timings.QuietPauseMilliseconds);
    connect(&QuietTimer, &QTimer::timeout, this, &AppWorkspaceSync::StartCycle);

    RetryTimer.setSingleShot(true);
    connect(&RetryTimer, &QTimer::timeout, this, &AppWorkspaceSync::StartCycle);

    LastCycleStart = Now();

    bRestoring = CanSync() && Workspace.GetCardRepository().CountCards() == 0;

    Workspace.GetConnection().SetCommitListener([this] { OnLocalWrite(); });

    if (CanSync() && QFile::exists(GetUnsentMarkerPath()))
    {
        qCInfo(LogSync) << "Writes from the last run were never sent, syncing after the quiet pause";

        WriteCount = 1;
        bUnsentMarked = true;
        QuietTimer.start();
    }
}

AppWorkspaceSync::~AppWorkspaceSync()
{
    Stop();

    Workspace->GetConnection().SetCommitListener({});
}

void AppWorkspaceSync::SyncNow()
{
    if (!CanSync())
    {
        return;
    }

    RetryAttempt = 0;

    StartCycle();
}

void AppWorkspaceSync::Stop()
{
    if (Cycle != nullptr)
    {
        qCInfo(LogSync) << "Sync cycle abandoned";
    }

    Cycle.reset();
    HeldMerge = nullptr;
    bAnotherCycleRequested = false;
    QuietTimer.stop();
    RetryTimer.stop();

    Store->CancelAll();
}

void AppWorkspaceSync::HoldMerges(bool bHold)
{
    if (bMergesHeld == bHold)
    {
        return;
    }

    bMergesHeld = bHold;

    if (bMergesHeld || !HeldMerge)
    {
        return;
    }

    qCInfo(LogSync) << "Merging the held cloud copy";

    QTimer::singleShot(0, this, std::exchange(HeldMerge, nullptr));
}

void AppWorkspaceSync::OnBackground()
{
    if (CanSync() && HasUnsentWrites())
    {
        StartCycle();
    }
}

void AppWorkspaceSync::OnForeground()
{
    if (!CanSync())
    {
        return;
    }

    if (!HasUnsentWrites() && Now() - LastCycleStart < Timings.ForegroundCooldownMilliseconds)
    {
        return;
    }

    StartCycle();
}

void AppWorkspaceSync::OnNetworkBack()
{
    if (!CanSync() || (!bLastCycleFailedToConnect && !HasUnsentWrites()))
    {
        return;
    }

    qCInfo(LogSync) << "The network is back, syncing";

    RetryAttempt = 0;

    StartCycle();
}

bool AppWorkspaceSync::IsRunning() const
{
    return Cycle != nullptr;
}

bool AppWorkspaceSync::IsRestoring() const
{
    return bRestoring;
}

bool AppWorkspaceSync::HasUnsentWrites() const
{
    return WriteCount != SentWriteCount;
}

bool AppWorkspaceSync::CanSync() const
{
    const std::optional<AppAccount> Account{Registry->GetActiveAccount()};

    return Account.has_value() && !Account->bNeedsReconnect;
}

bool AppWorkspaceSync::HoldDownloadedCopy(FDownloadHandler Resume)
{
    if (!bMergesHeld)
    {
        return false;
    }

    qCInfo(LogSync) << "Holding the downloaded cloud copy until merges are allowed";

    HeldMerge = [Continue = Guard(Resume)] { Continue(std::optional<AppCloudFailure>{}); };

    return true;
}

void AppWorkspaceSync::StartCycle()
{
    if (!CanSync())
    {
        return;
    }

    if (Cycle != nullptr)
    {
        bAnotherCycleRequested = true;

        return;
    }

    QuietTimer.stop();
    RetryTimer.stop();
    LastCycleStart = Now();

    Cycle = std::make_shared<FCycle>();

    qCInfo(LogSync) << "Sync cycle started";

    emit cycleStarted();

    Store->FindCopies(Guard(&AppWorkspaceSync::OnFound));
}

void AppWorkspaceSync::OnFound(const std::optional<AppCloudFailure>& Failure,
                               const std::vector<AppCloudCopy>& Copies)
{
    if (Failure.has_value())
    {
        Fail(*Failure);

        return;
    }

    if (Copies.empty())
    {
        qCInfo(LogSync) << "No cloud copy yet, uploading this workspace";

        UploadSnapshot(QString());

        return;
    }

    Cycle->CopyId = Copies.front().Id;

    for (size_t Index = 1; Index < Copies.size(); ++Index)
    {
        Cycle->ExtraIds.push_back(Copies[Index].Id);
    }

    if (!Cycle->ExtraIds.empty())
    {
        qCWarning(LogSync) << "Found" << static_cast<int>(Cycle->ExtraIds.size())
                           << "extra cloud copies, merging them into" << Cycle->CopyId;
    }

    Pull(Copies.front().Version);
}

void AppWorkspaceSync::Pull(const QString& Version)
{
    Cycle->PulledVersion = Version;
    ++Cycle->PullAttempts;

    qCInfo(LogSync) << "Pulling the cloud copy" << Cycle->CopyId << "version" << Version;

    Store->Download(Cycle->CopyId, GetSyncDirectory() + QStringLiteral("/download.zip"),
                    Guard(&AppWorkspaceSync::OnDownloaded));
}

void AppWorkspaceSync::OnDownloaded(const std::optional<AppCloudFailure>& Failure)
{
    if (Failure.has_value())
    {
        Fail(*Failure);

        return;
    }

    if (HoldDownloadedCopy(&AppWorkspaceSync::OnDownloaded))
    {
        return;
    }

    const FPullOutcome Outcome{MergeDownloadedCopy()};

    if (Outcome.bReplaceCopy)
    {
        qCWarning(LogSync) << "Replacing the unreadable cloud copy with this workspace";

        MergeNextExtra();

        return;
    }

    if (!Outcome.bMerged)
    {
        Finish(false);

        if (Outcome.bRetry)
        {
            ScheduleRetry();
        }

        return;
    }

    emit dataMerged();

    MergeNextExtra();
}

void AppWorkspaceSync::MergeNextExtra()
{
    if (Cycle->NextExtra >= Cycle->ExtraIds.size())
    {
        Store->FetchVersion(Cycle->CopyId, Guard(&AppWorkspaceSync::OnVersionFetched));

        return;
    }

    const QString& ExtraId{Cycle->ExtraIds[Cycle->NextExtra]};

    qCInfo(LogSync) << "Pulling the extra cloud copy" << ExtraId;

    Store->Download(ExtraId, GetSyncDirectory() + QStringLiteral("/download.zip"),
                    Guard(&AppWorkspaceSync::OnExtraDownloaded));
}

void AppWorkspaceSync::OnExtraDownloaded(const std::optional<AppCloudFailure>& Failure)
{
    if (Failure.has_value())
    {
        Fail(*Failure);

        return;
    }

    if (HoldDownloadedCopy(&AppWorkspaceSync::OnExtraDownloaded))
    {
        return;
    }

    const QString ExtraId{Cycle->ExtraIds[Cycle->NextExtra]};
    const FPullOutcome Outcome{MergeDownloadedCopy()};

    if (Outcome.bRetry)
    {
        Finish(false);
        ScheduleRetry();

        return;
    }

    if (Outcome.bMerged || Outcome.bReplaceCopy)
    {
        Cycle->DeleteIds.push_back(ExtraId);
    }

    if (Outcome.bMerged)
    {
        emit dataMerged();
    }

    ++Cycle->NextExtra;

    MergeNextExtra();
}

void AppWorkspaceSync::DeleteNextExtra()
{
    if (Cycle->DeleteIds.empty())
    {
        Finish(true);

        return;
    }

    const QString ExtraId{Cycle->DeleteIds.back()};
    Cycle->DeleteIds.pop_back();

    qCInfo(LogSync) << "Deleting the extra cloud copy" << ExtraId;

    Store->Delete(ExtraId, Guard(&AppWorkspaceSync::OnExtraDeleted));
}

void AppWorkspaceSync::OnExtraDeleted(const std::optional<AppCloudFailure>& Failure)
{
    if (Failure.has_value())
    {
        qCWarning(LogSync) << "Could not delete an extra cloud copy:" << Failure->Message;
    }

    DeleteNextExtra();
}

void AppWorkspaceSync::OnVersionFetched(const std::optional<AppCloudFailure>& Failure,
                                        const QString& Version)
{
    if (Failure.has_value())
    {
        Fail(*Failure);

        return;
    }

    if (Version == Cycle->PulledVersion)
    {
        UploadSnapshot(Cycle->CopyId);

        return;
    }

    if (Cycle->PullAttempts >= Timings.MaximumPulls)
    {
        qCWarning(LogSync) << "The cloud copy kept changing during the sync";

        Finish(false);
        ScheduleRetry();

        return;
    }

    qCInfo(LogSync) << "The cloud copy moved to version" << Version << "during the merge";

    Pull(Version);
}

void AppWorkspaceSync::UploadSnapshot(const QString& CopyId)
{
    const QString ArchivePath{CreateSnapshotArchive()};

    if (ArchivePath.isEmpty())
    {
        Finish(false);

        return;
    }

    Cycle->SnapshotWriteCount = WriteCount;

    Store->Upload(CopyId, ArchivePath, Guard(&AppWorkspaceSync::OnUploaded));
}

void AppWorkspaceSync::OnUploaded(const std::optional<AppCloudFailure>& Failure,
                                  const AppCloudCopy& Copy)
{
    if (Failure.has_value())
    {
        Fail(*Failure);

        return;
    }

    qCInfo(LogSync) << "Uploaded the workspace as" << Copy.Id << "version" << Copy.Version;

    SentWriteCount = Cycle->SnapshotWriteCount;
    RetryAttempt = 0;

    UpdateUnsentMarker();

    DeleteNextExtra();
}

AppWorkspaceSync::FPullOutcome AppWorkspaceSync::MergeDownloadedCopy()
{
    const std::optional<QByteArray> Read{ReadWholeFile(GetSyncDirectory() + QStringLiteral("/download.zip"))};

    if (!Read.has_value())
    {
        qCWarning(LogSync) << "Could not read the downloaded cloud copy";

        return FPullOutcome{.bRetry = true};
    }

    const QByteArray& Downloaded{*Read};

    std::optional<QByteArray> Database;

    if (Downloaded.startsWith(SqliteHeader))
    {
        qCInfo(LogSync) << "The cloud copy is a plain database left by the previous sync";

        Database = Downloaded;
    }
    else if (!IsArchive(Downloaded))
    {
        qCWarning(LogSync) << "The cloud copy is neither an archive nor a database";

        return FPullOutcome{.bReplaceCopy = true};
    }
    else
    {
        Database = UnpackDatabase(Downloaded);

        if (!Database.has_value())
        {
            qCWarning(LogSync) << "The cloud copy is an archive this build does not know, left alone";

            return FPullOutcome{};
        }
    }

    if (!Database->startsWith(SqliteHeader))
    {
        qCWarning(LogSync) << "The cloud copy's archive holds no database";

        return FPullOutcome{.bReplaceCopy = true};
    }

    const QString RemotePath{GetSyncDirectory() + QStringLiteral("/remote.db")};
    RemoveDatabaseFiles(RemotePath);

    if (!WriteFile(RemotePath, *Database))
    {
        qCWarning(LogSync) << "Could not stage the cloud copy at" << RemotePath;

        return FPullOutcome{.bRetry = true};
    }

    std::optional<FTDatabase> Remote{FTDatabase::Open(RemotePath.toStdString())};

    if (!Remote.has_value())
    {
        qCWarning(LogSync) << "Could not open the staged cloud copy";

        return FPullOutcome{.bRetry = true};
    }

    if (const std::optional<AppIncomingProblem> Problem{PrepareIncomingDatabase(*Remote)})
    {
        qCWarning(LogSync) << "The cloud copy cannot be merged:" << Problem->Reason;

        if (Problem->bFromNewerApp)
        {
            return FPullOutcome{};
        }

        return Problem->bRetryLater ? FPullOutcome{.bRetry = true} : FPullOutcome{.bReplaceCopy = true};
    }

    bIgnoreWrites = true;
    const AppMergeResult Result{Merger->Merge(Workspace->GetConnection(), *Remote)};
    bIgnoreWrites = false;

    if (!Result.bSuccess)
    {
        qCWarning(LogSync) << "The merge failed:" << Result.Error;

        return FPullOutcome{.bRetry = true};
    }

    qCInfo(LogSync) << "Merged the cloud copy into the workspace";

    return FPullOutcome{.bMerged = true};
}

QString AppWorkspaceSync::CreateSnapshotArchive()
{
    const QString SnapshotPath{GetSyncDirectory() + QStringLiteral("/snapshot.db")};
    const QString ArchivePath{GetSyncDirectory() + QStringLiteral("/upload.zip")};

    RemoveDatabaseFiles(SnapshotPath);

    QString Escaped{SnapshotPath};
    Escaped.replace(QLatin1Char('\''), QStringLiteral("''"));

    bIgnoreWrites = true;
    const bool bSnapshotTaken{
        Workspace->GetConnection().Execute("VACUUM INTO '" + Escaped.toStdString() + "'")};
    bIgnoreWrites = false;

    if (!bSnapshotTaken)
    {
        qCWarning(LogSync) << "Could not snapshot the workspace database";

        return {};
    }

    const std::optional<QByteArray> Snapshot{ReadWholeFile(SnapshotPath)};
    RemoveDatabaseFiles(SnapshotPath);

    const std::optional<QByteArray> Archive{Snapshot.has_value() ? PackArchive(*Snapshot) : std::nullopt};

    if (!Archive.has_value() || !WriteFile(ArchivePath, *Archive))
    {
        qCWarning(LogSync) << "Could not pack the workspace snapshot into" << ArchivePath;

        return {};
    }

    return ArchivePath;
}

QString AppWorkspaceSync::GetSyncDirectory() const
{
    const QString Directory{QDir(Workspace->GetFolder().Directory).filePath(QStringLiteral("sync"))};
    QDir().mkpath(Directory);

    return Directory;
}

QString AppWorkspaceSync::GetUnsentMarkerPath() const
{
    return GetSyncDirectory() + QStringLiteral("/unsent");
}

void AppWorkspaceSync::UpdateUnsentMarker()
{
    if (HasUnsentWrites() == bUnsentMarked)
    {
        return;
    }

    if (HasUnsentWrites())
    {
        QFile Marker(GetUnsentMarkerPath());
        bUnsentMarked = Marker.open(QIODevice::WriteOnly);

        if (!bUnsentMarked)
        {
            qCWarning(LogSync) << "Could not mark unsent writes at" << Marker.fileName();
        }

        return;
    }

    bUnsentMarked = !QFile::remove(GetUnsentMarkerPath());
}

void AppWorkspaceSync::Fail(const AppCloudFailure& Failure)
{
    if (Failure.bCredentialRefused)
    {
        const std::optional<AppAccount> Account{Registry->GetActiveAccount()};

        if (Account.has_value())
        {
            Registry->MarkNeedsReconnect(Account->Id);
        }

        Finish(false);

        return;
    }

    qCWarning(LogSync) << "Sync cycle failed:" << Failure.Message;

    bLastCycleFailedToConnect = true;

    Finish(false);
    ScheduleRetry();
}

void AppWorkspaceSync::Finish(bool bSuccess)
{
    Cycle.reset();

    if (bSuccess)
    {
        bLastCycleFailedToConnect = false;
    }

    qCInfo(LogSync) << "Sync cycle finished, success =" << bSuccess;

    if (bRestoring)
    {
        bRestoring = false;
        emit restoringChanged();
    }

    emit cycleFinished(bSuccess);

    if (bAnotherCycleRequested)
    {
        bAnotherCycleRequested = false;

        QTimer::singleShot(0, this, &AppWorkspaceSync::StartCycle);
    }
}

void AppWorkspaceSync::ScheduleRetry()
{
    if (RetryAttempt >= Timings.MaximumRetries)
    {
        qCWarning(LogSync) << "Sync failed" << RetryAttempt << "retries in a row, waiting for the next trigger";

        return;
    }

    const int Delay{Timings.RetryBaseMilliseconds << RetryAttempt};
    const int Jitter{QRandomGenerator::global()->bounded(std::max(1, Delay / 4))};

    ++RetryAttempt;

    qCInfo(LogSync) << "Sync retry" << RetryAttempt << "in" << Delay + Jitter << "ms";

    RetryTimer.start(Delay + Jitter);
}

qint64 AppWorkspaceSync::Now() const
{
    if (Timings.Clock)
    {
        return Timings.Clock();
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void AppWorkspaceSync::OnLocalWrite()
{
    if (bIgnoreWrites)
    {
        return;
    }

    ++WriteCount;

    if (CanSync())
    {
        UpdateUnsentMarker();
        QuietTimer.start();
    }
}
