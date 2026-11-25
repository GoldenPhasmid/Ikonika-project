#include "core/FTTextNormalization.h"

#include <algorithm>
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

namespace
{
    struct FLigature
    {
        const char* Ligature;
        const char* Letters;
    };

    constexpr size_t MaxLigatureSites = 3;

    const std::vector<FLigature>& LigatureTable()
    {
        static const std::vector<FLigature> Table = {
            {"œ", "oe"}, {"Œ", "Oe"}, {"æ", "ae"}, {"Æ", "Ae"}
        };

        return Table;
    }
}

std::string ExpandLigatures(const std::string& Value)
{
    std::string Result = Value;

    for (const FLigature& Entry : LigatureTable())
    {
        const std::string Ligature = Entry.Ligature;

        for (size_t At = Result.find(Ligature); At != std::string::npos; At = Result.find(Ligature, At))
        {
            Result.replace(At, Ligature.size(), Entry.Letters);
        }
    }

    return Result;
}

std::vector<std::string> LigatureSpellings(const std::string& Value)
{
    const std::string Base = ExpandLigatures(Value);

    std::vector<size_t> Sites;

    for (size_t Index = 0; Index + 1 < Base.size() && Sites.size() < MaxLigatureSites; ++Index)
    {
        const int First = std::tolower(static_cast<unsigned char>(Base[Index]));
        const int Second = std::tolower(static_cast<unsigned char>(Base[Index + 1]));

        if ((First == 'o' || First == 'a') && Second == 'e')
        {
            Sites.push_back(Index);
            ++Index;
        }
    }

    std::vector<std::string> Spellings = {Value};

    for (size_t Mask = 0; Mask < (size_t{1} << Sites.size()); ++Mask)
    {
        std::string Spelling;
        size_t SiteIndex = 0;

        for (size_t Index = 0; Index < Base.size(); ++Index)
        {
            const bool bSiteHere = SiteIndex < Sites.size() && Sites[SiteIndex] == Index;

            if (!bSiteHere)
            {
                Spelling.push_back(Base[Index]);

                continue;
            }

            const bool bChosen = (Mask & (size_t{1} << SiteIndex)) != 0;
            const bool bOpen = std::tolower(static_cast<unsigned char>(Base[Index])) == 'o';
            const bool bUpper = std::isupper(static_cast<unsigned char>(Base[Index])) != 0;

            ++SiteIndex;

            if (!bChosen)
            {
                Spelling.push_back(Base[Index]);

                continue;
            }

            Spelling += bOpen ? (bUpper ? "Œ" : "œ") : (bUpper ? "Æ" : "æ");
            ++Index;
        }

        if (std::find(Spellings.begin(), Spellings.end(), Spelling) == Spellings.end())
        {
            Spellings.push_back(std::move(Spelling));
        }
    }

    return Spellings;
}
