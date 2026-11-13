#include "AppWorkspaceMerger.h"

#include "core/FTCardRepository.h"
#include "core/FTDatabase.h"
#include "core/FTErasure.h"
#include "core/FTJournal.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"

#include <string>
#include <vector>

namespace
{
    struct FTableRule
    {
        const char* Table;
        std::vector<std::string> KeyColumns;
        const char* VersionColumn;
    };

    struct FColumn
    {
        std::string Name;
        std::string Type;
    };

    const std::vector<FTableRule>& GetTableRules()
    {
        static const std::vector<FTableRule> Rules{
            {"Erasures", {"Scope"}, "ErasedDate"},
            {"Cards", {"Id"}, "LastUpdatedDate"},
            {"Journal", {"Id"}, nullptr},
            {"WordForms", {"Id"}, "LastUpdatedDate"},
            {"Placements", {"TargetLanguage"}, "LastUpdatedDate"},
            {"SyncedSettings", {"Key"}, "LastUpdatedDate"},
            {"DailyStats", {"Day", "TargetLanguage", "NativeLanguage", "DeviceId"}, "LastUpdatedDate"},
            {"TriagedWords", {"TargetWord", "TargetLanguage", "PartOfSpeech", "Gender", "MeaningGist"}, nullptr},
            {"SearchHistory", {"Term", "TargetLanguage"}, "SearchedAt"},
            {"CloudBooks", {"Id"}, "LastUpdatedDate"}};

        return Rules;
    }

    std::vector<FColumn> ReadColumns(FTDatabase& Target, const std::string& Table)
    {
        std::vector<FColumn> Columns;

        FTStatement Query = Target.Prepare("PRAGMA table_info(" + Table + ")");

        while (Query.IsValid() && Query.Step())
        {
            Columns.push_back(FColumn{Query.GetText(1), Query.GetText(2)});
        }

        return Columns;
    }

    std::string JoinNames(const std::vector<std::string>& Names, const std::string& Prefix = {})
    {
        std::string Joined;

        for (const std::string& Name : Names)
        {
            if (!Joined.empty())
            {
                Joined += ", ";
            }

            Joined += Prefix + Name;
        }

        return Joined;
    }

    std::string QuoteRow(const std::vector<FColumn>& Columns, const std::string& Prefix)
    {
        std::string Row;

        for (const FColumn& Column : Columns)
        {
            if (!Row.empty())
            {
                Row += " || '|' || ";
            }

            Row += "quote(" + Prefix + Column.Name + ")";
        }

        return "(" + Row + ")";
    }

    bool ApplySettings(FTDatabase& Local, FTSettings& Settings)
    {
        FTStatement Query = Local.Prepare("SELECT Key, Value FROM SyncedSettings");

        if (!Query.IsValid())
        {
            return false;
        }

        std::vector<std::pair<QString, QVariant>> Values;

        while (Query.Step())
        {
            Values.emplace_back(QString::fromStdString(Query.GetText(0)), ParseSettingValue(Query.GetText(1)));
        }

        Settings.ApplySyncedValues(Values);

        return !Query.HasFailed();
    }

    std::string BuildUpsert(const FTableRule& Rule, const std::vector<FColumn>& Columns)
    {
        std::vector<std::string> Names;
        std::vector<std::string> Placeholders;
        std::vector<std::string> Assignments;

        for (size_t Index = 0; Index < Columns.size(); ++Index)
        {
            Names.push_back(Columns[Index].Name);
            Placeholders.push_back("?" + std::to_string(Index + 1));
            Assignments.push_back(Columns[Index].Name + " = excluded." + Columns[Index].Name);
        }

        const std::string Insert = "INSERT INTO " + std::string(Rule.Table) + " (" + JoinNames(Names)
            + ") VALUES (" + JoinNames(Placeholders) + ")";

        if (Rule.VersionColumn == nullptr)
        {
            return Insert + " ON CONFLICT DO NOTHING";
        }

        const std::string Version{Rule.VersionColumn};
        const std::string Table{Rule.Table};

        return Insert + " ON CONFLICT (" + JoinNames(Rule.KeyColumns) + ") DO UPDATE SET " + JoinNames(Assignments)
            + " WHERE excluded." + Version + " > " + Table + "." + Version + " OR (excluded." + Version + " = "
            + Table + "." + Version + " AND " + QuoteRow(Columns, "excluded.") + " > " + QuoteRow(Columns, Table + ".")
            + ")";
    }

    void CopyValue(const FTStatement& Source, FTStatement& Destination, const FColumn& Column, int Index)
    {
        const int Parameter{Index + 1};

        if (Source.IsNull(Index))
        {
            Destination.BindNull(Parameter);
        }
        else if (Column.Type == "INTEGER")
        {
            Destination.BindInt64(Parameter, Source.GetInt64(Index));
        }
        else if (Column.Type == "REAL")
        {
            Destination.BindDouble(Parameter, Source.GetDouble(Index));
        }
        else
        {
            Destination.BindText(Parameter, Source.GetText(Index));
        }
    }

