#include <catch2/catch_test_macros.hpp>

#include "core/FTCloudBookRepository.h"
#include "core/FTDatabase.h"
#include "core/FTReaderRepository.h"
#include "core/FTSchema.h"

#include <memory>
#include <string>

namespace
{
    struct Fixture
    {
        Fixture()
        {
            std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
            REQUIRE(Opened.has_value());

            Target = std::make_unique<FTDatabase>(std::move(*Opened));
            REQUIRE(MigrateSchema(*Target));

            Repository = std::make_unique<FTCloudBookRepository>(*Target, [this]() { return FakeNow; });
        }

        std::unique_ptr<FTDatabase> Target;
        std::unique_ptr<FTCloudBookRepository> Repository;

        int64_t FakeNow = 1000;
    };
}

TEST_CASE("An uploaded book is listed with the place it was read to", "[reader][cloud]")
{
    Fixture Test;

    const std::optional<std::string> Id =
        Test.Repository->AddBook("Le Petit Prince", "epub", 4096, "drive-1", FTReadingPlace{12, 1.5, 3.0, 40.0});

    REQUIRE(Id.has_value());

    const std::vector<FTCloudBookRow> Books = Test.Repository->ListBooks();

    REQUIRE(Books.size() == 1);
    CHECK(Books[0].Title == "Le Petit Prince");
    CHECK(Books[0].Format == "epub");
    CHECK(Books[0].FileSize == 4096);
    CHECK(Books[0].DriveFileId == "drive-1");
    CHECK(Books[0].LastPosition == 12);
    CHECK(Books[0].LastZoom == 1.5);
    CHECK(Books[0].LastScrollY == 40.0);
    CHECK(Books[0].LastUpdatedDate == 1000);
}

TEST_CASE("A book without a Drive file is refused", "[reader][cloud]")
{
    Fixture Test;

    CHECK_FALSE(Test.Repository->AddBook("Empty", "pdf", 1, "", FTReadingPlace{}).has_value());
    CHECK(Test.Repository->ListBooks().empty());
}

TEST_CASE("Reading a cloud book dates the place it was left", "[reader][cloud]")
{
    Fixture Test;

    const std::string Id = Test.Repository->AddBook("Book", "pdf", 1, "drive-1", FTReadingPlace{}).value();

    Test.FakeNow = 5000;
    REQUIRE(Test.Repository->SetReadingPlace(Id, FTReadingPlace{88, 2.0, 0.0, 10.0}));

    const std::optional<FTCloudBookRow> Book = Test.Repository->FindBook(Id);

    REQUIRE(Book.has_value());
    CHECK(Book->LastPosition == 88);
    CHECK(Book->LastZoom == 2.0);
    CHECK(Book->LastOpenedDate == 5000);
    CHECK(Book->LastUpdatedDate == 5000);
}

TEST_CASE("Books are listed most recently opened first", "[reader][cloud]")
{
    Fixture Test;

    const std::string First = Test.Repository->AddBook("First", "pdf", 1, "drive-1", FTReadingPlace{}).value();
    Test.FakeNow = 2000;
    Test.Repository->AddBook("Second", "pdf", 1, "drive-2", FTReadingPlace{});
    Test.FakeNow = 3000;
    REQUIRE(Test.Repository->SetReadingPlace(First, FTReadingPlace{}));

    const std::vector<FTCloudBookRow> Books = Test.Repository->ListBooks();

    REQUIRE(Books.size() == 2);
    CHECK(Books[0].Title == "First");
    CHECK(Books[1].Title == "Second");
}

TEST_CASE("A removed book leaves the list and is reported as deleted", "[reader][cloud]")
{
    Fixture Test;

    const std::string Id = Test.Repository->AddBook("Book", "pdf", 1, "drive-1", FTReadingPlace{}).value();

    Test.FakeNow = 9000;
    REQUIRE(Test.Repository->RemoveBook(Id));

    CHECK(Test.Repository->ListBooks().empty());
    CHECK_FALSE(Test.Repository->FindBook(Id).has_value());
    CHECK(Test.Repository->ListDeletedIds() == std::vector<std::string>{Id});

    REQUIRE(Test.Repository->SetReadingPlace(Id, FTReadingPlace{5}));
    CHECK(Test.Repository->ListBooks().empty());
}

