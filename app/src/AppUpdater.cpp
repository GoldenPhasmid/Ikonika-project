#include "AppUpdater.h"

#include "AppCatalog.h"
#include "AppFileTransfer.h"
#include "AppPackageDownloader.h"
#include "AppPaths.h"

#include "core/FTCatalog.h"
#include "core/FTLog.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

namespace
{
    QStringList ToStringList(const std::vector<std::string>& Values)
    {
        QStringList Result;

        for (const std::string& Value : Values)
        {
            Result.append(QString::fromStdString(Value));
        }

        return Result;
    }

    bool MatchesDigest(const QString& FilePath, const std::string& ExpectedSha256)
    {
        QFile File(FilePath);

        if (!File.open(QIODevice::ReadOnly))
        {
            return false;
        }

        QCryptographicHash Digest(QCryptographicHash::Sha256);

        if (!Digest.addData(&File))
        {
            return false;
        }

        return QString::fromLatin1(Digest.result().toHex()) ==
               QString::fromStdString(ExpectedSha256).toLower();
    }

    void RemoveEverythingExcept(const QString& Directory, const QString& KeptFileName)
    {
        const QDir Updates(Directory);

        if (!Updates.exists())
        {
            return;
        }

        for (const QString& Name : Updates.entryList(QDir::Files))
        {
            if (Name == KeptFileName)
            {
                continue;
            }

            qCInfo(LogUpdate) << "Removing a stale update file:" << Name;

            QFile::remove(Updates.filePath(Name));
        }
    }
}

AppUpdater& AppUpdater::Get()
{
    static AppUpdater Instance;

    return Instance;
}

AppUpdater::AppUpdater(QObject* Parent)
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
            [this](const QString& FilePath) { MarkReady(FilePath); });

    connect(Transfer.get(), &AppFileTransfer::failed, this,
            [this](const QString& Error) { Fail(Error); });
}

AppUpdater::~AppUpdater() = default;

bool AppUpdater::IsBusy() const
{
    return bBusy;
}

bool AppUpdater::IsReady() const
{
    return !InstallerPath.isEmpty() && QFile::exists(InstallerPath);
}

qint64 AppUpdater::GetReceivedBytes() const
{
    return ReceivedBytes;
}

qint64 AppUpdater::GetTotalBytes() const
{
    return TotalBytes;
}

QString AppUpdater::GetLastError() const
{
    return LastError;
}

bool AppUpdater::IsStorePackage() const
{
    return AppPaths::IsStorePackage();
}

bool AppUpdater::CanUninstall() const
{
    return !FindUninstaller().isEmpty();
}

QString AppUpdater::FindUninstaller() const
{
    if (IsStorePackage())
    {
        return {};
    }

    const QDir InstallDirectory{QCoreApplication::applicationDirPath()};
    const QStringList Found{InstallDirectory.entryList({QStringLiteral("unins*.exe")}, QDir::Files,
                                                       QDir::Name)};

    if (Found.isEmpty())
    {
        return {};
    }

    return InstallDirectory.filePath(Found.first());
}

void AppUpdater::Uninstall()
{
    const QString Uninstaller{FindUninstaller()};

    if (Uninstaller.isEmpty())
    {
        qCWarning(LogUpdate) << "Asked to uninstall, but this copy has no uninstaller.";

        return;
    }

    qCInfo(LogUpdate) << "Starting the uninstaller" << Uninstaller << "and quitting for it";

#if QT_CONFIG(process)
    if (!QProcess::startDetached(Uninstaller, {}, QFileInfo(Uninstaller).absolutePath()))
    {
        qCWarning(LogUpdate) << "The uninstaller could not be started.";

        return;
    }
#else
    qCWarning(LogUpdate) << "This platform cannot start another process, so the uninstaller stays unused.";

    return;
#endif

    QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit,
                              Qt::QueuedConnection);
}

