#include <catch2/catch_test_macros.hpp>

#include "core/FTDatabase.h"
#include "core/FTReaderRepository.h"
#include "core/FTSchema.h"

#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include <string>
#include <vector>

namespace
{
    /** A database and reader repository, migrated to the current schema. */
    struct Fixture
    {
        Fixture()
        {
            std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
            REQUIRE(Opened.has_value());

            Target = std::make_unique<FTDatabase>(std::move(*Opened));
            REQUIRE(MigrateDeviceSchema(*Target));

            Repository = std::make_unique<FTReaderRepository>(*Target, [this]() { return FakeNow; });
        }

        std::unique_ptr<FTDatabase> Target;
        std::unique_ptr<FTReaderRepository> Repository;

        int64_t FakeNow = 1000;
    };

    /** A real file that exists on disk, so the exists flag has something to check. */
    std::string MakeExistingFile(QTemporaryDir& Dir, const char* Name)
    {
        const QString Path = Dir.filePath(QString::fromLatin1(Name));

        QFile File(Path);

        if (!File.open(QIODevice::WriteOnly))
        {
            return {};
        }

        File.write("hello");
        File.close();

        return Path.toStdString();
    }
}

TEST_CASE("Adding a document records it in the library", "[reader][repository]")
{
    Fixture Test;
    QTemporaryDir Dir;

    const std::string Path = MakeExistingFile(Dir, "book.txt");
    REQUIRE_FALSE(Path.empty());

    const std::optional<std::string> Id = Test.Repository->AddDocument(Path, "My Book", "TXT");
    REQUIRE(Id.has_value());

    const std::vector<FTReaderDocumentRow> List = Test.Repository->ListDocuments();
    REQUIRE(List.size() == 1);
    CHECK(List[0].Id == *Id);
    CHECK(List[0].FilePath == Path);
    CHECK(List[0].Title == "My Book");
    CHECK(List[0].Format == "TXT");
    CHECK(List[0].bExists);
}

TEST_CASE("Opening the same file twice keeps a single row", "[reader][repository]")
{
    Fixture Test;
    QTemporaryDir Dir;

    const std::string Path = MakeExistingFile(Dir, "book.txt");
    REQUIRE_FALSE(Path.empty());

    const std::optional<std::string> First = Test.Repository->AddDocument(Path, "Old", "TXT");
    REQUIRE(First.has_value());

    // A later open rewrites the title, but the row (and its id) survives.
    const std::optional<std::string> Second = Test.Repository->AddDocument(Path, "Renamed", "TXT");
    REQUIRE(Second.has_value());
    CHECK(*Second == *First);

    const std::vector<FTReaderDocumentRow> List = Test.Repository->ListDocuments();
    REQUIRE(List.size() == 1);
    CHECK(List[0].Title == "Renamed");
}

TEST_CASE("A missing file shows as not existing", "[reader][repository]")
{
    Fixture Test;

    // Nothing wrote the file, so the path does not exist. AddDocument does not check the disk: a
    // row can be recorded before the file arrives, and the list reports the truth either way.
    const std::optional<std::string> Id =
        Test.Repository->AddDocument("C:/nowhere/missing.txt", "Gone", "TXT");
    REQUIRE(Id.has_value());

    const std::vector<FTReaderDocumentRow> List = Test.Repository->ListDocuments();
    REQUIRE(List.size() == 1);
    CHECK_FALSE(List[0].bExists);
}

TEST_CASE("Removing a document drops it from the list", "[reader][repository]")
{
    Fixture Test;
    QTemporaryDir Dir;

    const std::string Path = MakeExistingFile(Dir, "book.txt");
    const std::optional<std::string> Id = Test.Repository->AddDocument(Path, "My Book", "TXT");
    REQUIRE(Id.has_value());

    REQUIRE(Test.Repository->RemoveDocument(*Id));
    CHECK(Test.Repository->ListDocuments().empty());

    // The row is soft-deleted, not gone: the same path added again comes back as a new row.
    const std::optional<std::string> Again = Test.Repository->AddDocument(Path, "My Book", "TXT");
    REQUIRE(Again.has_value());
    CHECK_FALSE(*Again == *Id);
}

TEST_CASE("Opening a document moves it to the top of the list", "[reader][repository]")
{
    Fixture Test;
    QTemporaryDir Dir;

    const std::string First = MakeExistingFile(Dir, "first.txt");
    const std::string Second = MakeExistingFile(Dir, "second.txt");
    REQUIRE_FALSE(First.empty());
    REQUIRE_FALSE(Second.empty());

    const std::optional<std::string> FirstId =
        Test.Repository->AddDocument(First, "First", "TXT");
    Test.FakeNow = 2000;
    const std::optional<std::string> SecondId =
        Test.Repository->AddDocument(Second, "Second", "TXT");
    REQUIRE(FirstId.has_value());
    REQUIRE(SecondId.has_value());

    // Reopening the older row puts it first.
    Test.FakeNow = 3000;
    REQUIRE(Test.Repository->TouchDocument(*FirstId));

    const std::vector<FTReaderDocumentRow> List = Test.Repository->ListDocuments();
    REQUIRE(List.size() == 2);
    CHECK(List[0].Id == *FirstId);
    CHECK(List[1].Id == *SecondId);
}

TEST_CASE("A blank path cannot be added", "[reader][repository]")
{
    Fixture Test;

    CHECK_FALSE(Test.Repository->AddDocument("", "No Path", "TXT").has_value());
    CHECK(Test.Repository->GetLastError() == "empty file path");
}

TEST_CASE("A document keeps where its page was scrolled", "[reader][repository]")
{
    Fixture Test;
    QTemporaryDir Dir;

    const std::string Path = MakeExistingFile(Dir, "book.pdf");
    REQUIRE_FALSE(Path.empty());

    const std::optional<std::string> Id = Test.Repository->AddDocument(Path, "My Book", "PDF");
    REQUIRE(Id.has_value());

    CHECK(Test.Repository->ListDocuments()[0].LastScrollX == 0.0);
    CHECK(Test.Repository->ListDocuments()[0].LastScrollY == 0.0);

    REQUIRE(Test.Repository->SetScroll(*Id, 0.25, 0.5));

    const std::vector<FTReaderDocumentRow> List = Test.Repository->ListDocuments();
    REQUIRE(List.size() == 1);
    CHECK(List[0].LastScrollX == 0.25);
    CHECK(List[0].LastScrollY == 0.5);
}

TEST_CASE("Removing every document empties the library", "[reader][repository]")
{
    Fixture Test;

    REQUIRE(Test.Repository->AddDocument("C:/books/a.pdf", "A", "pdf").has_value());
    REQUIRE(Test.Repository->AddDocument("C:/books/b.epub", "B", "epub").has_value());

    REQUIRE(Test.Repository->RemoveAllDocuments());

    CHECK(Test.Repository->ListDocuments().empty());
}
