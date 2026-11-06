#include "AppPackageDownloader.h"

#include "AppCatalog.h"
#include "AppDictionary.h"
#include "AppFileTransfer.h"
#include "AppPaths.h"
#include "AppSpeech.h"
#include "AppTranslator.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"
#include "core/FTZipExtract.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QStorageInfo>

AppPackageDownloader& AppPackageDownloader::Get()
{
    static AppPackageDownloader Instance;

    return Instance;
}

AppPackageDownloader::AppPackageDownloader(QObject* Parent)
    : QObject(Parent)
    , Transfer(std::make_unique<AppFileTransfer>())
{
    connect(Transfer.get(), &AppFileTransfer::progress, this,
            [this](qint64 Received, qint64 Total)
            {
                ReceivedBytes = Received;

                if (Total > 0)
                {
                    TotalBytes = Total;
                }

                emit progressChanged();
            });

    connect(Transfer.get(), &AppFileTransfer::finished, this,
            &AppPackageDownloader::OnDownloaded);

    connect(Transfer.get(), &AppFileTransfer::failed, this,
            [this](const QString& Error) { Fail(Error); });
}

AppPackageDownloader::~AppPackageDownloader() = default;

bool AppPackageDownloader::IsBusy() const
{
    return !ActiveId.isEmpty();
}

QString AppPackageDownloader::GetActiveId() const
{
    return ActiveId;
}

qint64 AppPackageDownloader::GetReceivedBytes() const
{
    return ReceivedBytes;
}

qint64 AppPackageDownloader::GetTotalBytes() const
{
    return TotalBytes;
}

QString AppPackageDownloader::GetLastError() const
{
    return LastError;
}

QStringList AppPackageDownloader::GetQueue() const
{
    return Queue;
}

bool AppPackageDownloader::isPending(const QString& Id) const
{
    return ActiveId == Id || Queue.contains(Id);
}

void AppPackageDownloader::download(const QString& Id)
{
    if (isPending(Id))
    {
        qCInfo(LogUpdate) << Id << "is already downloading or queued";

        return;
    }

    Queue.append(Id);

    emit queueChanged();

    qCInfo(LogUpdate) << "Queued" << Id << "; waiting behind" << Queue.size() - 1;

    StartNextInQueue();
}

void AppPackageDownloader::forget(const QString& Id)
{
    if (ActiveId == Id)
    {
        cancel();

        return;
    }

    if (Queue.removeAll(Id) == 0)
    {
        return;
    }

    emit queueChanged();

    qCInfo(LogUpdate) << "Dropped" << Id << "from the queue";
}

void AppPackageDownloader::StartNextInQueue()
{
    if (IsBusy())
    {
        return;
    }

    while (!Queue.isEmpty())
    {
        const QString Id = Queue.takeFirst();

        emit queueChanged();

        if (Begin(Id))
        {
            return;
        }
    }
}

bool AppPackageDownloader::Begin(const QString& Id)
{
    const std::optional<FTCatalogPackage> Package = AppCatalog::Get().FindPackage(Id);

    if (!Package.has_value())
    {
        LastError = tr("No package called %1 is published.").arg(Id);

        emit lastErrorChanged();
        emit packageFailed(Id, LastError);

        return false;
    }

    if (!Package->IsCompatible())
    {
        LastError = tr("%1 was built for a dictionary layout this version cannot read.").arg(Id);

        emit lastErrorChanged();
        emit packageFailed(Id, LastError);

        return false;
    }

    const QString Destination = AppCatalog::ResolveDestination(*Package);

    if (Destination.isEmpty())
    {
        LastError = tr("%1 names a destination outside the download directory.").arg(Id);

        emit lastErrorChanged();
        emit packageFailed(Id, LastError);

        return false;
    }

    const qint64 Needed = Package->Bytes + Package->UnpackedBytes;

    QDir().mkpath(AppPaths::DownloadRoot());

    const QStorageInfo Storage(AppPaths::DownloadRoot());
    const qint64 Available = Storage.bytesAvailable();

    if (Storage.isValid() && Available >= 0 && Available < Needed)
    {
        LastError = tr("%1 needs %2 MB free and there is %3 MB.")
                        .arg(Id)
                        .arg(Needed / (1024 * 1024))
                        .arg(Available / (1024 * 1024));

        emit lastErrorChanged();
        emit packageFailed(Id, LastError);

        return false;
    }

    ActiveId = Id;
    ActiveVersion = Package->Version;
    ActiveDestination = Destination;
    ReceivedBytes = 0;
    TotalBytes = Package->Bytes;
    LastError.clear();

    const QString FileName = Package->FileName.empty()
                                 ? Id + QStringLiteral(".zip")
                                 : QString::fromStdString(Package->FileName);

    ArchivePath = QDir(AppPaths::DownloadScratchDirectory()).filePath(FileName);

    qCInfo(LogUpdate) << "Installing package" << Id << "version" << ActiveVersion << "into"
                      << Destination;

    emit busyChanged();
    emit progressChanged();
    emit lastErrorChanged();

    Transfer->Start(QUrl(QString::fromStdString(Package->Url)), ArchivePath,
                    QString::fromStdString(Package->Sha256), Package->Bytes);

    return true;
}

void AppPackageDownloader::cancel()
{
    if (!IsBusy())
    {
        return;
    }

    const QString Cancelled = ActiveId;

    Transfer->Cancel();
    QFile::remove(ArchivePath);
    Clear();

    qCInfo(LogUpdate) << "Cancelled" << Cancelled;

    StartNextInQueue();
}