void AppUpdater::AdoptOrDiscardPendingUpdate()
{
    const FTCatalog* Catalog = AppCatalog::Get().GetCatalog();
    const QString Updates = AppPaths::PendingUpdateDirectory();

    if (Catalog == nullptr || !Catalog->App.has_value() ||
        !AppCatalog::Get().IsUpdateAvailable())
    {
        RemoveEverythingExcept(Updates, QString());

        return;
    }

    const FTCatalogApp& Published = *Catalog->App;
    const QString FileName = QString::fromStdString(Published.FileName);
    const QString Candidate = QDir(Updates).filePath(FileName);

    RemoveEverythingExcept(Updates, FileName);

    if (FileName.isEmpty() || !QFile::exists(Candidate))
    {
        return;
    }

    if (QFileInfo(Candidate).size() != Published.Bytes ||
        !MatchesDigest(Candidate, Published.Sha256))
    {
        qCInfo(LogUpdate) << "Discarding a pending update that no longer verifies:" << Candidate;

        QFile::remove(Candidate);

        return;
    }

    SilentArguments = ToStringList(Published.SilentArguments);

    qCInfo(LogUpdate) << "Adopting the update downloaded earlier:" << Candidate;

    MarkReady(Candidate);
}

void AppUpdater::Download()
{
    if (bBusy || IsReady())
    {
        return;
    }

    const FTCatalog* Catalog = AppCatalog::Get().GetCatalog();

    if (Catalog == nullptr || !Catalog->App.has_value())
    {
        Fail(tr("No application build is published."));

        return;
    }

    if (!AppCatalog::Get().IsUpdateAvailable())
    {
        qCInfo(LogUpdate) << "Already on the published version"
                          << QString::fromStdString(Catalog->App->Version);

        return;
    }

    const FTCatalogApp& Published = *Catalog->App;

    SilentArguments = ToStringList(Published.SilentArguments);

    const QString FileName = Published.FileName.empty()
                                 ? QStringLiteral("Ikonika-Setup.exe")
                                 : QString::fromStdString(Published.FileName);

    RemoveEverythingExcept(AppPaths::PendingUpdateDirectory(), QString());

    const QString Destination = QDir(AppPaths::PendingUpdateDirectory()).filePath(FileName);

    bBusy = true;
    ReceivedBytes = 0;
    TotalBytes = Published.Bytes;
    LastError.clear();

    qCInfo(LogUpdate) << "Downloading version" << QString::fromStdString(Published.Version);

    emit busyChanged();
    emit progressChanged();
    emit lastErrorChanged();

    Transfer->Start(QUrl(QString::fromStdString(Published.Url)), Destination,
                    QString::fromStdString(Published.Sha256), Published.Bytes);
}

void AppUpdater::ApplyAndRestart()
{
    if (!IsReady())
    {
        qCWarning(LogUpdate) << "Asked to apply an update with nothing downloaded.";

        return;
    }

    if (AppPackageDownloader::Get().HasPendingWork())
    {
        qCWarning(LogUpdate) << "Asked to apply an update while packages are still downloading or installing.";

        return;
    }

    qCInfo(LogUpdate) << "Starting the installer" << InstallerPath << SilentArguments
                      << "and quitting for it";

#if QT_CONFIG(process)
    if (!QProcess::startDetached(InstallerPath, SilentArguments,
                                 QFileInfo(InstallerPath).absolutePath()))
    {
        Fail(tr("The installer could not be started."));

        return;
    }
#else
    Fail(tr("The installer could not be started."));

    return;
#endif

    QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit,
                              Qt::QueuedConnection);
}

void AppUpdater::Cancel()
{
    if (bBusy)
    {
        Transfer->Cancel();
    }

    bBusy = false;
    InstallerPath.clear();
    ReceivedBytes = 0;
    TotalBytes = 0;

    RemoveEverythingExcept(AppPaths::PendingUpdateDirectory(), QString());

    emit busyChanged();
    emit readyChanged();
    emit progressChanged();
}

void AppUpdater::MarkReady(const QString& FilePath)
{
    InstallerPath = FilePath;
    bBusy = false;

    qCInfo(LogUpdate) << "Update ready:" << FilePath;

    emit busyChanged();
    emit readyChanged();
    emit updateReady();
}

void AppUpdater::Fail(const QString& Message)
{
    bBusy = false;
    LastError = Message;
    InstallerPath.clear();

    qCWarning(LogUpdate) << "Update failed:" << Message;

    emit busyChanged();
    emit readyChanged();
    emit lastErrorChanged();
    emit updateFailed(Message);
}
