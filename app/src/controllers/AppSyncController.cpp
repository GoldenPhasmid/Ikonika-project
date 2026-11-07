#include "AppSyncController.h"

#include "AppAccountRegistry.h"
#include "AppDatabase.h"
#include "AppGoogleAccount.h"
#include "AppLocalization.h"
#include "AppSessionCheckpoints.h"
#include "AppState.h"
#include "AppWorkspaceHost.h"
#include "AppWorkspaceSync.h"

#include "core/FTLog.h"

#include <QFile>
#include <QFileInfo>

#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>
#elif defined(Q_OS_IOS)
#include "AppIosBackgroundTask.h"
#include "AppIosShare.h"
#endif

namespace
{
    AppSyncController* Instance = nullptr;
}

AppSyncController* AppSyncController::GetInstance()
{
    return Instance;
}

AppSyncController::AppSyncController(AppWorkspaceHost& Host, AppGoogleAccount& Account, QObject* Parent)
    : QObject(Parent)
    , Host{&Host}
    , Account{&Account}
{
    Instance = this;

    connect(&Account, &AppGoogleAccount::busyChanged, this, [this]
            {
                emit busyChanged();
                emit statusChanged();
            });
    connect(&Account, &AppGoogleAccount::errorChanged, this, &AppSyncController::statusChanged);
    connect(&AppLocalization::Get(), &AppLocalization::languageChanged, this,
            &AppSyncController::statusChanged);
    connect(&Account, &AppGoogleAccount::signedIn, this, &AppSyncController::OnSignedIn);
    connect(&Host, &AppWorkspaceHost::workspaceOpened, this, &AppSyncController::OnWorkspaceOpened);

    OnWorkspaceOpened();
}

AppSyncController::~AppSyncController()
{
    if (Instance == this)
    {
        Instance = nullptr;
    }
}

void AppSyncController::signIn()
{
    Account->SignIn(NeedsReconnect() ? GetAccountEmail() : QString());
}

void AppSyncController::switchAccount()
{
    qCInfo(LogSync) << "The user asked to switch account from" << GetAccountEmail();

    Account->SignIn();
}

void AppSyncController::signOut()
{
    QMetaObject::invokeMethod(this, [this]
                              {
                                  const std::optional<AppAccount> Active{Host->GetRegistry().GetActiveAccount()};

                                  if (!Active.has_value())
                                  {
                                      return;
                                  }

                                  Account->ForgetToken();
                                  Host->SignOut();

                                  emit signedInChanged();
                                  emit accountChanged();
                                  emit statusChanged();
                              }, Qt::QueuedConnection);
}

void AppSyncController::syncNow()
{
    qCInfo(LogSync) << "The user asked to sync now";

    if (AppWorkspaceSync* Sync{Host->GetSync()})
    {
        Sync->SyncNow();
    }
}

void AppSyncController::OnSignedIn(const AppAccount& SignedIn, const QString& Credential)
{
    QMetaObject::invokeMethod(this, [this, SignedIn, Credential]
                              {
                                  if (!Host->SignIn(SignedIn, Credential))
                                  {
                                      qCWarning(LogSync) << "The registry refused the sign-in of" << SignedIn.Email;
                                  }

                                  emit signedInChanged();
                                  emit accountChanged();
                                  emit statusChanged();
                              }, Qt::QueuedConnection);
}

void AppSyncController::OnWorkspaceOpened()
{
    disconnect(CycleStarted);
    disconnect(CycleFinished);
    disconnect(RestoringChanged);
    disconnect(DataMerged);

    bHasSynced = false;
    bLastCycleSucceeded = false;

    if (AppWorkspaceSync* Sync{Host->GetSync()})
    {
        CycleStarted = connect(Sync, &AppWorkspaceSync::cycleStarted, this, &AppSyncController::statusChanged);
        CycleFinished = connect(Sync, &AppWorkspaceSync::cycleFinished, this, &AppSyncController::OnCycleFinished);
        RestoringChanged = connect(Sync, &AppWorkspaceSync::restoringChanged, this, &AppSyncController::statusChanged);
        DataMerged = connect(Sync, &AppWorkspaceSync::dataMerged, this, &AppSyncController::dataMerged);

        Sync->HoldMerges(bMergesHeld);
    }

    emit signedInChanged();
    emit accountChanged();
    emit statusChanged();
}

