#include "AppReadSources.h"

#include "AppDatabase.h"
#include "AppDictionary.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

std::optional<FReadSourcesRequest> AppReadSources::Capture()
{
    if (!AppDatabase::Get().IsReady() || !AppDictionary::Get().IsReady())
    {
        return std::nullopt;
    }

    const FTDictionaryPackage* Package = AppDictionary::Get().GetActivePackage();
    const QString UserPath = AppDatabase::Get().GetFilePath();

    if (Package == nullptr || Package->FilePath.empty() || UserPath.isEmpty() || UserPath.contains(QStringLiteral(":memory:")))
    {
        return std::nullopt;
    }

    const FTLanguageRules& Languages = FTSettings::Get().GetLanguageRules();

    return FReadSourcesRequest{UserPath.toStdString(), Package->FilePath, Languages.TargetLanguage,
                               Languages.NativeLanguage};
}

std::unique_ptr<AppReadSources> AppReadSources::Open(const FReadSourcesRequest& Request)
{
    std::unique_ptr<AppReadSources> Sources{new AppReadSources};

    Sources->UserConnection = FTDatabase::OpenReadOnly(Request.UserDatabasePath);
    Sources->DictionaryConnection = FTDatabase::OpenReadOnly(Request.DictionaryPath);

    if (!Sources->UserConnection.has_value() || !Sources->DictionaryConnection.has_value())
    {
        qCWarning(LogDatabase) << "Could not open read-only connections for a background task";

        return nullptr;
    }

    Sources->Cards = std::make_unique<FTCardRepository>(*Sources->UserConnection);
    Sources->Cards->SetActiveLanguages(Request.TargetLanguage, Request.NativeLanguage);
    Sources->Dictionary = std::make_unique<FTDictionaryRepository>(*Sources->DictionaryConnection, Request.NativeLanguage);

    return Sources;
}

FTDatabase& AppReadSources::GetDictionaryConnection()
{
    return *DictionaryConnection;
}

const FTDictionaryRepository& AppReadSources::GetDictionary() const
{
    return *Dictionary;
}

const FTCardRepository& AppReadSources::GetCards() const
{
    return *Cards;
}
