#include "AppCredentialStore.h"

#include "core/FTLog.h"

#include <utility>

#import <Foundation/Foundation.h>
#import <Security/Security.h>

namespace
{
    NSMutableDictionary* MakeQuery(const QString& Service, const QString& AccountId)
    {
        NSMutableDictionary* Query = [NSMutableDictionary dictionary];
        Query[(id)kSecClass] = (id)kSecClassGenericPassword;
        Query[(id)kSecAttrService] = Service.toNSString();
        Query[(id)kSecAttrAccount] = AccountId.toNSString();

        return Query;
    }
}

AppIosCredentialStore::AppIosCredentialStore(QString Service)
    : Service{std::move(Service)}
{
}

bool AppIosCredentialStore::Write(const QString& AccountId, const QString& Secret)
{
    SecItemDelete((CFDictionaryRef)MakeQuery(Service, AccountId));

    NSMutableDictionary* Item = MakeQuery(Service, AccountId);
    Item[(id)kSecValueData] = Secret.toUtf8().toNSData();
    Item[(id)kSecAttrAccessible] = (id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;

    const OSStatus Status = SecItemAdd((CFDictionaryRef)Item, nullptr);

    if (Status != errSecSuccess)
    {
        qCWarning(LogApp) << "The Keychain did not store the credential of" << AccountId << ", status" << Status;

        return false;
    }

    return true;
}

QString AppIosCredentialStore::Read(const QString& AccountId) const
{
    NSMutableDictionary* Query = MakeQuery(Service, AccountId);
    Query[(id)kSecReturnData] = @YES;
    Query[(id)kSecMatchLimit] = (id)kSecMatchLimitOne;

    CFTypeRef Result = nullptr;
    const OSStatus Status = SecItemCopyMatching((CFDictionaryRef)Query, &Result);

    if (Status != errSecSuccess)
    {
        if (Status != errSecItemNotFound)
        {
            qCWarning(LogApp) << "The Keychain could not read the credential of" << AccountId << ", status" << Status;
        }

        return {};
    }

    const QString Secret{QString::fromUtf8(QByteArray::fromNSData((NSData*)Result))};
    CFRelease(Result);

    return Secret;
}

void AppIosCredentialStore::Remove(const QString& AccountId)
{
    const OSStatus Status = SecItemDelete((CFDictionaryRef)MakeQuery(Service, AccountId));

    if (Status != errSecSuccess && Status != errSecItemNotFound)
    {
        qCWarning(LogApp) << "The Keychain could not remove the credential of" << AccountId << ", status" << Status;
    }
}
