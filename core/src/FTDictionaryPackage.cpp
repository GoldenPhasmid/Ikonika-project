#include "core/FTDictionaryPackage.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cstdlib>

std::string ReadDictionaryMetadata(FTDatabase& Target, const std::string& Key)
{
    FTStatement Query = Target.Prepare("SELECT value FROM metadata WHERE key = ?1");

    if (!Query.IsValid())
    {
        return {};
    }

    Query.BindText(1, Key);

    if (!Query.Step())
    {
        return {};
    }

    return Query.GetText(0);
}

namespace
{
    std::vector<std::string> ReadDistinctLanguages(FTDatabase& Target, const char* Sql)
    {
        std::vector<std::string> Codes;

        FTStatement Query = Target.Prepare(Sql);

        if (!Query.IsValid())
        {
            return Codes;
        }

        while (Query.Step())
        {
            Codes.push_back(Query.GetText(0));
        }

        return Codes;
    }

    int ReadWordCount(FTDatabase& Target)
    {
        FTStatement Query = Target.Prepare("SELECT COUNT(*) FROM words");

        if (!Query.IsValid() || !Query.Step())
        {
            return 0;
        }

        return Query.GetInt(0);
    }

    /** The language a file teaches, taken from its own name. The last resort: a dictionary with no
        metadata table and no examples still has to be identifiable. */
    std::string TargetLanguageFromFileName(const std::string& FilePath)
    {
        const QString Name = QFileInfo(QString::fromStdString(FilePath)).fileName();
        const QString Suffix = QString::fromUtf8(DictionaryFileSuffix);

        if (!Name.endsWith(Suffix, Qt::CaseInsensitive))
        {
            return {};
        }

        return Name.left(Name.size() - Suffix.size()).toLower().toStdString();
    }
}

bool FTDictionaryPackage::SupportsNative(const std::string& LanguageCode) const
{
    return std::find(NativeLanguages.begin(), NativeLanguages.end(), LanguageCode)
        != NativeLanguages.end();
}

bool FTDictionaryPackage::SharesPairWith(const FTDictionaryPackage& Other) const
{
    if (TargetLanguage != Other.TargetLanguage)
    {
        return false;
    }

    return std::any_of(NativeLanguages.begin(), NativeLanguages.end(),
                       [&Other](const std::string& Native)
                       {
                           return Other.SupportsNative(Native);
                       });
}

bool IsReadableDictionarySchema(int SchemaVersion)
{
    return SchemaVersion >= OldestDictionarySchemaVersion && SchemaVersion <= DictionarySchemaVersion;
}

bool FTDictionaryPackage::IsCompatible() const
{
    return IsReadableDictionarySchema(SchemaVersion);
}

