#include "AppAccountRegistry.h"

#include "AppCredentialStore.h"

#include "core/FTLog.h"

#include <QDir>
#include <QRegularExpression>
#include <QSettings>

namespace
{
    const QString SignedOutFolderName{QStringLiteral("signed-out")};
    const QString AccountFolderPrefix{QStringLiteral("google-")};

    const QString ActiveKey{QStringLiteral("active")};
    const QString AccountsGroup{QStringLiteral("accounts")};
    const QString EmailKey{QStringLiteral("email")};
    const QString NameKey{QStringLiteral("name")};
    const QString NeedsReconnectKey{QStringLiteral("needsReconnect")};
    const QString MissingDriveAccessKey{QStringLiteral("missingDriveAccess")};

    bool IsPlainFolderName(const QString& AccountId)
    {
        static const QRegularExpression Pattern{QStringLiteral("^[A-Za-z0-9_-]+$")};

        return Pattern.match(AccountId).hasMatch();
    }
}

QString AppWorkspaceFolder::GetSettingsFilePath() const
{
    return QDir(Directory).filePath(QStringLiteral("settings.ini"));
}

QString AppWorkspaceFolder::GetDatabaseFilePath() const
{
    return QDir(Directory).filePath(QStringLiteral("Ikonika.db"));
}

QString AppWorkspaceFolder::GetDeviceDatabaseFilePath() const
{
    return QDir(Directory).filePath(QStringLiteral("Device.db"));
}

QString AppWorkspaceFolder::GetBooksDirectory() const
{
    return QDir(Directory).filePath(QStringLiteral("books"));
}

QString AppWorkspaceFolder::GetCheckpointsDirectory() const
{
    return QDir(Directory).filePath(QStringLiteral("checkpoints"));
}

AppAccountRegistry::AppAccountRegistry(const QString& DataRoot, AppCredentialStore& Credentials)
    : DataRoot{DataRoot}
    , Credentials{&Credentials}
{
    Load();
    PrepareActiveFolder();
}

AppWorkspaceFolder AppAccountRegistry::GetActiveFolder() const
{
    return GetFolderFor(ActiveAccountId);
}

std::optional<AppAccount> AppAccountRegistry::GetActiveAccount() const
{
    const auto Found = Accounts.constFind(ActiveAccountId);

    if (Found == Accounts.constEnd())
    {
        return std::nullopt;
    }

    return *Found;
}

bool AppAccountRegistry::SignIn(const AppAccount& Account, const QString& Credential)
{
    if (!IsPlainFolderName(Account.Id))
    {
        qCWarning(LogApp) << "Sign-in refused, the account id is not a plain folder name:"
                          << Account.Id;

        return false;
    }

    if (!Credentials->Write(Account.Id, Credential))
    {
        qCWarning(LogApp) << "Sign-in refused, the credential store did not take the credential of"
                          << Account.Email;

        return false;
    }

    AppAccount Stored{Account};
    Stored.bNeedsReconnect = false;
    Stored.bMissingDriveAccess = false;

    Accounts.insert(Stored.Id, Stored);
    ActiveAccountId = Stored.Id;

    Save();
    PrepareActiveFolder();

    qCInfo(LogApp) << "Signed in as" << Stored.Email << ", workspace" << GetActiveFolder().Directory;

    return true;
}

void AppAccountRegistry::SignOut()
{
    if (ActiveAccountId.isEmpty())
    {
        return;
    }

    qCInfo(LogApp) << "Signed out of" << Accounts.value(ActiveAccountId).Email;

    Credentials->Remove(ActiveAccountId);
    ActiveAccountId.clear();

    Save();
    PrepareActiveFolder();
}

void AppAccountRegistry::MarkNeedsReconnect(const QString& AccountId, bool bMissingDriveAccess)
{
    const auto Found = Accounts.find(AccountId);

    if (Found == Accounts.end())
    {
        qCWarning(LogApp) << "Cannot mark an unknown account as needing reconnect:" << AccountId;

        return;
    }

    Found->bNeedsReconnect = true;
    Found->bMissingDriveAccess = bMissingDriveAccess;

    Save();

    qCInfo(LogApp) << (bMissingDriveAccess ? "Google withheld Drive access from" : "Google refused the credential of")
                   << Found->Email << ", the account needs reconnect";
}

void AppAccountRegistry::Load()
{
    QSettings Registry(GetRegistryFilePath(), QSettings::IniFormat);

    Registry.beginGroup(AccountsGroup);

    for (const QString& AccountId : Registry.childGroups())
    {
        if (!IsPlainFolderName(AccountId))
        {
            continue;
        }

        Registry.beginGroup(AccountId);

        AppAccount Account;
        Account.Id = AccountId;
        Account.Email = Registry.value(EmailKey).toString();
        Account.Name = Registry.value(NameKey).toString();
        Account.bNeedsReconnect = Registry.value(NeedsReconnectKey).toBool();
        Account.bMissingDriveAccess = Registry.value(MissingDriveAccessKey).toBool();

        Accounts.insert(AccountId, Account);

        Registry.endGroup();
    }

    Registry.endGroup();

    const QString StoredActive{Registry.value(ActiveKey).toString()};

    if (!StoredActive.isEmpty() && !Accounts.contains(StoredActive))
    {
        qCWarning(LogApp) << "The registry names an unknown active account" << StoredActive
                          << ", opening the signed-out workspace";

        return;
    }

    ActiveAccountId = StoredActive;
}

void AppAccountRegistry::Save() const
{
    QSettings Registry(GetRegistryFilePath(), QSettings::IniFormat);

    Registry.clear();
    Registry.setValue(ActiveKey, ActiveAccountId);

    Registry.beginGroup(AccountsGroup);

    for (const AppAccount& Account : Accounts)
    {
        Registry.beginGroup(Account.Id);
        Registry.setValue(EmailKey, Account.Email);
        Registry.setValue(NameKey, Account.Name);
        Registry.setValue(NeedsReconnectKey, Account.bNeedsReconnect);
        Registry.setValue(MissingDriveAccessKey, Account.bMissingDriveAccess);
        Registry.endGroup();
    }

    Registry.endGroup();
    Registry.sync();

    if (Registry.status() != QSettings::NoError)
    {
        qCWarning(LogApp) << "Could not write the account registry" << GetRegistryFilePath();
    }
}

void AppAccountRegistry::PrepareActiveFolder() const
{
    const QString Directory{GetActiveFolder().Directory};

    if (!QDir().mkpath(Directory))
    {
        qCWarning(LogApp) << "Could not create the workspace folder" << Directory;
    }
}

AppWorkspaceFolder AppAccountRegistry::GetFolderFor(const QString& AccountId) const
{
    return MakeFolder(DataRoot, AccountId);
}

AppWorkspaceFolder AppAccountRegistry::MakeFolder(const QString& DataRoot, const QString& AccountId)
{
    const QString FolderName{AccountId.isEmpty() ? SignedOutFolderName
                                                 : AccountFolderPrefix + AccountId};

    return AppWorkspaceFolder{QDir(DataRoot).filePath(QStringLiteral("workspaces/") + FolderName)};
}

QString AppAccountRegistry::GetRegistryFilePath() const
{
    return QDir(DataRoot).filePath(QStringLiteral("accounts.ini"));
}
