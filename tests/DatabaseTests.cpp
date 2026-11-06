#include <catch2/catch_test_macros.hpp>

#include "core/FTDatabase.h"

#include <filesystem>
#include <set>
#include <string>

namespace
{
    FTDatabase MakeDatabase()
    {
        std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
        REQUIRE(Opened.has_value());

        return std::move(*Opened);
    }

    /** A unique path under the OS temp directory, removed (along with -shm/-wal) when the guard
        goes out of scope, so a file-based Open() test does not leave a database behind on disk
        whether it passes or fails. */
    struct FTempDatabasePath
    {
        FTempDatabasePath()
            : Path((std::filesystem::temp_directory_path()
                    / ("ft-database-test-" + std::to_string(reinterpret_cast<uintptr_t>(this))
                       + ".db")).string())
        {
        }

        ~FTempDatabasePath()
        {
            std::filesystem::remove(Path);
            std::filesystem::remove(Path + "-shm");
            std::filesystem::remove(Path + "-wal");
        }

        FTempDatabasePath(const FTempDatabasePath&) = delete;
        FTempDatabasePath& operator=(const FTempDatabasePath&) = delete;

        std::string Path;
    };
}

TEST_CASE("An in-memory database opens", "[database]")
{
    const FTDatabase Target = MakeDatabase();

    CHECK(Target.IsOpen());
}

TEST_CASE("A default constructed database is not open", "[database]")
{
    const FTDatabase Target;

    CHECK_FALSE(Target.IsOpen());
}

TEST_CASE("Statements can create a table and insert into it", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Words (Id TEXT PRIMARY KEY, Word TEXT NOT NULL)"));

    FTStatement Insert = Target.Prepare("INSERT INTO Words (Id, Word) VALUES (?1, ?2)");
    REQUIRE(Insert.IsValid());

    Insert.BindText(1, "a");
    Insert.BindText(2, "le chien");

    CHECK(Insert.Run());
    CHECK(Insert.GetChangeCount() == 1);
}

TEST_CASE("A semicolon inside a line comment does not split the statement", "[database]")
{
    FTDatabase Target = MakeDatabase();

    // A naive split on ';' would cut this script after "gender;", leaving "INTEGER)" as its own
    // fragment and failing the whole script. The real statement boundary is the one after the
    // closing paren, not the one inside the comment.
    REQUIRE(Target.Execute(
        "CREATE TABLE Words (\n"
        "    Word TEXT NOT NULL,\n"
        "    -- 0 = masculine, 1 = feminine; NULL means no gender\n"
        "    Gender INTEGER\n"
        ")"));

    FTStatement Insert = Target.Prepare("INSERT INTO Words (Word, Gender) VALUES (?1, ?2)");
    REQUIRE(Insert.IsValid());
    Insert.BindText(1, "chien");
    Insert.BindInt(2, 0);

    CHECK(Insert.Run());
}

TEST_CASE("A semicolon inside a string literal does not split the statement", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Notes (Text TEXT NOT NULL DEFAULT 'a;b')"));

    FTStatement Select = Target.Prepare("SELECT Text FROM Notes");
    REQUIRE(Select.IsValid());

    // The default value itself proves the statement ran whole: if the script had been cut at the
    // semicolon inside the string, CREATE TABLE would have failed outright.
    FTStatement Insert = Target.Prepare("INSERT INTO Notes DEFAULT VALUES");
    REQUIRE(Insert.IsValid());
    REQUIRE(Insert.Run());

    REQUIRE(Select.Step());
    CHECK(Select.GetText(0) == "a;b");
}

TEST_CASE("Text survives a round trip with its accents intact", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Words (Word TEXT NOT NULL)"));

    FTStatement Insert = Target.Prepare("INSERT INTO Words (Word) VALUES (?1)");
    REQUIRE(Insert.IsValid());
    Insert.BindText(1, "l'école, le café, cœur, être");
    REQUIRE(Insert.Run());

    FTStatement Select = Target.Prepare("SELECT Word FROM Words");
    REQUIRE(Select.IsValid());
    REQUIRE(Select.Step());

    CHECK(Select.GetText(0) == "l'école, le café, cœur, être");
    CHECK_FALSE(Select.Step());
}

TEST_CASE("Integers round trip, including large ones", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Numbers (Small INTEGER, Large INTEGER)"));

    FTStatement Insert = Target.Prepare("INSERT INTO Numbers (Small, Large) VALUES (?1, ?2)");
    REQUIRE(Insert.IsValid());
    Insert.BindInt(1, -42);
    Insert.BindInt64(2, 1785312000000LL);
    REQUIRE(Insert.Run());

    FTStatement Select = Target.Prepare("SELECT Small, Large FROM Numbers");
    REQUIRE(Select.IsValid());
    REQUIRE(Select.Step());

    CHECK(Select.GetInt(0) == -42);
    CHECK(Select.GetInt64(1) == 1785312000000LL);
}

