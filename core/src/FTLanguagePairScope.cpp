#include "core/FTLanguagePairScope.h"

#include "core/FTDatabase.h"

namespace
{
    constexpr int TargetLanguageParameter = 90;
    constexpr int NativeLanguageParameter = 91;
}

void FTLanguagePairScope::Set(const std::string& InTargetLanguage,
                              const std::string& InNativeLanguage)
{
    TargetLanguage = InTargetLanguage;
    NativeLanguage = InNativeLanguage;
}

const std::string& FTLanguagePairScope::GetTargetLanguage() const
{
    return TargetLanguage;
}

const std::string& FTLanguagePairScope::GetNativeLanguage() const
{
    return NativeLanguage;
}

bool FTLanguagePairScope::IsSet() const
{
    return !TargetLanguage.empty() && !NativeLanguage.empty();
}

std::string FTLanguagePairScope::Clause(const std::string& Table) const
{
    if (!IsSet())
    {
        return "1";
    }

    return Table + ".TargetLanguage = ?90 AND " + Table + ".NativeLanguage = ?91";
}

void FTLanguagePairScope::Bind(FTStatement& Query) const
{
    if (!IsSet())
    {
        return;
    }

    Query.BindText(TargetLanguageParameter, TargetLanguage);
    Query.BindText(NativeLanguageParameter, NativeLanguage);
}