void AppSyncController::OnCycleFinished(bool bSuccess)
{
    bHasSynced = true;
    bLastCycleSucceeded = bSuccess;

    emit accountChanged();
    emit statusChanged();
}

void AppSyncController::OnApplicationStateChanged(Qt::ApplicationState State)
{
    AppWorkspaceSync* Sync{Host->GetSync()};

    if (Sync == nullptr)
    {
        return;
    }

    if (State == Qt::ApplicationActive)
    {
        Sync->OnForeground();

        return;
    }

    if (State == Qt::ApplicationHidden || State == Qt::ApplicationSuspended)
    {
        Sync->OnBackground();

#if defined(Q_OS_IOS)
        if (Sync->IsRunning())
        {
            const unsigned long Task{AppIosBackgroundTask::Begin(QStringLiteral("sync upload"))};

            connect(Sync, &AppWorkspaceSync::cycleFinished, this, [Task]() { AppIosBackgroundTask::End(Task); },
                    Qt::SingleShotConnection);
        }
#endif
    }
}

void AppSyncController::OnNetworkBack()
{
    if (AppWorkspaceSync* Sync{Host->GetSync()})
    {
        Sync->OnNetworkBack();
    }
}

bool AppSyncController::IsSignedIn() const
{
    return Host->GetRegistry().GetActiveAccount().has_value();
}

bool AppSyncController::NeedsReconnect() const
{
    const std::optional<AppAccount> Active{Host->GetRegistry().GetActiveAccount()};

    return Active.has_value() && Active->bNeedsReconnect;
}

bool AppSyncController::IsBusy() const
{
    return Account->IsBusy();
}

bool AppSyncController::IsSyncing() const
{
    const AppWorkspaceSync* Sync{Host->GetSync()};

    return Sync != nullptr && Sync->IsRunning();
}

bool AppSyncController::IsRestoring() const
{
    const AppWorkspaceSync* Sync{Host->GetSync()};

    return Sync != nullptr && Sync->IsRestoring();
}

QString AppSyncController::GetStatusText() const
{
    if (Account->IsBusy())
    {
        return tr("Waiting for Google…");
    }

    if (!IsSignedIn())
    {
        return Account->GetLastError().isEmpty() ? tr("Not signed in") : Account->GetLastError();
    }

    if (NeedsReconnect())
    {
        return tr("Google asked you to sign in again. Sync is paused.");
    }

    if (!Account->GetLastError().isEmpty())
    {
        return Account->GetLastError();
    }

    if (IsSyncing())
    {
        return IsRestoring() ? tr("Restoring your words…") : tr("Syncing…");
    }

    if (Host->GetSync() == nullptr || !bHasSynced)
    {
        return tr("Signed in");
    }

    return bLastCycleSucceeded ? tr("Synced") : tr("Not synced yet, will retry");
}

QString AppSyncController::GetAccountEmail() const
{
    const std::optional<AppAccount> Active{Host->GetRegistry().GetActiveAccount()};

    return Active.has_value() ? Active->Email : QString();
}

QString AppSyncController::GetAccountName() const
{
    const std::optional<AppAccount> Active{Host->GetRegistry().GetActiveAccount()};

    return Active.has_value() ? Active->Name : QString();
}

bool AppSyncController::IsConfigured() const
{
    return Account->IsConfigured();
}

bool AppSyncController::AreMergesHeld() const
{
    return bMergesHeld;
}