std::optional<FTDictionaryPackage> ReadDictionaryPackage(const std::string& FilePath)
{
    const FTQueryTimer Timer("ReadDictionaryPackage");

    std::optional<FTDatabase> Opened = FTDatabase::OpenReadOnly(FilePath);

    if (!Opened.has_value())
    {
        return std::nullopt;
    }

    FTDictionaryPackage Result;
    Result.FilePath = FilePath;

    // The metadata table is the cheap path and the authoritative one. Everything below it exists so
    // that dictionaries built before that table was added still identify themselves.
    const std::string StoredVersion = ReadDictionaryMetadata(*Opened, "schema_version");

    if (!StoredVersion.empty())
    {
        Result.SchemaVersion = std::atoi(StoredVersion.c_str());
    }

    Result.TargetLanguage = ReadDictionaryMetadata(*Opened, "target_language");

    if (Result.TargetLanguage.empty())
    {
        // Examples are written in the language being learned, so they say what it is. A full scan
        // of one column, which costs a few milliseconds on a large file and only happens for a
        // dictionary with no metadata.
        const std::vector<std::string> ExampleLanguages = ReadDistinctLanguages(*Opened,
            "SELECT DISTINCT l.code FROM examples e JOIN languages l ON l.id = e.language_id");

        if (ExampleLanguages.size() == 1)
        {
            Result.TargetLanguage = ExampleLanguages.front();
        }
    }

    if (Result.TargetLanguage.empty())
    {
        Result.TargetLanguage = TargetLanguageFromFileName(FilePath);
    }

    if (Result.TargetLanguage.empty())
    {
        qCWarning(LogDatabase) << "Ignoring a dictionary that does not say what language it teaches:"
                               << QString::fromStdString(FilePath);

        return std::nullopt;
    }

    Result.NativeLanguages = ReadDistinctLanguages(*Opened,
        "SELECT DISTINCT l.code FROM translations t JOIN languages l ON l.id = t.language_id");

    // Sorted so two scans of the same directory report the same order, whatever the row order was.
    std::sort(Result.NativeLanguages.begin(), Result.NativeLanguages.end());

    // A dictionary that translates into nothing cannot be learned from, whatever else it holds.
    if (Result.NativeLanguages.empty())
    {
        qCWarning(LogDatabase) << "Ignoring a dictionary with no translations:"
                               << QString::fromStdString(FilePath);

        return std::nullopt;
    }

    Result.WordCount = ReadWordCount(*Opened);

    if (Result.WordCount == 0)
    {
        qCWarning(LogDatabase) << "Ignoring an empty dictionary:"
                               << QString::fromStdString(FilePath);

        return std::nullopt;
    }

    return Result;
}

std::vector<FTDictionaryPackage> FindDictionaryPackages(const std::string& Directory,
                                                        std::vector<FTDictionaryPackage>* OutIncompatible)
{
    std::vector<FTDictionaryPackage> Found;

    const QDir Root(QString::fromStdString(Directory));

    if (!Root.exists())
    {
        return Found;
    }

    const QStringList Pattern{QStringLiteral("*") + QString::fromUtf8(DictionaryFileSuffix)};

    // This directory, then one level down. Deeper than that is not searched: a dictionary buried
    // three levels deep is more likely a stray copy than something meant to be loaded.
    QStringList Directories{Root.absolutePath()};

    for (const QString& Child : Root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        Directories.append(Root.absoluteFilePath(Child));
    }

    for (const QString& Path : Directories)
    {
        const QDir Current(Path);

        for (const QString& File : Current.entryList(Pattern, QDir::Files, QDir::Name))
        {
            const std::string FilePath = Current.absoluteFilePath(File).toStdString();

            std::optional<FTDictionaryPackage> Package = ReadDictionaryPackage(FilePath);

            if (!Package.has_value())
            {
                continue;
            }

            // Before the language is claimed, not after. A stale build sorting earlier than a
            // current one used to take the language and then be thrown out for its schema, which
            // left nothing: data/built held fr at version 3 and fr-en at 6, and the app loaded
            // neither.
            if (!Package->IsCompatible())
            {
                qCWarning(LogDatabase)
                    << "Ignoring a dictionary built for a different schema:"
                    << QString::fromStdString(Package->FilePath)
                    << "declares schema" << Package->SchemaVersion
                    << "and this build reads" << OldestDictionarySchemaVersion << "to"
                    << DictionarySchemaVersion
                    << "- rebuild it, see docs/dictionary-pipeline.md";

                if (OutIncompatible != nullptr)
                {
                    OutIncompatible->push_back(std::move(*Package));
                }

                continue;
            }

            // The same language found twice means two files claim it, in which case the first
            // directory searched wins. Which is why the caller's search order is what decides
            // whether an installed package or a build tree takes precedence.
            const auto Existing = std::find_if(Found.begin(), Found.end(),
                [&Package](const FTDictionaryPackage& Other)
                {
                    return Other.SharesPairWith(*Package);
                });

            if (Existing != Found.end())
            {
                continue;
            }

            Found.push_back(std::move(*Package));
        }
    }

    return Found;
}