TEST_CASE("A library document remembers the cloud book it is a copy of", "[reader][cloud]")
{
    std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
    REQUIRE(Opened.has_value());
    REQUIRE(MigrateDeviceSchema(*Opened));

    FTReaderRepository Library{*Opened};

    const std::string Id = Library.AddDocument("C:/books/book.pdf", "Book", "pdf").value();
    REQUIRE(Library.SetCloudId(Id, "cloud-1"));

    const std::vector<FTReaderDocumentRow> Documents = Library.ListDocuments();

    REQUIRE(Documents.size() == 1);
    CHECK(Documents[0].CloudId == "cloud-1");
}

TEST_CASE("A device database from before cloud books gains the link column", "[reader][cloud]")
{
    std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
    REQUIRE(Opened.has_value());

    REQUIRE(Opened->Execute(
        "CREATE TABLE ReaderDocuments (Id TEXT PRIMARY KEY NOT NULL, FilePath TEXT NOT NULL, "
        "Title TEXT NOT NULL DEFAULT '', Format TEXT NOT NULL DEFAULT '', AddedDate INTEGER NOT NULL, "
        "LastOpenedDate INTEGER NOT NULL, LastPosition INTEGER NOT NULL DEFAULT 0, LastZoom REAL NOT NULL DEFAULT 0, "
        "LastScrollX REAL NOT NULL DEFAULT 0, LastScrollY REAL NOT NULL DEFAULT 0, Deleted INTEGER NOT NULL DEFAULT 0)"));
    REQUIRE(Opened->Execute(
        "INSERT INTO ReaderDocuments (Id, FilePath, Title, Format, AddedDate, LastOpenedDate) "
        "VALUES ('old', 'C:/old.pdf', 'Old', 'pdf', 1, 1)"));

    REQUIRE(MigrateDeviceSchema(*Opened));

    FTReaderRepository Library{*Opened};
    const std::vector<FTReaderDocumentRow> Documents = Library.ListDocuments();

    REQUIRE(Documents.size() == 1);
    CHECK(Documents[0].Title == "Old");
    CHECK(Documents[0].CloudId.empty());
}

TEST_CASE("A cloud book keeps its length, and a place without one leaves it alone", "[reader][cloud]")
{
    Fixture Test;

    const std::string Id = Test.Repository->AddBook("Book", "pdf", 1, "drive-1", FTReadingPlace{0, 0.0, 0.0, 0.0, 120}).value();

    REQUIRE(Test.Repository->SetReadingPlace(Id, FTReadingPlace{40, 1.0, 0.0, 0.0, 0}));
    CHECK(Test.Repository->FindBook(Id)->PositionCount == 120);
    CHECK(Test.Repository->FindBook(Id)->LastPosition == 40);

    REQUIRE(Test.Repository->SetReadingPlace(Id, FTReadingPlace{41, 1.0, 0.0, 0.0, 130}));
    CHECK(Test.Repository->FindBook(Id)->PositionCount == 130);
}

TEST_CASE("A library document keeps its length and can be pointed at a moved file", "[reader][cloud]")
{
    std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
    REQUIRE(Opened.has_value());
    REQUIRE(MigrateDeviceSchema(*Opened));

    FTReaderRepository Library{*Opened};

    const std::string Id = Library.AddDocument("C:/old/book.pdf", "Book", "pdf").value();
    REQUIRE(Library.SetPositionCount(Id, 88));
    REQUIRE(Library.SetFilePath(Id, "D:/new/book.pdf"));
    CHECK_FALSE(Library.SetFilePath(Id, ""));

    const std::optional<FTReaderDocumentRow> Row = Library.FindDocument(Id);

    REQUIRE(Row.has_value());
    CHECK(Row->PositionCount == 88);
    CHECK(Row->FilePath == "D:/new/book.pdf");
}
