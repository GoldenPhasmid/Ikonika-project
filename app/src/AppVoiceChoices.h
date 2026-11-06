#pragma once

#include "AppSpeech.h"

#include "core/FTCatalog.h"

#include <QVariantList>

#include <string>

class AppVoiceChoices
{
public:
    static QVariantList Build(const std::string& TargetLanguage,
                              const AppSpeech& Speech,
                              const FTCatalog* Catalog);

    static std::string DirectoryOf(const FTCatalogPackage& Package);
};
