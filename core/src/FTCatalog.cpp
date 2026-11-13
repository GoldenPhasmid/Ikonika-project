#include "core/FTCatalog.h"

#include "core/FTDictionaryPackage.h"
#include "core/FTLog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <algorithm>

bool IsCatalogChecksum(const std::string& Value)
{
    if (Value.size() != 64)
    {
        return false;
    }

    return std::all_of(Value.begin(), Value.end(), [](unsigned char Character)
    {
        return (Character >= '0' && Character <= '9') || (Character >= 'a' && Character <= 'f');
    });
}

namespace
{
    std::string ReadString(const QJsonObject& Object, const char* Key)
    {
        const QJsonValue Value = Object.value(QLatin1String(Key));

        if (!Value.isString())
        {
            return {};
        }

        return Value.toString().toStdString();
    }

    std::vector<std::string> ReadStringList(const QJsonObject& Object, const char* Key)
    {
        std::vector<std::string> Result;

        for (const QJsonValue& Entry : Object.value(QLatin1String(Key)).toArray())
        {
            if (Entry.isString())
            {
                Result.push_back(Entry.toString().toStdString());
            }
        }

        return Result;
    }

    std::optional<FTCatalogApp> ReadApp(const QJsonObject& Object)
    {
        FTCatalogApp App;
        App.Version = ReadString(Object, "version");
        App.FileName = ReadString(Object, "file");
        App.Url = ReadString(Object, "url");
        App.Sha256 = ReadString(Object, "sha256");
        App.SilentArguments = ReadStringList(Object, "silentArguments");
        App.Bytes = static_cast<std::int64_t>(Object.value(QLatin1String("bytes")).toDouble(0.0));

        if (App.Version.empty() || App.Url.empty())
        {
            qCWarning(LogUpdate) << "Catalog app entry has no version or no url. Ignoring it.";

            return std::nullopt;
        }

        if (!IsCatalogChecksum(App.Sha256))
        {
            qCWarning(LogUpdate) << "Catalog app entry"
                                 << QString::fromStdString(App.Version)
                                 << "has no usable checksum. Ignoring it, since a download could "
                                    "never be verified.";

            return std::nullopt;
        }

        return App;
    }

    std::optional<FTCatalogPackage> ReadPackage(const QJsonObject& Object)
    {
        FTCatalogPackage Package;
        Package.Id = ReadString(Object, "id");
        Package.Kind = ReadString(Object, "kind");
        Package.TargetLanguage = ReadString(Object, "target");
        Package.NativeLanguages = ReadStringList(Object, "natives");
        Package.FileName = ReadString(Object, "file");
        Package.UnpackTo = ReadString(Object, "unpackTo");
        Package.Url = ReadString(Object, "url");
        Package.Sha256 = ReadString(Object, "sha256");
        Package.Version = Object.value(QLatin1String("version")).toInt(0);
        Package.DictionarySchema = Object.value(QLatin1String("dictionarySchema")).toInt(0);
        Package.Bytes = static_cast<std::int64_t>(Object.value(QLatin1String("bytes")).toDouble(0.0));
        Package.UnpackedBytes =
            static_cast<std::int64_t>(Object.value(QLatin1String("unpackedBytes")).toDouble(0.0));
        Package.bDesktopOnly = Object.value(QLatin1String("desktopOnly")).toBool(false);

        if (Package.Id.empty() || Package.Url.empty() || Package.Version <= 0)
        {
            qCWarning(LogUpdate) << "Dropping a catalog package with no id, no url, or no version.";

            return std::nullopt;
        }

        const bool bKnownKind = Package.Kind == CatalogKindDictionary ||
                                Package.Kind == CatalogKindVoice ||
                                Package.Kind == CatalogKindTranslation;

        if (!bKnownKind)
        {
            qCWarning(LogUpdate) << "Dropping catalog package"
                                 << QString::fromStdString(Package.Id)
                                 << "of unknown kind" << QString::fromStdString(Package.Kind);

            return std::nullopt;
        }

        if (!IsCatalogChecksum(Package.Sha256))
        {
            qCWarning(LogUpdate) << "Dropping catalog package"
                                 << QString::fromStdString(Package.Id)
                                 << "because its checksum is not usable, so a download could never "
                                    "be verified.";

            return std::nullopt;
        }

        return Package;
    }

    std::optional<FTWebSearchSource> ReadWebSearchSource(const QJsonObject& Object)
    {
        FTWebSearchSource Source;
        Source.Id = ReadString(Object, "id");
        Source.Name = ReadString(Object, "name");
        Source.UrlTemplate = ReadString(Object, "url");

        const QJsonObject Languages = Object.value(QLatin1String("languages")).toObject();

        for (auto Entry = Languages.begin(); Entry != Languages.end(); ++Entry)
        {
            if (Entry.value().isString() && !Entry.value().toString().isEmpty())
            {
                Source.LanguageNames[Entry.key().toStdString()] = Entry.value().toString().toStdString();
            }
        }

        const QString Url{QString::fromStdString(Source.UrlTemplate)};
        const bool bUsableUrl = Url.startsWith(QLatin1String("https://"))
                                && Url.contains(QLatin1String("{word}"));

        if (Source.Id.empty() || Source.Name.empty() || !bUsableUrl || Source.LanguageNames.empty())
        {
            qCWarning(LogUpdate) << "Dropping catalog web search source"
                                 << QString::fromStdString(Source.Id)
                                 << "with no name, no languages, or an address that is not https "
                                    "or has no place for the word.";

            return std::nullopt;
        }

        return Source;
    }

