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

TEST_CASE("The free shelf combines level filtering with accent-insensitive search", "[reader][book-level]")
{
    QJSEngine Engine;
    LoadReaderShelf(Engine);

    const QJSValue Result{Engine.evaluate(R"(
        arrange([
            {title: "Café primero", level: "beginner"},
            {title: "Cafe segundo", level: "advanced"},
            {title: "Otro cuento", level: "beginner"}
        ], "", "cafe", false, "beginner")
    )")};

    REQUIRE_FALSE(Result.isError());
    REQUIRE(Result.property(QStringLiteral("length")).toInt() == 1);
    REQUIRE(Result.property(0).property(QStringLiteral("title")).toString() == QStringLiteral("Café primero"));
}

TEST_CASE("All levels preserves catalog order and changing level leaves the source intact", "[reader][book-level]")
{
    QJSEngine Engine;
    LoadReaderShelf(Engine);

    const QJSValue Result{Engine.evaluate(R"(
        const books = [
            {title: "Second", level: "advanced"},
            {title: "First", level: "beginner"},
            {title: "Third", level: "intermediate"}
        ];
        const filtered = arrange(books, "", "", false, "intermediate");
        [filtered.length, filtered[0].title, arrange(books, "", "", false, "")[0].title, books.length]
    )")};

    REQUIRE_FALSE(Result.isError());
    REQUIRE(Result.property(0).toInt() == 1);
    REQUIRE(Result.property(1).toString() == QStringLiteral("Third"));
    REQUIRE(Result.property(2).toString() == QStringLiteral("Second"));
    REQUIRE(Result.property(3).toInt() == 3);
}
