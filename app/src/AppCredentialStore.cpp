#include "AppCredentialStore.h"

#include "core/FTLog.h"

#include <utility>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <wincred.h>

AppWindowsCredentialStore::AppWindowsCredentialStore(QString Prefix)
    : Prefix{std::move(Prefix)}
{
}

bool AppWindowsCredentialStore::Write(const QString& AccountId, const QString& Secret)
{
    const std::wstring Target{(Prefix + AccountId).toStdWString()};
    QByteArray Blob{Secret.toUtf8()};

    CREDENTIALW Credential{};
    Credential.Type = CRED_TYPE_GENERIC;
    Credential.TargetName = const_cast<LPWSTR>(Target.c_str());
    Credential.CredentialBlobSize = static_cast<DWORD>(Blob.size());
    Credential.CredentialBlob = reinterpret_cast<LPBYTE>(Blob.data());
    Credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (!CredWriteW(&Credential, 0))
    {
        qCWarning(LogApp) << "Credential Manager did not store the credential of" << AccountId << ", error"
                          << GetLastError();

        return false;
    }

    return true;
}

QString AppWindowsCredentialStore::Read(const QString& AccountId) const
{
    const std::wstring Target{(Prefix + AccountId).toStdWString()};
    PCREDENTIALW Credential{nullptr};

    if (!CredReadW(Target.c_str(), CRED_TYPE_GENERIC, 0, &Credential))
    {
        if (GetLastError() != ERROR_NOT_FOUND)
        {
            qCWarning(LogApp) << "Credential Manager could not read the credential of" << AccountId << ", error"
                              << GetLastError();
        }

        return {};
    }

    const QString Secret{QString::fromUtf8(reinterpret_cast<const char*>(Credential->CredentialBlob),
                                           static_cast<qsizetype>(Credential->CredentialBlobSize))};
    CredFree(Credential);

    return Secret;
}

void AppWindowsCredentialStore::Remove(const QString& AccountId)
{
    const std::wstring Target{(Prefix + AccountId).toStdWString()};

    if (!CredDeleteW(Target.c_str(), CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND)
    {
        qCWarning(LogApp) << "Credential Manager could not remove the credential of" << AccountId << ", error"
                          << GetLastError();
    }
}
#endif

bool AppMemoryCredentialStore::Write(const QString& AccountId, const QString& Secret)
{
    Secrets.insert(AccountId, Secret);

    return true;
}

QString AppMemoryCredentialStore::Read(const QString& AccountId) const
{
    return Secrets.value(AccountId);
}

void AppMemoryCredentialStore::Remove(const QString& AccountId)
{
    Secrets.remove(AccountId);
}
