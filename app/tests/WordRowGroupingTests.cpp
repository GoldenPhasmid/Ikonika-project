#include <catch2/catch_test_macros.hpp>

#include "WordRowGrouping.h"

#include <QVariantMap>

static QVariantMap MakeRow(const QString& Word, const QString& PartOfSpeech, const QString& Translation,
                           const QString& CefrLevel, int Stage, bool bCompleted)
{
    QVariantMap Row;
    Row[QStringLiteral("word")] = Word;
    Row[QStringLiteral("partOfSpeech")] = PartOfSpeech;
    Row[QStringLiteral("translation")] = Translation;
    Row[QStringLiteral("cefrLevel")] = CefrLevel;
    Row[QStringLiteral("stage")] = Stage;
    Row[QStringLiteral("completed")] = bCompleted;

    return Row;
}

TEST_CASE("Rows sharing a headword collapse into the first one", "[wordrows]")
{
    const QVariantList Grouped = GroupRowsByHeadword({
        MakeRow(QStringLiteral("jeune"), QStringLiteral("adj"), QStringLiteral("young"), QStringLiteral("A2"), -1, false),
        MakeRow(QStringLiteral("jeu"), QStringLiteral("noun"), QStringLiteral("game"), QStringLiteral("A1"), -1, false),
        MakeRow(QStringLiteral("jeune"), QStringLiteral("noun"), QStringLiteral("youth"), QStringLiteral("A1"), 2, false)
    });

    REQUIRE(Grouped.size() == 2);

    const QVariantMap Jeune = Grouped[0].toMap();
    CHECK(Jeune.value(QStringLiteral("partOfSpeech")).toString() == QStringLiteral("adj"));
    CHECK(Jeune.value(QStringLiteral("translation")).toString() == QStringLiteral("young"));
    CHECK(Jeune.value(QStringLiteral("cefrLevel")).toString() == QStringLiteral("A1"));
    CHECK(Jeune.value(QStringLiteral("stage")).toInt() == 2);
    CHECK_FALSE(Jeune.value(QStringLiteral("completed")).toBool());

    CHECK(Grouped[1].toMap().value(QStringLiteral("word")).toString() == QStringLiteral("jeu"));
}

TEST_CASE("A grouped row is completed when any reading is, and keeps a known CEFR level", "[wordrows]")
{
    const QVariantList Grouped = GroupRowsByHeadword({
        MakeRow(QStringLiteral("fier"), QStringLiteral("adj"), QString(), QStringLiteral("B1"), 1, false),
        MakeRow(QStringLiteral("fier"), QStringLiteral("verb"), QStringLiteral("to trust"), QString(), 0, true)
    });

    REQUIRE(Grouped.size() == 1);

    const QVariantMap Fier = Grouped[0].toMap();
    CHECK(Fier.value(QStringLiteral("completed")).toBool());
    CHECK(Fier.value(QStringLiteral("cefrLevel")).toString() == QStringLiteral("B1"));
    CHECK(Fier.value(QStringLiteral("translation")).toString() == QStringLiteral("to trust"));
}

TEST_CASE("Card rows group by their headword, not their display word", "[wordrows]")
{
    QVariantMap Noun = MakeRow(QStringLiteral("le livre"), QStringLiteral("noun"), QStringLiteral("book"), QString(), 1, false);
    Noun[QStringLiteral("targetWord")] = QStringLiteral("livre");

    QVariantMap Verb = MakeRow(QStringLiteral("livre"), QStringLiteral("verb"), QStringLiteral("delivers"), QString(), 3, false);
    Verb[QStringLiteral("targetWord")] = QStringLiteral("livre");

    const QVariantList Grouped = GroupRowsByHeadword({Noun, Verb});

    REQUIRE(Grouped.size() == 1);
    CHECK(Grouped[0].toMap().value(QStringLiteral("word")).toString() == QStringLiteral("le livre"));
    CHECK(Grouped[0].toMap().value(QStringLiteral("stage")).toInt() == 3);
}

TEST_CASE("A grouped lookup opens the most frequent reading and preserves progress", "[wordrows][lookupfix]")
{
    QVariantMap Noun = MakeRow("si", "noun", "B", "", 2, true);
    Noun["frequencyRank"] = 15360;
    QVariantMap Adverb = MakeRow("si", "adv", "so", "A1", -1, false);
    Adverb["frequencyRank"] = 73;

    for (const QVariantList& Rows : {QVariantList{Noun, Adverb}, QVariantList{Adverb, Noun}})
    {
        const QVariantList Grouped = GroupRowsByHeadword(Rows);
        REQUIRE(Grouped.size() == 1);
        const QVariantMap Row = Grouped.front().toMap();
        CHECK(Row.value("partOfSpeech").toString() == "adv");
        CHECK(Row.value("translation").toString() == "so");
        CHECK(Row.value("cefrLevel").toString() == "A1");
        CHECK(Row.value("stage").toInt() == 2);
        CHECK(Row.value("completed").toBool());
    }
}