TEST_CASE("Null is distinguishable from an empty string", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Values_ (A TEXT, B TEXT)"));

    FTStatement Insert = Target.Prepare("INSERT INTO Values_ (A, B) VALUES (?1, ?2)");
    REQUIRE(Insert.IsValid());
    Insert.BindNull(1);
    Insert.BindText(2, "");
    REQUIRE(Insert.Run());

    FTStatement Select = Target.Prepare("SELECT A, B FROM Values_");
    REQUIRE(Select.IsValid());
    REQUIRE(Select.Step());

    CHECK(Select.IsNull(0));
    CHECK_FALSE(Select.IsNull(1));
    CHECK(Select.GetText(1).empty());
}

TEST_CASE("A statement can be reset and reused", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Words (Word TEXT NOT NULL)"));

    FTStatement Insert = Target.Prepare("INSERT INTO Words (Word) VALUES (?1)");
    REQUIRE(Insert.IsValid());

    for (const std::string& Word : {std::string("le chien"), std::string("la maison")})
    {
        Insert.Reset();
        Insert.BindText(1, Word);
        REQUIRE(Insert.Run());
    }

    FTStatement Count = Target.Prepare("SELECT COUNT(*) FROM Words");
    REQUIRE(Count.IsValid());
    REQUIRE(Count.Step());

    CHECK(Count.GetInt(0) == 2);
}

TEST_CASE("A reset statement forgets its bound values", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Pairs (A TEXT, B TEXT)"));

    FTStatement Insert = Target.Prepare("INSERT INTO Pairs (A, B) VALUES (?1, ?2)");
    REQUIRE(Insert.IsValid());

    Insert.BindText(1, "le chien");
    Insert.BindText(2, "dog");
    REQUIRE(Insert.Run());

    Insert.Reset();
    Insert.BindText(1, "la maison");
    REQUIRE(Insert.Run());

    FTStatement Select = Target.Prepare("SELECT B FROM Pairs WHERE A = 'la maison'");
    REQUIRE(Select.Step());

    CHECK(Select.IsNull(0));
}

TEST_CASE("A reset query runs again with new values, repeated placeholders included", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Words (Word TEXT NOT NULL, Other TEXT NOT NULL)"));
    REQUIRE(Target.Execute("INSERT INTO Words (Word, Other) VALUES ('a', 'a'), ('b', 'b'), ('c', 'x')"));

    FTStatement Select = Target.Prepare("SELECT COUNT(*) FROM Words WHERE Word = ?1 AND Other = ?1");
    REQUIRE(Select.IsValid());

    for (const auto& [Word, Expected] : {std::pair<std::string, int>{"a", 1}, {"b", 1}, {"c", 0}})
    {
        Select.Reset();
        Select.BindText(1, Word);

        REQUIRE(Select.Step());
        CHECK(Select.GetInt(0) == Expected);
        CHECK_FALSE(Select.Step());
    }
}

TEST_CASE("Stepping walks every row", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Words (Word TEXT NOT NULL)"));
    REQUIRE(Target.Execute("INSERT INTO Words (Word) VALUES ('a'), ('b'), ('c')"));

    FTStatement Select = Target.Prepare("SELECT Word FROM Words ORDER BY Word");

    std::set<std::string> Seen;
    while (Select.Step())
    {
        Seen.insert(Select.GetText(0));
    }

    CHECK(Seen == std::set<std::string>{"a", "b", "c"});
}

TEST_CASE("Invalid SQL fails rather than crashing, and reports why", "[database]")
{
    FTDatabase Target = MakeDatabase();

    CHECK_FALSE(Target.Execute("THIS IS NOT SQL"));
    CHECK_FALSE(Target.GetLastError().empty());

    const FTStatement Broken = Target.Prepare("SELECT FROM WHERE");
    CHECK_FALSE(Broken.IsValid());
}

TEST_CASE("Using an invalid statement is harmless", "[database]")
{
    FTDatabase Target = MakeDatabase();

    FTStatement Broken = Target.Prepare("NONSENSE");
    REQUIRE_FALSE(Broken.IsValid());

    Broken.BindText(1, "x");
    Broken.BindInt(2, 1);
    Broken.Reset();

    CHECK_FALSE(Broken.Step());
    CHECK_FALSE(Broken.Run());
    CHECK(Broken.GetText(0).empty());
    CHECK(Broken.GetInt(0) == 0);
}

