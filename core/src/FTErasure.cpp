#include "core/FTErasure.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"

#include <string>
#include <vector>

namespace
{
    struct FErasedTable
    {
        const char* Table;
        const char* DateColumn;
    };

    struct FErasure
    {
        std::string TargetLanguage;
        int64_t ErasedDate{0};
    };

    constexpr FErasedTable ErasedTables[]{
        {"Cards", "LastUpdatedDate"},
        {"WordForms", "LastUpdatedDate"},
        {"Placements", "LastUpdatedDate"},
        {"DailyStats", "LastUpdatedDate"},
        {"TriagedWords", "MarkedDate"},
        {"SearchHistory", "SearchedAt"}};

    const std::string EverythingScope{"everything"};
    const std::string LanguageScopePrefix{"language:"};

    std::optional<std::vector<FErasure>> ReadErasures(FTDatabase& Target)
    {
        FTStatement Query = Target.Prepare("SELECT Scope, ErasedDate FROM Erasures");

        if (!Query.IsValid())
        {
            return std::nullopt;
        }

        std::vector<FErasure> Erasures;

        while (Query.Step())
        {
            const std::string Scope = Query.GetText(0);

            if (Scope == EverythingScope)
            {
                Erasures.push_back(FErasure{"", Query.GetInt64(1)});
            }
            else if (Scope.rfind(LanguageScopePrefix, 0) == 0 && Scope.size() > LanguageScopePrefix.size())
            {
                Erasures.push_back(FErasure{Scope.substr(LanguageScopePrefix.size()), Query.GetInt64(1)});
            }
            else
            {
                qCWarning(LogDatabase) << "Skipping an erasure of unknown scope" << Scope.c_str();
            }
        }

        return Erasures;
    }

    bool RemoveRowsBefore(FTDatabase& Target, const FErasure& Erasure)
    {
        const bool bOneLanguage = !Erasure.TargetLanguage.empty();

        for (const FErasedTable& Erased : ErasedTables)
        {
            FTStatement Delete = Target.Prepare(std::string("DELETE FROM ") + Erased.Table + " WHERE "
                                                + Erased.DateColumn + " < ?1"
                                                + (bOneLanguage ? " AND TargetLanguage = ?2" : ""));

            if (!Delete.IsValid())
            {
                return false;
            }

            Delete.BindInt64(1, Erasure.ErasedDate);

            if (bOneLanguage)
            {
                Delete.BindText(2, Erasure.TargetLanguage);
            }

            if (!Delete.Run())
            {
                qCWarning(LogDatabase) << "Could not remove erased rows from" << Erased.Table;

                return false;
            }
        }

        return true;
    }
}

std::string MakeLanguageErasureScope(const std::string& TargetLanguage)
{
    return LanguageScopePrefix + TargetLanguage;
}

std::optional<int64_t> ReadErasedDate(FTDatabase& Target)
{
    FTStatement Query = Target.Prepare("SELECT ErasedDate FROM Erasures WHERE Scope = 'everything'");

    if (!Query.IsValid() || !Query.Step())
    {
        return std::nullopt;
    }

    return Query.GetInt64(0);
}

bool RemoveErasedData(FTDatabase& Target)
{
    const FTQueryTimer Timer("RemoveErasedData");

    const std::optional<std::vector<FErasure>> Erasures = ReadErasures(Target);

    if (!Erasures.has_value())
    {
        return false;
    }

    for (const FErasure& Erasure : *Erasures)
    {
        if (!RemoveRowsBefore(Target, Erasure))
        {
            return false;
        }
    }

    return Target.Execute("DELETE FROM Journal WHERE CardId NOT IN (SELECT Id FROM Cards)");
}
