#pragma once

#include <map>
#include <string>
#include <vector>

inline constexpr const char* DefaultWebSearchSourceId = "reverso";

struct FTWebSearchSource
{
    std::string Id;
    std::string Name;
    std::string UrlTemplate;
    std::map<std::string, std::string> LanguageNames;

    bool ServesPair(const std::string& TargetLanguage, const std::string& NativeLanguage) const;

    std::string BuildUrl(const std::string& TargetLanguage, const std::string& NativeLanguage,
                         const std::string& Word) const;
};

std::vector<FTWebSearchSource> GetBuiltInWebSearchSources();

std::vector<FTWebSearchSource> SelectWebSearchSourcesForPair(
    const std::vector<FTWebSearchSource>& Sources, const std::string& TargetLanguage,
    const std::string& NativeLanguage);

int FindWebSearchSourceIndex(const std::vector<FTWebSearchSource>& Sources,
                             const std::string& PreferredId);
