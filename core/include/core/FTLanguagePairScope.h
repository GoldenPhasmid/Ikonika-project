#pragma once

#include <string>

class FTStatement;

class FTLanguagePairScope
{
public:
    void Set(const std::string& InTargetLanguage, const std::string& InNativeLanguage);

    const std::string& GetTargetLanguage() const;
    const std::string& GetNativeLanguage() const;

    bool IsSet() const;

    std::string Clause(const std::string& Table) const;

    void Bind(FTStatement& Query) const;

private:
    std::string TargetLanguage;
    std::string NativeLanguage;
};