TEST_CASE("A committed transaction keeps its writes", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Words (Word TEXT NOT NULL)"));

    REQUIRE(Target.BeginTransaction());
    REQUIRE(Target.Execute("INSERT INTO Words (Word) VALUES ('le chien')"));
    REQUIRE(Target.Commit());

    FTStatement Count = Target.Prepare("SELECT COUNT(*) FROM Words");
    REQUIRE(Count.Step());

    CHECK(Count.GetInt(0) == 1);
}

TEST_CASE("A rolled back transaction discards its writes", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Words (Word TEXT NOT NULL)"));

    REQUIRE(Target.BeginTransaction());
    REQUIRE(Target.Execute("INSERT INTO Words (Word) VALUES ('le chien')"));
    REQUIRE(Target.Rollback());

    FTStatement Count = Target.Prepare("SELECT COUNT(*) FROM Words");
    REQUIRE(Count.Step());

    CHECK(Count.GetInt(0) == 0);
}

TEST_CASE("Foreign keys are enforced without asking", "[database]")
{
    // Compiled in with SQLITE_DEFAULT_FOREIGN_KEYS, because they are off per connection otherwise
    // and a connection that forgets is a silent data problem.
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Parents (Id TEXT PRIMARY KEY)"));
    REQUIRE(Target.Execute(
        "CREATE TABLE Children (Id TEXT PRIMARY KEY, ParentId TEXT REFERENCES Parents(Id))"));

    CHECK_FALSE(Target.Execute("INSERT INTO Children (Id, ParentId) VALUES ('c', 'missing')"));
}

TEST_CASE("Double quoted strings are rejected rather than silently accepted", "[database]")
{
    // Compiled in with SQLITE_DQS=0. Without it a misspelled column name becomes a string constant
    // and the query quietly returns nonsense.
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Words (Word TEXT NOT NULL)"));

    CHECK_FALSE(Target.Execute("INSERT INTO Words (Word) VALUES (\"le chien\")"));
}

TEST_CASE("A file-based database opens, writes survive a reopen", "[database]")
{
    FTempDatabasePath Temp;

    {
        std::optional<FTDatabase> Opened = FTDatabase::Open(Temp.Path);
        REQUIRE(Opened.has_value());
        CHECK(Opened->IsOpen());

        REQUIRE(Opened->Execute("CREATE TABLE Words (Word TEXT NOT NULL)"));
        REQUIRE(Opened->Execute("INSERT INTO Words (Word) VALUES ('le chien')"));
    }

    // The first connection closed (end of scope) before this one opens; the write only counts if
    // it actually reached disk rather than living in the first connection's own memory.
    std::optional<FTDatabase> Reopened = FTDatabase::Open(Temp.Path);
    REQUIRE(Reopened.has_value());

    FTStatement Count = Reopened->Prepare("SELECT COUNT(*) FROM Words");
    REQUIRE(Count.Step());
    CHECK(Count.GetInt(0) == 1);
}

TEST_CASE("Opening a database at an unusable path fails rather than crashing", "[database]")
{
    // A subdirectory this test never creates, under the real temp directory: guaranteed missing
    // without assuming a specific drive letter exists. This class never creates directories.
    const std::string Path = (std::filesystem::temp_directory_path()
        / "ft-database-test-missing-dir-2f8a1c" / "nowhere.db").string();

    const std::optional<FTDatabase> Opened = FTDatabase::Open(Path);

    CHECK_FALSE(Opened.has_value());
}

TEST_CASE("Committing or rolling back with no active transaction fails cleanly", "[database]")
{
    // TransactionGuard elsewhere relies on this returning false rather than crashing when a
    // transaction was never successfully begun.
    FTDatabase Target = MakeDatabase();

    CHECK_FALSE(Target.Commit());
    CHECK_FALSE(Target.Rollback());
}

TEST_CASE("A moved database takes the connection with it", "[database]")
{
    FTDatabase First = MakeDatabase();
    REQUIRE(First.Execute("CREATE TABLE Words (Word TEXT NOT NULL)"));

    const FTDatabase Second = std::move(First);

    CHECK(Second.IsOpen());
    CHECK_FALSE(First.IsOpen());
}

TEST_CASE("A write reports how many rows it changed", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Words (Word TEXT NOT NULL, Stage INTEGER NOT NULL)"));
    REQUIRE(Target.Execute("INSERT INTO Words (Word, Stage) VALUES ('a', 0), ('b', 0), ('c', 1)"));

    FTStatement Promote = Target.Prepare("UPDATE Words SET Stage = 1 WHERE Stage = ?1");
    REQUIRE(Promote.IsValid());
    Promote.BindInt(1, 0);

    REQUIRE(Promote.Run());
    CHECK(Promote.GetChangeCount() == 2);

    FTStatement Remove = Target.Prepare("DELETE FROM Words WHERE Word = ?1");
    REQUIRE(Remove.IsValid());
    Remove.BindText(1, "c");

    REQUIRE(Remove.Run());
    CHECK(Remove.GetChangeCount() == 1);
}

