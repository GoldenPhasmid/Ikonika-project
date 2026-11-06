#pragma once

#include <QJSEngine>
#include <QMetaObject>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class AppGoogleAccount;
class AppWorkspaceHost;
struct AppAccount;

class AppSyncController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool signedIn READ IsSignedIn NOTIFY signedInChanged)
    Q_PROPERTY(bool needsReconnect READ NeedsReconnect NOTIFY accountChanged)
    Q_PROPERTY(bool busy READ IsBusy NOTIFY busyChanged)
    Q_PROPERTY(bool syncing READ IsSyncing NOTIFY statusChanged)
    Q_PROPERTY(bool restoring READ IsRestoring NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ GetStatusText NOTIFY statusChanged)
    Q_PROPERTY(QString accountEmail READ GetAccountEmail NOTIFY accountChanged)
    Q_PROPERTY(QString accountName READ GetAccountName NOTIFY accountChanged)
    Q_PROPERTY(bool configured READ IsConfigured CONSTANT)
    Q_PROPERTY(bool mergesHeld READ AreMergesHeld WRITE SetMergesHeld NOTIFY mergesHeldChanged)

public:
    static AppSyncController* GetInstance();

    AppSyncController(AppWorkspaceHost& Host, AppGoogleAccount& Account, QObject* Parent = nullptr);
    ~AppSyncController() override;

    Q_INVOKABLE void signIn();
    Q_INVOKABLE void switchAccount();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void syncNow();

    Q_INVOKABLE void eraseAllData();
    Q_INVOKABLE void exportDatabase(const QUrl& DestinationUrl);
    Q_INVOKABLE void importDatabase(const QUrl& SourceUrl);
    Q_INVOKABLE void shareDatabase();
    Q_INVOKABLE void deleteLanguage(const QString& TargetLanguage);
    Q_INVOKABLE int countLanguageWords(const QString& TargetLanguage) const;

    void OnApplicationStateChanged(Qt::ApplicationState State);
    void OnNetworkBack();

    bool IsSignedIn() const;
    bool NeedsReconnect() const;
    bool IsBusy() const;
    bool IsSyncing() const;
    bool IsRestoring() const;
    QString GetStatusText() const;
    QString GetAccountEmail() const;
    QString GetAccountName() const;
    bool IsConfigured() const;
    bool AreMergesHeld() const;
    void SetMergesHeld(bool bHeld);

signals:
    void signedInChanged();
    void busyChanged();
    void statusChanged();
    void accountChanged();
    void mergesHeldChanged();

    void dataMerged();
    void localDataChanged(bool replaced);
    void dataErased();
    void exportFinished(bool bSuccess, const QString& Message);
    void importFinished(bool bSuccess, const QString& Message);
    void languageDeleted(bool bSuccess, const QString& Message);

private:
    void OnWorkspaceOpened();
    void OnSignedIn(const AppAccount& Account, const QString& Credential);
    void OnCycleFinished(bool bSuccess);
    void ClearExerciseSessionState();
    QString CreateSnapshot();
    QString GetWorkingDirectory() const;

    AppWorkspaceHost* Host = nullptr;
    AppGoogleAccount* Account = nullptr;
    QMetaObject::Connection CycleStarted;
    QMetaObject::Connection CycleFinished;
    QMetaObject::Connection RestoringChanged;
    QMetaObject::Connection DataMerged;
    bool bHasSynced = false;
    bool bLastCycleSucceeded = false;
    bool bMergesHeld = false;
};

struct AppSyncControllerSingleton
{
    Q_GADGET
    QML_FOREIGN(AppSyncController)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppSyncController)

public:
    static AppSyncController* create(QQmlEngine* Engine, QJSEngine*)
    {
        Q_UNUSED(Engine);

        AppSyncController* Instance = AppSyncController::GetInstance();

        QJSEngine::setObjectOwnership(Instance, QJSEngine::CppOwnership);

        return Instance;
    }
};
