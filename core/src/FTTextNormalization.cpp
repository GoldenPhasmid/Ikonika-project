#include "core/FTTextNormalization.h"

#include <cctype>
#include <unordered_map>

const std::vector<FAccentFold>& AccentFoldTable()
{
    static const std::vector<FAccentFold> Table = {
        {"à", 'a'}, {"á", 'a'}, {"â", 'a'}, {"ã", 'a'}, {"ä", 'a'}, {"å", 'a'},
        {"ç", 'c'}, {"è", 'e'}, {"é", 'e'}, {"ê", 'e'}, {"ë", 'e'}, {"ì", 'i'},
        {"í", 'i'}, {"î", 'i'}, {"ï", 'i'}, {"ñ", 'n'}, {"ò", 'o'}, {"ó", 'o'},
        {"ô", 'o'}, {"õ", 'o'}, {"ö", 'o'}, {"ù", 'u'}, {"ú", 'u'}, {"û", 'u'},
        {"ü", 'u'}, {"ý", 'y'}, {"ÿ", 'y'}, {"À", 'a'}, {"Á", 'a'}, {"Â", 'a'},
        {"Ã", 'a'}, {"Ä", 'a'}, {"Å", 'a'}, {"Ç", 'c'}, {"È", 'e'}, {"É", 'e'},
        {"Ê", 'e'}, {"Ë", 'e'}, {"Ì", 'i'}, {"Í", 'i'}, {"Î", 'i'}, {"Ï", 'i'},
        {"Ñ", 'n'}, {"Ò", 'o'}, {"Ó", 'o'}, {"Ô", 'o'}, {"Õ", 'o'}, {"Ö", 'o'},
        {"Ù", 'u'}, {"Ú", 'u'}, {"Û", 'u'}, {"Ü", 'u'}, {"Ý", 'y'}
    };

    return Table;
}

std::string FoldAccents(const std::string& Value)
{
    static const std::unordered_map<std::string, char> Replacements = []()
    {
        std::unordered_map<std::string, char> Map;

        for (const FAccentFold& Fold : AccentFoldTable())
        {
            Map.emplace(Fold.Utf8, Fold.Base);
        }

        return Map;
    }();

    std::string Result;
    Result.reserve(Value.size());

    for (size_t Index = 0; Index < Value.size();)
    {
        if (Index + 1 < Value.size())
        {
            const auto Found = Replacements.find(Value.substr(Index, 2));

            if (Found != Replacements.end())
            {
                Result.push_back(Found->second);
                Index += 2;

                continue;
            }
        }

        Result.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(Value[Index]))));
        ++Index;
    }

    return Result;
}

std::string FoldCase(const std::string& Value)
{
    constexpr unsigned char LatinLeadByte = 0xC3;
    constexpr unsigned char FirstCapital = 0x80;
    constexpr unsigned char LastCapital = 0x9E;
    constexpr unsigned char MultiplicationSign = 0x97;
    constexpr unsigned char CaseOffset = 0x20;

    std::string Result = Value;

    for (size_t Index = 0; Index < Result.size(); ++Index)
    {
        const unsigned char Byte = static_cast<unsigned char>(Result[Index]);

        if (Byte != LatinLeadByte || Index + 1 == Result.size())
        {
            Result[Index] = static_cast<char>(std::tolower(Byte));

            continue;
        }

        ++Index;

        const unsigned char Second = static_cast<unsigned char>(Result[Index]);

        if (Second >= FirstCapital && Second <= LastCapital && Second != MultiplicationSign)
        {
            Result[Index] = static_cast<char>(Second + CaseOffset);
        }
    }

    return Result;
}
