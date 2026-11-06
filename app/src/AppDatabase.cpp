#include "AppDatabase.h"

#include "AppDatabaseService.h"
#include "AppPaths.h"

#include "core/FTSchema.h"
#include "core/FTSettings.h"

AppDatabase* AppDatabase::CurrentInstance = nullptr;

AppDatabase& AppDatabase::Get()
{
    static AppDatabaseService DefaultInstance;

    return CurrentInstance != nullptr ? *CurrentInstance : DefaultInstance;
}

void AppDatabase::SetInstance(AppDatabase* Instance)
{
    CurrentInstance = Instance;
}

QString AppDatabase::ResolveFilePath()
{
    return AppPaths::DatabaseFilePath();
}

std::string AppDatabase::GetDataRevision()
{
    if (!IsReady())
    {
        return {};
    }

    const FTLanguageRules& Languages = FTSettings::Get().GetLanguageRules();

    return GetConnection().GetDataRevision() + "|" + std::to_string(GetTodayDayNumber()) + "|"
        + Languages.TargetLanguage + "|" + Languages.NativeLanguage;
}