bool AppPackageDownloader::Unpack(const FTCatalogPackage& Package,
                                 const QString& ArchiveFilePath)
{
    const QString Id = QString::fromStdString(Package.Id);
    const QString Destination = AppCatalog::ResolveDestination(Package);

    if (Destination.isEmpty())
    {
        return false;
    }

    qCInfo(LogUpdate) << "Unpacking" << Id << "into" << Destination;

    // A dictionary already in use is about to have its file rewritten, so the connection is let go
    // first and the same pair is opened again afterwards.
    const FTDictionaryPackage* Active = AppDictionary::Get().GetActivePackage();

    const bool bReplacingOpenDictionary = Package.Kind == CatalogKindDictionary
        && Active != nullptr
        && Active->TargetLanguage == Package.TargetLanguage;

    const FTLanguageRules Reopen = FTSettings::Get().GetLanguageRules();

    const bool bReplacingOutdatedDictionary = Package.Kind == CatalogKindDictionary
        && Active == nullptr
        && AppDictionary::Get().IsOutdated(Package.TargetLanguage);

    if (bReplacingOpenDictionary)
    {
        AppDictionary::Get().Close();
    }

    if (Package.Kind == CatalogKindTranslation)
    {
        AppTranslator::Reset();
    }

    if (!ExtractZipArchive(ArchiveFilePath.toStdString(), Destination.toStdString()))
    {
        if (bReplacingOpenDictionary)
        {
            AppDictionary::Get().Discover();
            AppDictionary::Get().Open(Reopen.TargetLanguage, Reopen.NativeLanguage);
        }

        return false;
    }

    AppCatalog::SetInstalledVersion(Id, Package.Version);

    qCInfo(LogUpdate) << "Installed" << Id << "version" << Package.Version;

    AppDictionary::Get().Discover();

    if (bReplacingOpenDictionary
        && !AppDictionary::Get().Open(Reopen.TargetLanguage, Reopen.NativeLanguage))
    {
        qCWarning(LogUpdate) << "The updated dictionary for"
                             << QString::fromStdString(Reopen.TargetLanguage)
                             << "could not be opened again";
    }

    if (bReplacingOutdatedDictionary && !AppDictionary::Get().Initialise())
    {
        qCWarning(LogUpdate) << "No dictionary could be opened after updating"
                             << QString::fromStdString(Package.TargetLanguage);
    }

    // A voice that arrives while the app is running is of no use until something loads it, and
    // speech reports itself unavailable until then.
    if (Package.Kind == CatalogKindVoice)
    {
        AppSpeech::Get().Reload();
    }

    if (Package.Kind == CatalogKindTranslation)
    {
        AppTranslator::Reset();
    }

    return true;
}

void AppPackageDownloader::OnDownloaded(const QString& InArchivePath)
{
    const QString Id = ActiveId;
    const std::optional<FTCatalogPackage> Package = AppCatalog::Get().FindPackage(Id);

    if (!Package.has_value())
    {
        QFile::remove(InArchivePath);
        Fail(tr("%1 is no longer published.").arg(Id));

        return;
    }

    const bool bUnpacked = Unpack(*Package, InArchivePath);

    QFile::remove(InArchivePath);

    if (!bUnpacked)
    {
        Fail(tr("%1 downloaded but could not be unpacked.").arg(Id));

        return;
    }

    Clear();

    emit packageInstalled(Id);

    StartNextInQueue();
}

void AppPackageDownloader::installFromFile(const QUrl& FileUrl)
{
    const QString Path = FileUrl.isLocalFile() ? FileUrl.toLocalFile() : FileUrl.toString();

    QFile Archive(Path);

    if (!Archive.open(QIODevice::ReadOnly))
    {
        FailImport(QString(), tr("%1 cannot be read.").arg(Path));

        return;
    }

    QCryptographicHash Digest(QCryptographicHash::Sha256);

    if (!Digest.addData(&Archive))
    {
        FailImport(QString(), tr("%1 cannot be read.").arg(Path));

        return;
    }

    Archive.close();

    const QString Computed = QString::fromLatin1(Digest.result().toHex());
    const FTCatalog* Catalog = AppCatalog::Get().GetCatalog();

    if (Catalog == nullptr)
    {
        FailImport(QString(), tr("Ikonika has not read the list of packages yet."));

        return;
    }

    for (const FTCatalogPackage& Package : Catalog->Packages)
    {
        if (QString::fromStdString(Package.Sha256).toLower() != Computed)
        {
            continue;
        }

        const QString Id = QString::fromStdString(Package.Id);

        if (!Package.IsCompatible())
        {
            FailImport(Id, tr("%1 was built for a dictionary layout this version cannot read.")
                               .arg(Id));

            return;
        }

        qCInfo(LogUpdate) << "Installing" << Id << "from" << Path;

        if (!Unpack(Package, Path))
        {
            FailImport(Id, tr("%1 could not be unpacked.").arg(Id));

            return;
        }

        LastError.clear();

        emit lastErrorChanged();
        emit packageInstalled(Id);

        return;
    }

    FailImport(QString(), tr("That file is not one of Ikonika's downloads."));
}

void AppPackageDownloader::FailImport(const QString& Id, const QString& Message)
{
    LastError = Message;

    qCWarning(LogUpdate) << "Import failed:" << Message;

    emit lastErrorChanged();
    emit packageFailed(Id, Message);
}

void AppPackageDownloader::Fail(const QString& Message)
{
    const QString Id = ActiveId;

    LastError = Message;

    Clear();

    qCWarning(LogUpdate) << "Package" << Id << "failed:" << Message;

    emit lastErrorChanged();
    emit packageFailed(Id, Message);

    StartNextInQueue();
}

void AppPackageDownloader::Clear()
{
    ActiveId.clear();
    ActiveDestination.clear();
    ArchivePath.clear();
    ActiveVersion = 0;
    ReceivedBytes = 0;
    TotalBytes = 0;

    emit busyChanged();
    emit progressChanged();
}
