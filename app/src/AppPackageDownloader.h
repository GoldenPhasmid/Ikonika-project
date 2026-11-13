#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <memory>

class AppFileTransfer;
struct FTCatalogPackage;

class AppPackageDownloader : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ IsBusy NOTIFY busyChanged)
    Q_PROPERTY(QString activeId READ GetActiveId NOTIFY busyChanged)
    Q_PROPERTY(qint64 receivedBytes READ GetReceivedBytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 totalBytes READ GetTotalBytes NOTIFY progressChanged)
    Q_PROPERTY(QString lastError READ GetLastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QStringList queue READ GetQueue NOTIFY queueChanged)

public:
    static AppPackageDownloader& Get();

    explicit AppPackageDownloader(QObject* Parent = nullptr);
    ~AppPackageDownloader() override;

    Q_INVOKABLE void download(const QString& Id);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void forget(const QString& Id);
    Q_INVOKABLE bool isPending(const QString& Id) const;

    void RestoreQueue();

    static bool InstallArchive(const QString& ArchivePath, const QString& DestinationDirectory,
                               const QString& StagingRoot);
    static void RecoverInterruptedInstalls(const QStringList& Roots, const QString& StagingRoot);
    static void RecoverInterruptedInstalls();

    /**
     * Installs a package from an archive the user already has, skipping the transfer. The file is
     * identified by its checksum rather than by its name, so a renamed copy still installs and a
     * file that is not one of Ikonika's downloads is refused.
     */
    Q_INVOKABLE void installFromFile(const QUrl& FileUrl);

    bool IsBusy() const;
    bool HasPendingWork() const;
    QString GetActiveId() const;
    qint64 GetReceivedBytes() const;
    qint64 GetTotalBytes() const;
    QString GetLastError() const;
    QStringList GetQueue() const;

signals:
    void busyChanged();
    void progressChanged();
    void lastErrorChanged();
    void queueChanged();
    void packageInstalled(const QString& Id);
    void packageFailed(const QString& Id, const QString& Message);

private:
    bool Begin(const QString& Id);
    bool Unpack(const FTCatalogPackage& Package, const QString& ArchiveFilePath);
    void FailImport(const QString& Id, const QString& Message);
    void StartNextInQueue();
    void OnDownloaded(const QString& ArchivePath);
    void Fail(const QString& Message);
    void Clear();
    void PersistQueue() const;

    QStringList Queue;
    QStringList PendingRestore;

    std::unique_ptr<AppFileTransfer> Transfer;

    QString ActiveId;
    QString ActiveDestination;
    QString ArchivePath;
    QString LastError;

    int ActiveVersion = 0;
    qint64 ReceivedBytes = 0;
    qint64 TotalBytes = 0;
};

struct AppPackageDownloaderSingleton
{
    Q_GADGET
    QML_FOREIGN(AppPackageDownloader)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppPackageDownloader)

public:
    static AppPackageDownloader* create(QQmlEngine* Engine, QJSEngine*)
    {
        AppPackageDownloader& Instance = AppPackageDownloader::Get();

        QQmlEngine::setObjectOwnership(&Instance, QQmlEngine::CppOwnership);
        Q_UNUSED(Engine);

        return &Instance;
    }
};