void AppSyncController::SetMergesHeld(bool bHeld)
{
    if (bMergesHeld == bHeld)
    {
        return;
    }

    bMergesHeld = bHeld;

    qCInfo(LogSync) << "Sync merges" << (bHeld ? "held" : "allowed");

    if (AppWorkspaceSync* Sync{Host->GetSync()})
    {
        Sync->HoldMerges(bHeld);
    }

    emit mergesHeldChanged();
}

void AppSyncController::eraseAllData()
{
    if (!AppDatabase::Get().IsReady())
    {
        return;
    }

    if (!AppDatabase::Get().GetCardRepository().DeleteEverything())
    {
        qCWarning(LogDatabase) << "Erasing the workspace data failed:"
                               << QString::fromStdString(AppDatabase::Get().GetCardRepository().GetLastError());

        return;
    }

    if (!AppDatabase::Get().GetReaderLibrary().RemoveAllDocuments())
    {
        qCWarning(LogDatabase) << "Clearing the reader library failed";
    }

    FTDatabase& Local = AppDatabase::Get().GetConnection();

    Local.Execute("VACUUM");

    AppDatabase::Get().ApplyActiveLanguages();

    qCInfo(LogDatabase) << "Local data erased";

    emit dataErased();
    emit localDataChanged(true);
}

void AppSyncController::deleteLanguage(const QString& TargetLanguage)
{
    FTCardRepository& CardRepository{AppDatabase::Get().GetCardRepository()};

    if (!CardRepository.DeleteLanguage(TargetLanguage.toStdString()))
    {
        qCWarning(LogDatabase) << "Deleting the words of" << TargetLanguage << "failed";

        emit languageDeleted(false, tr("The words could not be deleted."));

        return;
    }

    qCInfo(LogDatabase) << "Deleted the words of" << TargetLanguage;

    if (CardRepository.GetTargetLanguage() == TargetLanguage.toStdString())
    {
        ClearExerciseSessionState();
    }

    emit localDataChanged(true);
    emit languageDeleted(true, tr("The words were deleted."));
}

int AppSyncController::countLanguageWords(const QString& TargetLanguage) const
{
    if (!AppDatabase::Get().IsReady())
    {
        return 0;
    }

    return AppDatabase::Get().GetCardRepository().CountLanguageCards(TargetLanguage.toStdString());
}

void AppSyncController::importDatabase(const QUrl& SourceUrl)
{
    const QString SourcePath{SourceUrl.isLocalFile() ? SourceUrl.toLocalFile() : SourceUrl.toString()};

    qCInfo(LogSync) << "The user asked to import" << SourcePath;

    if (const std::optional<AppIncomingProblem> Problem{Host->ImportDatabase(SourcePath)})
    {
        if (Problem->bBusy)
        {
            emit importFinished(false, tr("A sync is running. Try again in a moment."));
        }
        else if (Problem->bFromNewerApp)
        {
            emit importFinished(false, tr("That database comes from a newer Ikonika. Update the app first."));
        }
        else
        {
            emit importFinished(false, tr("That file could not be imported."));
        }

        return;
    }

    emit dataMerged();
    emit importFinished(true, tr("The database was merged into yours."));
}

void AppSyncController::exportDatabase(const QUrl& DestinationUrl)
{
    const QString SnapshotPath = CreateSnapshot();

    if (SnapshotPath.isEmpty())
    {
        emit exportFinished(false, tr("Could not prepare the database for export."));

        return;
    }

    QFile Snapshot(SnapshotPath);

    if (!Snapshot.open(QIODevice::ReadOnly))
    {
        qCWarning(LogDatabase) << "Could not read" << SnapshotPath << "for export";

        emit exportFinished(false, tr("Could not prepare the database for export."));

        return;
    }

    const QByteArray Data = Snapshot.readAll();
    Snapshot.close();
    QFile::remove(SnapshotPath);

    const QString DestinationPath =
        DestinationUrl.isLocalFile() ? DestinationUrl.toLocalFile() : DestinationUrl.toString();

    QFile Destination(DestinationPath);

    if (!Destination.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qCWarning(LogDatabase) << "Could not open" << DestinationPath << "for export";

        emit exportFinished(false, tr("Could not write that file."));

        return;
    }

    if (Destination.write(Data) != Data.size())
    {
        qCWarning(LogDatabase) << "Could not write" << DestinationPath << "for export";

        emit exportFinished(false, tr("Could not write that file."));

        return;
    }

    qCInfo(LogDatabase) << "Exported the database to" << DestinationPath;

    emit exportFinished(true, tr("Your database was saved."));
}

