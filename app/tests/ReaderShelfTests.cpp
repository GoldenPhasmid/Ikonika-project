#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QJSEngine>
#include <QJSValue>

void LoadReaderShelf(QJSEngine& Engine)
{
    QFile Source{QStringLiteral(PROJECT_ROOT_DIRECTORY "/app/qml/ReaderShelf.js")};
    REQUIRE(Source.open(QIODevice::ReadOnly));

    QString Script{QString::fromUtf8(Source.readAll())};
    Script.remove(QStringLiteral(".pragma library"));

    const QJSValue Result{Engine.evaluate(Script)};
    REQUIRE_FALSE(Result.isError());
}

TEST_CASE("The shelf combines level filtering with accent-insensitive search", "[reader][book-level]")
{
    QJSEngine Engine;
    LoadReaderShelf(Engine);

    const QJSValue Result{Engine.evaluate(R"(
        shelve([
            {title: "Café primero", level: "beginner", local: true},
            {title: "Cafe segundo", level: "advanced", local: true},
            {title: "Otro cuento", level: "beginner", local: true}
        ], "all", toggledFilter(emptyFilter(), "levels", "beginner"), "cafe", "title", false)
    )")};

    REQUIRE_FALSE(Result.isError());
    REQUIRE(Result.property(QStringLiteral("length")).toInt() == 1);
    REQUIRE(Result.property(0).property(QStringLiteral("title")).toString() == QStringLiteral("Café primero"));
}

TEST_CASE("An empty filter keeps the order and filtering leaves the source intact", "[reader][book-level]")
{
    QJSEngine Engine;
    LoadReaderShelf(Engine);

    const QJSValue Result{Engine.evaluate(R"(
        const books = [
            {title: "Second", level: "advanced", local: true},
            {title: "First", level: "beginner", local: true},
            {title: "Third", level: "intermediate", local: true}
        ];
        const filtered = shelve(books, "all", toggledFilter(emptyFilter(), "levels", "intermediate"), "", "recent", false);
        const toggledTwice = toggledFilter(toggledFilter(emptyFilter(), "levels", "advanced"), "levels", "advanced");
        [filtered.length, filtered[0].title, shelve(books, "all", toggledTwice, "", "recent", false)[0].title, books.length]
    )")};

    REQUIRE_FALSE(Result.isError());
    REQUIRE(Result.property(0).toInt() == 1);
    REQUIRE(Result.property(1).toString() == QStringLiteral("Third"));
    REQUIRE(Result.property(2).toString() == QStringLiteral("Second"));
    REQUIRE(Result.property(3).toInt() == 3);
}
