#pragma once

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTCloudBookRepository.h"
#include "core/FTReaderRepository.h"
#include "core/FTYoutubeRepository.h"

#include <QString>

#include <string>

class AppDatabase
{
public:
    virtual ~AppDatabase() = default;

    static AppDatabase& Get();
    static void SetInstance(AppDatabase* Instance);

    virtual bool Initialise() = 0;

    virtual bool IsReady() = 0;

    virtual FTCardRepository& GetCardRepository() = 0;

    virtual FTReaderRepository& GetReaderLibrary() = 0;

    virtual FTCloudBookRepository& GetCloudBooks() = 0;

    virtual FTYoutubeRepository& GetYoutubeLibrary() = 0;

    virtual void ApplyActiveLanguages() = 0;

    virtual FTDatabase& GetConnection() = 0;

    virtual QString GetFilePath() const = 0;
    virtual QString GetLastError() const = 0;

    static QString ResolveFilePath();

    std::string GetDataRevision();

private:
    static AppDatabase* CurrentInstance;
};
