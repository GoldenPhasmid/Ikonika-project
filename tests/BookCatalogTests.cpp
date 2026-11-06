#include <catch2/catch_test_macros.hpp>

#include "core/FTBookCatalog.h"
#include "core/FTCatalog.h"

#include <string>

namespace
{
    const std::string GoodHash = std::string(64, 'a');

    std::string BookFields(const std::string& Id = "segur-les-malheurs-de-sophie",
                           const std::string& FileName = "segur-les-malheurs-de-sophie.epub",
                           const std::string& Hash = GoodHash,
                           const std::string& Level = R"("beginner")")
    {
        return R"("id":")" + Id + R"(","title":"Les Malheurs de Sophie","author":"Comtesse de Ségur",)"
               R"("file":")" + FileName + R"(","url":"https://example.invalid/)" + FileName + R"(",)"
               R"("bytes":160983,"sha256":")" + Hash + R"(")"
               + (Level.empty() ? "" : R"(,"level":)" + Level);
    }

    std::string Books(const std::string& Entries, const std::string& Target = "fr")
    {
        return R"({"schema":1,"target":")" + Target + R"(","books":[)" + Entries + "]}";
    }
}

TEST_CASE("A book catalog for an unknown schema or no language is refused whole", "[catalog]")
{
    REQUIRE_FALSE(ParseBookCatalog(R"({"schema":2,"target":"fr","books":[]})").has_value());
    REQUIRE_FALSE(ParseBookCatalog(R"({"schema":1,"books":[]})").has_value());
    REQUIRE_FALSE(ParseBookCatalog("not json").has_value());
}

TEST_CASE("A catalog book is read field for field", "[catalog]")
{
    const std::optional<FTBookCatalog> Catalog = ParseBookCatalog(Books("{" + BookFields() + "}"));

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->TargetLanguage == "fr");
    REQUIRE(Catalog->Books.size() == 1);

    const FTCatalogBook& Book = Catalog->Books.front();

    REQUIRE(Book.Id == "segur-les-malheurs-de-sophie");
    REQUIRE(Book.Title == "Les Malheurs de Sophie");
    REQUIRE(Book.Author == "Comtesse de Ségur");
    REQUIRE(Book.Level == "beginner");
    REQUIRE(Book.Url == "https://example.invalid/segur-les-malheurs-de-sophie.epub");
    REQUIRE(Book.Bytes == 160983);
    REQUIRE(Book.GetFormat() == EFTDocumentFormat::Epub);
    REQUIRE(Catalog->FindBook("segur-les-malheurs-de-sophie") == &Book);
    REQUIRE(Catalog->FindBook("candide") == nullptr);
}

TEST_CASE("A book that could not be downloaded or opened is dropped, not the catalog", "[catalog]")
{
    const std::string Entries = "{" + BookFields("Bad Id") + "},"
                                "{" + BookFields("unreadable", "unreadable.docx") + "},"
                                "{" + BookFields("unverifiable", "unverifiable.epub", "abc") + "},"
                                "{" + BookFields("candide", "candide.epub") + "}";

    const std::optional<FTBookCatalog> Catalog = ParseBookCatalog(Books(Entries));

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->Books.size() == 1);
    REQUIRE(Catalog->Books.front().Id == "candide");
}

TEST_CASE("A second book under the same id is dropped", "[catalog]")
{
    const std::optional<FTBookCatalog> Catalog =
        ParseBookCatalog(Books("{" + BookFields() + "},{" + BookFields() + "}"));

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->Books.size() == 1);
}

TEST_CASE("A catalog book requires a recognized reading level", "[catalog][book-level]")
{
    for (const std::string Level : {"", "null", "7", R"("")", R"("classic")", R"("Beginner")"})
    {
        const std::optional<FTBookCatalog> Catalog =
            ParseBookCatalog(Books("{" + BookFields("invalid-level", "book.epub", GoodHash, Level) + "}"));

        REQUIRE(Catalog.has_value());
        REQUIRE(Catalog->Books.empty());
    }
}

TEST_CASE("Each catalog reading level is preserved", "[catalog][book-level]")
{
    for (const std::string Level : {"beginner", "intermediate", "advanced"})
    {
        const std::optional<FTBookCatalog> Catalog =
            ParseBookCatalog(Books("{" + BookFields("book", "book.epub", GoodHash, "\"" + Level + "\"") + "}"));

        REQUIRE(Catalog.has_value());
        REQUIRE(Catalog->Books.size() == 1);
        REQUIRE(Catalog->Books.front().Level == Level);
    }
}

TEST_CASE("The catalog points each language at its book catalog over https only", "[catalog]")
{
    const std::optional<FTCatalog> Catalog = ParseCatalog(
        R"({"schema":1,"packages":[],"books":{"fr":"https://example.invalid/books-fr.json",)"
        R"("es":"http://example.invalid/books-es.json","en":7}})");

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->BookCatalogUrls.size() == 1);
    REQUIRE(Catalog->BookCatalogUrls.at("fr") == "https://example.invalid/books-fr.json");
}

TEST_CASE("A catalog without book catalogs still reads", "[catalog]")
{
    const std::optional<FTCatalog> Catalog = ParseCatalog(R"({"schema":1,"packages":[]})");

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->BookCatalogUrls.empty());
}