    std::vector<int> SplitVersion(const std::string& Version)
    {
        std::vector<int> Parts;
        std::string Current;

        const auto Flush = [&Parts, &Current]() -> bool
        {
            if (Current.empty())
            {
                return false;
            }

            try
            {
                Parts.push_back(std::stoi(Current));
            }
            catch (...)
            {
                return false;
            }

            Current.clear();

            return true;
        };

        for (const char Character : Version)
        {
            if (Character == '.')
            {
                if (!Flush())
                {
                    return {};
                }

                continue;
            }

            if (Character < '0' || Character > '9')
            {
                return {};
            }

            Current.push_back(Character);
        }

        if (!Flush())
        {
            return {};
        }

        return Parts;
    }
}

bool FTCatalogPackage::IsCompatible() const
{
    if (Kind != CatalogKindDictionary)
    {
        return true;
    }

    return IsReadableDictionarySchema(DictionarySchema);
}

const FTCatalogPackage* FTCatalog::FindCompatibleDictionary(const std::string& TargetLanguage,
                                                            const std::string& NativeLanguage) const
{
    const FTCatalogPackage* Best = nullptr;

    for (const FTCatalogPackage& Package : Packages)
    {
        if (Package.Kind != CatalogKindDictionary || Package.TargetLanguage != TargetLanguage
            || !Package.SupportsNative(NativeLanguage) || !Package.IsCompatible())
        {
            continue;
        }

        if (Best == nullptr || Package.DictionarySchema > Best->DictionarySchema)
        {
            Best = &Package;
        }
    }

    return Best;
}

bool FTCatalogPackage::ServesTarget(const std::string& LanguageCode) const
{
    if (TargetLanguage.empty())
    {
        return true;
    }

    return TargetLanguage == LanguageCode;
}

bool FTCatalogPackage::SupportsNative(const std::string& LanguageCode) const
{
    if (NativeLanguages.empty())
    {
        return true;
    }

    return std::find(NativeLanguages.begin(), NativeLanguages.end(), LanguageCode) !=
           NativeLanguages.end();
}

bool FTCatalogPackage::Replaces(const FTDictionaryPackage& Installed) const
{
    if (Kind != CatalogKindDictionary || TargetLanguage != Installed.TargetLanguage)
    {
        return false;
    }

    return std::any_of(Installed.NativeLanguages.begin(), Installed.NativeLanguages.end(),
                       [this](const std::string& Native)
                       {
                           return SupportsNative(Native);
                       });
}

std::optional<FTCatalog> ParseCatalog(const std::string& Json)
{
    QJsonParseError Error{};
    const QJsonDocument Document =
        QJsonDocument::fromJson(QByteArray::fromStdString(Json), &Error);

    if (Error.error != QJsonParseError::NoError || !Document.isObject())
    {
        qCWarning(LogUpdate) << "Catalog is not readable JSON:" << Error.errorString();

        return std::nullopt;
    }

    const QJsonObject Root = Document.object();

    FTCatalog Catalog;
    Catalog.SchemaVersion = Root.value(QLatin1String("schema")).toInt(0);

    if (Catalog.SchemaVersion != CatalogSchemaVersion)
    {
        qCWarning(LogUpdate) << "Refusing a catalog written for schema" << Catalog.SchemaVersion
                             << "when this build reads" << CatalogSchemaVersion;

        return std::nullopt;
    }

    const QJsonValue AppValue = Root.value(QLatin1String("app"));

    if (AppValue.isObject())
    {
        Catalog.App = ReadApp(AppValue.toObject());
    }

    for (const QJsonValue& Entry : Root.value(QLatin1String("packages")).toArray())
    {
        if (!Entry.isObject())
        {
            continue;
        }

        if (std::optional<FTCatalogPackage> Package = ReadPackage(Entry.toObject()))
        {
            Catalog.Packages.push_back(*Package);
        }
    }

    for (const QJsonValue& Entry : Root.value(QLatin1String("webSearch")).toArray())
    {
        if (!Entry.isObject())
        {
            continue;
        }

        if (std::optional<FTWebSearchSource> Source = ReadWebSearchSource(Entry.toObject()))
        {
            Catalog.WebSearchSources.push_back(*Source);
        }
    }

    const QJsonObject Books = Root.value(QLatin1String("books")).toObject();

    for (auto Entry = Books.begin(); Entry != Books.end(); ++Entry)
    {
        const QString Url = Entry.value().toString();

        if (!Url.startsWith(QLatin1String("https://")))
        {
            qCWarning(LogUpdate) << "Dropping the book catalog for" << Entry.key()
                                 << "because its address is not https";

            continue;
        }

        Catalog.BookCatalogUrls[Entry.key().toStdString()] = Url.toStdString();
    }

    qCInfo(LogUpdate) << "Catalog read:" << Catalog.Packages.size() << "packages,"
                      << Catalog.BookCatalogUrls.size() << "book catalogs,"
                      << Catalog.WebSearchSources.size() << "web search sources, app version"
                      << (Catalog.App ? QString::fromStdString(Catalog.App->Version)
                                      : QStringLiteral("none"));

    return Catalog;
}

bool IsNewerVersion(const std::string& Installed, const std::string& Published)
{
    const std::vector<int> Left = SplitVersion(Installed);
    const std::vector<int> Right = SplitVersion(Published);

    if (Right.empty())
    {
        return false;
    }

    if (Left.empty())
    {
        return true;
    }

    const std::size_t Count = std::max(Left.size(), Right.size());

    for (std::size_t Index = 0; Index < Count; ++Index)
    {
        const int LeftPart = Index < Left.size() ? Left[Index] : 0;
        const int RightPart = Index < Right.size() ? Right[Index] : 0;

        if (RightPart != LeftPart)
        {
            return RightPart > LeftPart;
        }
    }

    return false;
}