    bool MergeTable(FTDatabase& Local, FTDatabase& Downloaded, const FTableRule& Rule, QString& Error)
    {
        const std::vector<FColumn> Columns = ReadColumns(Local, Rule.Table);

        if (Columns.empty())
        {
            Error = QStringLiteral("this device has no %1 table").arg(QLatin1String(Rule.Table));

            return false;
        }

        std::vector<std::string> Names;

        for (const FColumn& Column : Columns)
        {
            Names.push_back(Column.Name);
        }

        FTStatement Source = Downloaded.Prepare("SELECT " + JoinNames(Names) + " FROM " + Rule.Table);

        if (!Source.IsValid())
        {
            Error = QStringLiteral("the downloaded copy cannot be read from %1").arg(QLatin1String(Rule.Table));

            return false;
        }

        FTStatement Upsert = Local.Prepare(BuildUpsert(Rule, Columns));

        if (!Upsert.IsValid())
        {
            Error = QStringLiteral("%1 cannot be written").arg(QLatin1String(Rule.Table));

            return false;
        }

        while (Source.Step())
        {
            Upsert.Reset();

            for (size_t Index = 0; Index < Columns.size(); ++Index)
            {
                CopyValue(Source, Upsert, Columns[Index], static_cast<int>(Index));
            }

            if (!Upsert.Run())
            {
                Error = QStringLiteral("a row of %1 could not be merged").arg(QLatin1String(Rule.Table));

                return false;
            }
        }

        if (Source.HasFailed())
        {
            Error = QStringLiteral("the downloaded copy stopped reading in %1").arg(QLatin1String(Rule.Table));

            return false;
        }

        return true;
    }

    AppMergeResult Fail(FTDatabase& Local, const QString& Error)
    {
        Local.Rollback();

        qCWarning(LogSync) << "Merge failed:" << Error;

        AppMergeResult Result;
        Result.Error = Error;

        return Result;
    }

    bool PassesIntegrityCheck(FTDatabase& Incoming)
    {
        FTStatement Check{Incoming.Prepare("PRAGMA quick_check")};

        return Check.IsValid() && Check.Step() && Check.GetText(0) == "ok";
    }
}

AppWorkspaceMerger::AppWorkspaceMerger(FTSettings& Settings)
    : Settings{&Settings}
{
}

AppMergeResult AppWorkspaceMerger::Merge(FTDatabase& Local, FTDatabase& Downloaded)
{
    const FTQueryTimer Timer("AppWorkspaceMerger::Merge");

    if (!Local.BeginTransaction())
    {
        AppMergeResult Result;
        Result.Error = QStringLiteral("could not start a transaction");

        return Result;
    }

    QString Error;

    for (const FTableRule& Rule : GetTableRules())
    {
        if (!MergeTable(Local, Downloaded, Rule, Error))
        {
            return Fail(Local, Error);
        }
    }

    if (!RemoveErasedData(Local))
    {
        return Fail(Local, QStringLiteral("erased rows could not be removed"));
    }

    const std::vector<std::pair<QString, QVariant>> PreviousSettings{Settings->ReadSyncedValues()};

    const auto FailAndRestoreSettings = [this, &Local, &PreviousSettings](const QString& Reason)
    {
        Settings->RestoreSyncedValues(PreviousSettings);

        return Fail(Local, Reason);
    };

    if (!ApplySettings(Local, *Settings))
    {
        return FailAndRestoreSettings(QStringLiteral("the merged settings could not be read"));
    }

    AdoptLastStudiedLanguage(FTCardRepository{Local}, *Settings);

    if (!RebuildCardProgress(Local, GetTodayDayNumber()))
    {
        return FailAndRestoreSettings(QStringLiteral("progress could not be rebuilt from the journal"));
    }

    if (!Local.Commit())
    {
        return FailAndRestoreSettings(QStringLiteral("the merge could not be committed"));
    }

    qCInfo(LogSync) << "Merged the downloaded copy";

    AppMergeResult Result;
    Result.bSuccess = true;

    return Result;
}

std::optional<AppIncomingProblem> PrepareIncomingDatabase(FTDatabase& Incoming)
{
    if (!PassesIntegrityCheck(Incoming))
    {
        return AppIncomingProblem{.Reason = QStringLiteral("it is damaged or not a database")};
    }

    const int Version{ReadSchemaVersion(Incoming)};

    if (Version == 0)
    {
        return AppIncomingProblem{.Reason = QStringLiteral("this app never wrote it")};
    }

    if (Version > CurrentSchemaVersion)
    {
        return AppIncomingProblem{.Reason = QStringLiteral("it has schema %1 from a newer app").arg(Version),
                                  .bFromNewerApp = true};
    }

    if (Version < CurrentSchemaVersion && !MigrateSchema(Incoming))
    {
        return AppIncomingProblem{.Reason = QStringLiteral("it could not be brought up from schema %1").arg(Version),
                                  .bRetryLater = true};
    }

    return std::nullopt;
}
