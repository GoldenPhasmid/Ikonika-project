#pragma once

#include "core/FTWebSearch.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct FTDictionaryPackage;

inline constexpr int CatalogSchemaVersion = 1;

inline constexpr const char* CatalogKindDictionary = "dictionary";
inline constexpr const char* CatalogKindVoice = "voice";
inline constexpr const char* CatalogKindTranslation = "translation";

struct FTCatalogPackage
{
    std::string Id;
    std::string Kind;
    std::string TargetLanguage;
    std::vector<std::string> NativeLanguages;

    int Version = 0;
    int DictionarySchema = 0;

    std::string FileName;
    std::string UnpackTo;
    std::string Url;

    std::int64_t Bytes = 0;
    std::int64_t UnpackedBytes = 0;

    std::string Sha256;

    bool bDesktopOnly = false;

    bool IsCompatible() const;
    bool ServesTarget(const std::string& LanguageCode) const;
    bool SupportsNative(const std::string& LanguageCode) const;
    bool Replaces(const FTDictionaryPackage& Installed) const;
};

struct FTCatalogApp
{
    std::string Version;
    std::string FileName;
    std::string Url;
    std::int64_t Bytes = 0;
    std::string Sha256;
    std::vector<std::string> SilentArguments;
};

struct FTCatalog
{
    int SchemaVersion = 0;
    std::optional<FTCatalogApp> App;
    std::vector<FTCatalogPackage> Packages;
    std::vector<FTWebSearchSource> WebSearchSources;
    std::map<std::string, std::string> BookCatalogUrls;

    const FTCatalogPackage* FindCompatibleDictionary(const std::string& TargetLanguage,
                                                     const std::string& NativeLanguage) const;
};

std::optional<FTCatalog> ParseCatalog(const std::string& Json);

bool IsCatalogChecksum(const std::string& Value);

bool IsNewerVersion(const std::string& Installed, const std::string& Published);