TEST_CASE("A write that matches nothing reports no rows changed", "[database]")
{
    // Every caller reads this as "did the row exist", so a statement that ran fine but matched
    // no row has to be distinguishable from one that matched.
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Words (Word TEXT NOT NULL)"));

    FTStatement Remove = Target.Prepare("DELETE FROM Words WHERE Word = ?1");
    REQUIRE(Remove.IsValid());
    Remove.BindText(1, "missing");

    REQUIRE(Remove.Run());
    CHECK(Remove.GetChangeCount() == 0);
}

TEST_CASE("A failed write reports no rows changed", "[database]")
{
    FTDatabase Target = MakeDatabase();

    REQUIRE(Target.Execute("CREATE TABLE Words (Word TEXT NOT NULL UNIQUE)"));
    REQUIRE(Target.Execute("INSERT INTO Words (Word) VALUES ('a')"));

    FTStatement Insert = Target.Prepare("INSERT INTO Words (Word) VALUES (?1)");
    REQUIRE(Insert.IsValid());
    Insert.BindText(1, "a");

    CHECK_FALSE(Insert.Run());
    CHECK(Insert.GetChangeCount() == 0);
}

TEST_CASE("A statement outlives a move of the database it came from", "[database]")
{
    // A statement answers for its own writes rather than reaching back into the database, so
    // moving the database out from under a live statement leaves nothing to dangle.
    std::optional<FTDatabase> First = FTDatabase::OpenInMemory();
    REQUIRE(First.has_value());
    REQUIRE(First->Execute("CREATE TABLE Words (Word TEXT NOT NULL)"));

    FTStatement Insert = First->Prepare("INSERT INTO Words (Word) VALUES (?1)");
    REQUIRE(Insert.IsValid());

    FTDatabase Second = std::move(*First);
    First.reset();

    Insert.BindText(1, "le chien");

    REQUIRE(Insert.Run());
    CHECK(Insert.GetChangeCount() == 1);
}

TEST_CASE("A statement reports whether its steps failed", "[database]")
{
    FTDatabase Target = MakeDatabase();

    FTStatement Missing{Target.Prepare("SELECT * FROM Missing")};
    CHECK(Missing.HasFailed());

    FTStatement Overflow{Target.Prepare("SELECT abs(-9223372036854775807 - 1)")};
    CHECK_FALSE(Overflow.Step());
    CHECK(Overflow.HasFailed());

    FTStatement Fine{Target.Prepare("SELECT 1")};
    CHECK(Fine.Step());
    CHECK_FALSE(Fine.Step());
    CHECK_FALSE(Fine.HasFailed());
}

TEST_CASE("A committed write tells the commit listener and a read does not", "[database]")
{
    FTDatabase Database = MakeDatabase();
    REQUIRE(Database.Execute("CREATE TABLE Words (Word TEXT)"));

    int Commits{0};
    Database.SetCommitListener([&Commits] { ++Commits; });

    REQUIRE(Database.Execute("INSERT INTO Words (Word) VALUES ('chien')"));

    CHECK(Commits == 1);

    FTStatement Query{Database.Prepare("SELECT COUNT(*) FROM Words")};
    REQUIRE(Query.IsValid());
    REQUIRE(Query.Step());

    CHECK(Commits == 1);
}

TEST_CASE("Writes inside a transaction tell the commit listener once, at the commit", "[database]")
{
    FTDatabase Database = MakeDatabase();
    REQUIRE(Database.Execute("CREATE TABLE Words (Word TEXT)"));

    int Commits{0};
    Database.SetCommitListener([&Commits] { ++Commits; });

    REQUIRE(Database.BeginTransaction());
    REQUIRE(Database.Execute("INSERT INTO Words (Word) VALUES ('chien')"));
    REQUIRE(Database.Execute("INSERT INTO Words (Word) VALUES ('chat')"));

    CHECK(Commits == 0);

    REQUIRE(Database.Commit());

    CHECK(Commits == 1);
}

TEST_CASE("The commit listener follows a moved database", "[database]")
{
    FTDatabase Original = MakeDatabase();
    REQUIRE(Original.Execute("CREATE TABLE Words (Word TEXT)"));

    int Commits{0};
    Original.SetCommitListener([&Commits] { ++Commits; });

    FTDatabase Moved{std::move(Original)};
    REQUIRE(Moved.Execute("INSERT INTO Words (Word) VALUES ('chien')"));

    CHECK(Commits == 1);
}
