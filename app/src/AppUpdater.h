#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include <memory>

class AppFileTransfer;

class AppUpdater : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ IsBusy NOTIFY busyChanged)
    Q_PROPERTY(bool ready READ IsReady NOTIFY readyChanged)
    Q_PROPERTY(qint64 receivedBytes READ GetReceivedBytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 totalBytes READ GetTotalBytes NOTIFY progressChanged)
    Q_PROPERTY(QString lastError READ GetLastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool storePackage READ IsStorePackage CONSTANT)
    Q_PROPERTY(bool canUninstall READ CanUninstall CONSTANT)

public:
    static AppUpdater& Get();

    explicit AppUpdater(QObject* Parent = nullptr);
    ~AppUpdater() override;

    Q_INVOKABLE void Download();
    Q_INVOKABLE void ApplyAndRestart();
    Q_INVOKABLE void Cancel();
    Q_INVOKABLE void Uninstall();

    /** Adopts an installer left by a previous run when it matches the published build, and clears
        anything else out of the update directory. */
    void AdoptOrDiscardPendingUpdate();

    bool IsBusy() const;
    bool IsReady() const;
    qint64 GetReceivedBytes() const;
    qint64 GetTotalBytes() const;
    QString GetLastError() const;
    bool IsStorePackage() const;
    bool CanUninstall() const;

signals:
    void busyChanged();
    void readyChanged();
    void progressChanged();
    void lastErrorChanged();
    void updateReady();
    void updateFailed(const QString& Error);

private:
    void MarkReady(const QString& FilePath);
    void Fail(const QString& Message);
    QString FindUninstaller() const;

    std::unique_ptr<AppFileTransfer> Transfer;

    QString InstallerPath;
    QStringList SilentArguments;
    QString LastError;

    qint64 ReceivedBytes = 0;
    qint64 TotalBytes = 0;
    bool bBusy = false;
};

struct AppUpdaterSingleton
{
    Q_GADGET
    QML_FOREIGN(AppUpdater)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppUpdater)

public:
    static AppUpdater* create(QQmlEngine* Engine, QJSEngine*)
    {
        AppUpdater& Instance = AppUpdater::Get();

        QQmlEngine::setObjectOwnership(&Instance, QQmlEngine::CppOwnership);
        Q_UNUSED(Engine);

        return &Instance;
    }
};
