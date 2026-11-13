#pragma once

#include <QMap>
#include <QString>

#include <optional>

class AppCredentialStore;

struct AppAccount
{
    QString Id;
    QString Email;
    QString Name;
    bool bNeedsReconnect = false;
    bool bMissingDriveAccess = false;
};

struct AppWorkspaceFolder
{
    QString Directory;

    QString GetSettingsFilePath() const;
    QString GetDatabaseFilePath() const;
    QString GetDeviceDatabaseFilePath() const;
    QString GetBooksDirectory() const;
    QString GetCheckpointsDirectory() const;
};

class AppAccountRegistry
{
public:
    AppAccountRegistry(const QString& DataRoot, AppCredentialStore& Credentials);

    AppWorkspaceFolder GetActiveFolder() const;
    std::optional<AppAccount> GetActiveAccount() const;

    bool SignIn(const AppAccount& Account, const QString& Credential);
    void SignOut();
    void MarkNeedsReconnect(const QString& AccountId, bool bMissingDriveAccess = false);

    static AppWorkspaceFolder MakeFolder(const QString& DataRoot, const QString& AccountId);

private:
    void Load();
    void Save() const;
    void PrepareActiveFolder() const;
    AppWorkspaceFolder GetFolderFor(const QString& AccountId) const;
    QString GetRegistryFilePath() const;

    QString DataRoot;
    AppCredentialStore* Credentials = nullptr;
    QMap<QString, AppAccount> Accounts;
    QString ActiveAccountId;
};
