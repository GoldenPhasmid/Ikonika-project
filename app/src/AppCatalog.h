#pragma once

#include "core/FTCatalog.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <optional>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class AppCatalog : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ IsBusy NOTIFY busyChanged)
    Q_PROPERTY(bool loaded READ IsLoaded NOTIFY catalogChanged)
    Q_PROPERTY(QString lastError READ GetLastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantList packages READ GetPackages NOTIFY catalogChanged)
    Q_PROPERTY(QVariantList languages READ GetLanguages NOTIFY languagesChanged)
    Q_PROPERTY(bool updateAvailable READ IsUpdateAvailable NOTIFY catalogChanged)
    Q_PROPERTY(QVariantMap dictionaryUpdate READ GetDictionaryUpdate NOTIFY dictionaryUpdateChanged)
    Q_PROPERTY(QVariantMap availableDictionaryUpdate READ GetAvailableDictionaryUpdate NOTIFY dictionaryUpdateChanged)
    Q_PROPERTY(QString publishedVersion READ GetPublishedVersion NOTIFY catalogChanged)
    Q_PROPERTY(bool appUpdateDismissed READ IsAppUpdateDismissed NOTIFY catalogChanged)
    Q_PROPERTY(QVariantList packageUpdates READ GetPackageUpdates NOTIFY catalogChanged)

public:
    static AppCatalog& Get();

    explicit AppCatalog(QObject* Parent = nullptr);
    ~AppCatalog() override;

    Q_INVOKABLE void Refresh();

    void RefreshWhenStale();

    /** A package id as something to show, "French dictionary" rather than "dictionary-fr". */
    Q_INVOKABLE QString describePackage(const QString& Id) const;

    /**
     * The published dictionary for the language being learned, when it is newer than the installed
     * one and the learner has not waved this version away. Empty otherwise.
     */
    QVariantMap GetDictionaryUpdate() const;
    QVariantMap GetAvailableDictionaryUpdate() const;

    /** Keeps the offer out of the way until a version after this one is published. */
    Q_INVOKABLE void dismissDictionaryUpdate();
    Q_INVOKABLE void dismissAppUpdate();
    Q_INVOKABLE void dismissPackageUpdate(const QString& Id);
    QVariantList GetPackageUpdates() const;

    bool IsBusy() const;
    bool IsLoaded() const;
    QString GetLastError() const;

    QVariantList GetPackages() const;

    /**
     * Every language that can be learned: the ones a dictionary is published for and the ones
     * already on disk. One row per language rather than per package, since a language is what
     * anyone chooses.
     */
    QVariantList GetLanguages() const;

    /** The voice package that speaks this language on the desktop, installed or not. Empty on a
        phone, which speaks through the platform voice. */
    QString FindVoicePackage(const std::string& TargetLanguage) const;

    std::optional<FTCatalogPackage> FindTranslationPackage(const std::string& TargetLanguage,
                                                           const std::string& NativeLanguage) const;

    bool IsUpdateAvailable() const;
    QString GetPublishedVersion() const;
    bool IsAppUpdateDismissed() const;

    const FTCatalog* GetCatalog() const;

    std::optional<FTCatalogPackage> FindPackage(const QString& Id) const;

    /** Empty when the package would unpack outside the directory its kind belongs in. */
    static QString ResolveDestination(const FTCatalogPackage& Package);

    static int GetInstalledVersion(const QString& Id);
    static void SetInstalledVersion(const QString& Id, int Version);

signals:
    void busyChanged();
    void lastErrorChanged();
    void catalogChanged();
    void languagesChanged();
    void dictionaryUpdateChanged();

private:
    void Fail(const QString& Message);
    void SetBusy(bool bValue);
    void WatchApplicationState();

    std::unique_ptr<QNetworkAccessManager> Network;
    QNetworkReply* Reply = nullptr;
    QTimer* RefreshCooldownTimer = nullptr;

    std::optional<FTCatalog> Catalog;
    QString LastError;
    bool bBusy = false;
};

struct AppCatalogSingleton
{
    Q_GADGET
    QML_FOREIGN(AppCatalog)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppCatalog)

public:
    static AppCatalog* create(QQmlEngine* Engine, QJSEngine*)
    {
        AppCatalog& Instance = AppCatalog::Get();

        QQmlEngine::setObjectOwnership(&Instance, QQmlEngine::CppOwnership);
        Q_UNUSED(Engine);

        return &Instance;
    }
};
