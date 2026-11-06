#include "core/FTBookCatalog.h"

#include "core/FTCatalog.h"
#include "core/FTLog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <algorithm>

namespace
{
    bool IsFileSafeId(const std::string& Id)
    {
        if (Id.empty())
        {
            return false;
        }

        return std::all_of(Id.begin(), Id.end(), [](unsigned char Character)
        {
            return (Character >= 'a' && Character <= 'z') || (Character >= '0' && Character <= '9')
                   || Character == '-';
        });
    }

    std::optional<FTCatalogBook> ReadBook(const QJsonObject& Object)
    {
        FTCatalogBook Book;
        Book.Id = Object.value(QLatin1String("id")).toString().toStdString();
        Book.Title = Object.value(QLatin1String("title")).toString().toStdString();
        Book.Author = Object.value(QLatin1String("author")).toString().toStdString();
        Book.Level = Object.value(QLatin1String("level")).toString().toStdString();
        Book.FileName = Object.value(QLatin1String("file")).toString().toStdString();
        Book.Url = Object.value(QLatin1String("url")).toString().toStdString();
        Book.Sha256 = Object.value(QLatin1String("sha256")).toString().toStdString();
        Book.Bytes = static_cast<std::int64_t>(Object.value(QLatin1String("bytes")).toDouble(0.0));

        if (!IsFileSafeId(Book.Id) || Book.Title.empty() || Book.Url.empty())
        {
            qCWarning(LogUpdate) << "Dropping a catalog book with no title, no url, or an id that is "
                                    "not lowercase letters, digits and dashes:"
                                 << QString::fromStdString(Book.Id);

            return std::nullopt;
        }

        if (Book.Level != "beginner" && Book.Level != "intermediate" && Book.Level != "advanced")
        {
            qCWarning(LogUpdate) << "Dropping catalog book" << QString::fromStdString(Book.Id)
                                 << "because its reading level is missing or unknown:" << QString::fromStdString(Book.Level);

            return std::nullopt;
        }

        if (Book.GetFormat() == EFTDocumentFormat::Unknown)
        {
            qCWarning(LogUpdate) << "Dropping catalog book" << QString::fromStdString(Book.Id)
                                 << "whose file" << QString::fromStdString(Book.FileName)
                                 << "is not a format the reader opens";

            return std::nullopt;
        }

        if (!IsCatalogChecksum(Book.Sha256))
        {
            qCWarning(LogUpdate) << "Dropping catalog book" << QString::fromStdString(Book.Id)
                                 << "because its checksum is not usable, so a download could never "
                                    "be verified.";

            return std::nullopt;
        }

        return Book;
    }
}

EFTDocumentFormat FTCatalogBook::GetFormat() const
{
    return DetectDocumentFormat(FileName);
}

const FTCatalogBook* FTBookCatalog::FindBook(const std::string& Id) const
{
    const auto Found = std::find_if(Books.begin(), Books.end(),
                                    [&Id](const FTCatalogBook& Book) { return Book.Id == Id; });

    return Found == Books.end() ? nullptr : &*Found;
}

std::optional<FTBookCatalog> ParseBookCatalog(const std::string& Json)
{
    QJsonParseError Error{};
    const QJsonDocument Document = QJsonDocument::fromJson(QByteArray::fromStdString(Json), &Error);

    if (Error.error != QJsonParseError::NoError || !Document.isObject())
    {
        qCWarning(LogUpdate) << "Book catalog is not readable JSON:" << Error.errorString();

        return std::nullopt;
    }

    const QJsonObject Root = Document.object();

    FTBookCatalog Catalog;
    Catalog.SchemaVersion = Root.value(QLatin1String("schema")).toInt(0);
    Catalog.TargetLanguage = Root.value(QLatin1String("target")).toString().toStdString();

    if (Catalog.SchemaVersion != BookCatalogSchemaVersion)
    {
        qCWarning(LogUpdate) << "Refusing a book catalog written for schema" << Catalog.SchemaVersion
                             << "when this build reads" << BookCatalogSchemaVersion;

        return std::nullopt;
    }

    if (Catalog.TargetLanguage.empty())
    {
        qCWarning(LogUpdate) << "Refusing a book catalog that names no language";

        return std::nullopt;
    }

    for (const QJsonValue& Entry : Root.value(QLatin1String("books")).toArray())
    {
        if (!Entry.isObject())
        {
            continue;
        }

        std::optional<FTCatalogBook> Book = ReadBook(Entry.toObject());

        if (!Book.has_value())
        {
            continue;
        }

        if (Catalog.FindBook(Book->Id) != nullptr)
        {
            qCWarning(LogUpdate) << "Dropping a second catalog book with the id"
                                 << QString::fromStdString(Book->Id);

            continue;
        }

        Catalog.Books.push_back(std::move(*Book));
    }

    qCInfo(LogUpdate) << "Book catalog read:" << Catalog.Books.size() << "books for"
                      << QString::fromStdString(Catalog.TargetLanguage);

    return Catalog;
}