void AppSyncController::shareDatabase()
{
    const QString SnapshotPath = CreateSnapshot();

    if (SnapshotPath.isEmpty())
    {
        emit exportFinished(false, tr("Could not prepare the database for export."));

        return;
    }

    const QString SharePath = GetWorkingDirectory() + QStringLiteral("/ikonika-backup.db");
    QFile::remove(SharePath);

    if (!QFile::rename(SnapshotPath, SharePath))
    {
        qCWarning(LogDatabase) << "Could not move" << SnapshotPath << "to" << SharePath;

        emit exportFinished(false, tr("Could not prepare the database for export."));

        return;
    }

#if defined(Q_OS_ANDROID)
    QJniObject Activity = QNativeInterface::QAndroidApplication::context();
    QJniObject JniPath = QJniObject::fromString(SharePath);
    QJniObject JniMime = QJniObject::fromString(QStringLiteral("application/octet-stream"));
    QJniObject JniTitle = QJniObject::fromString(tr("Share your Ikonika database"));

    const bool bShared = QJniObject::callStaticMethod<jboolean>(
        "com/goldenphasmid/ikonika/PlatformShare", "shareFile",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
        Activity.object<jobject>(), JniPath.object<jstring>(), JniMime.object<jstring>(),
        JniTitle.object<jstring>());

    if (!bShared)
    {
        qCWarning(LogDatabase) << "The share sheet could not be opened for" << SharePath;

        emit exportFinished(false, tr("Could not open the share sheet."));

        return;
    }

    qCInfo(LogDatabase) << "Opened the share sheet for" << SharePath;

    emit exportFinished(true, QString());
#elif defined(Q_OS_IOS)
    if (!AppIosShare::ShareFile(SharePath))
    {
        emit exportFinished(false, tr("Could not open the share sheet."));

        return;
    }

    emit exportFinished(true, QString());
#else
    QFile::remove(SharePath);

    emit exportFinished(false, tr("Sharing is not available on this device."));
#endif
}

void AppSyncController::ClearExerciseSessionState()
{
    AppState::Get().IntensiveLearningCheckpoints.clear();
    AppState::Get().RepetitionCheckpoint.clear();
    AppState::Get().SentenceCheckpoints.clear();

    for (const auto& [SessionKey, CheckpointJson] : AppSessionCheckpoints::LoadAll("Intensive"))
    {
        Q_UNUSED(CheckpointJson);

        AppSessionCheckpoints::Clear("Intensive", SessionKey);
    }

    AppSessionCheckpoints::Clear("Repetition", "");

    for (const auto& [ExerciseKey, CheckpointJson] : AppSessionCheckpoints::LoadAll("Sentences"))
    {
        Q_UNUSED(CheckpointJson);

        AppSessionCheckpoints::Clear("Sentences", ExerciseKey);
    }
}

QString AppSyncController::CreateSnapshot()
{
    const QString Target = GetWorkingDirectory() + QStringLiteral("/export-snapshot.db");
    QFile::remove(Target);

    const std::string Sql = "VACUUM INTO '" + Target.toStdString() + "'";

    if (!AppDatabase::Get().GetConnection().Execute(Sql))
    {
        qCWarning(LogDatabase) << "VACUUM INTO failed:"
                               << QString::fromStdString(
                                      AppDatabase::Get().GetConnection().GetLastError());

        return {};
    }

    return Target;
}

QString AppSyncController::GetWorkingDirectory() const
{
    return QFileInfo(AppDatabase::Get().GetFilePath()).absolutePath();
}
