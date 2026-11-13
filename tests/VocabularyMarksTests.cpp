#include <catch2/catch_test_macros.hpp>

#include "core/FTVocabularyMarks.h"

#include <vector>

static const FTVocabularyProgress Learning{false, 1};
static const FTVocabularyProgress Known{true, 6};

static QStringList MarkedText(const FTVocabularyMarks& Marks, const QString& Text)
{
    QStringList Result;

    for (const FTVocabularyMark& Mark : Marks.Find(Text))
    {
        Result.append(Text.mid(Mark.Start, Mark.Length) + (Mark.bKnown ? QStringLiteral(":known") : QStringLiteral(":learning")));
    }

    return Result;
}

TEST_CASE("A spelling matches whole words regardless of case", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("chat"), Learning);

    CHECK(MarkedText(Marks, QStringLiteral("Le Chat dort, le chaton aussi.")) == QStringList{QStringLiteral("Chat:learning")});
}

TEST_CASE("Accents are part of the spelling", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("à"), Known);

    CHECK(MarkedText(Marks, QStringLiteral("Il a parlé à Marie.")) == QStringList{QStringLiteral("à:known")});
}

TEST_CASE("A decomposed accent in the book matches the composed spelling", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("été"), Learning);

    CHECK(MarkedText(Marks, QString::fromUtf8("e\xCC\x81te\xCC\x81")).size() == 1);
}

TEST_CASE("An elided article leaves the word after it marked", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("homme"), Learning);
    Marks.AddSpelling(QStringLiteral("aujourd'hui"), Known);

    CHECK(MarkedText(Marks, QStringLiteral("L’homme vient aujourd’hui.")) ==
          QStringList{QStringLiteral("homme:learning"), QStringLiteral("aujourd’hui:known")});
}

TEST_CASE("A hyphenated word marks its parts when the whole is unknown", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("dit"), Learning);
    Marks.AddSpelling(QStringLiteral("est"), Known);
    Marks.AddSpelling(QStringLiteral("peut-être"), Known);

    CHECK(MarkedText(Marks, QStringLiteral("Peut-être, dit-il. Qu'est-ce ?")) ==
          QStringList{QStringLiteral("Peut-être:known"), QStringLiteral("dit:learning"), QStringLiteral("est:known")});
}

TEST_CASE("A phrase matches only its words in a row", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("tout à fait"), Learning);
    Marks.AddSpelling(QStringLiteral("tout"), Known);

    CHECK(MarkedText(Marks, QStringLiteral("C'est tout à fait juste, tout, à fait.")) ==
          QStringList{QStringLiteral("tout à fait:learning"), QStringLiteral("tout:known")});
}

TEST_CASE("A spelling claimed by a learning and a finished card counts as learning", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("fils"), Known);
    Marks.AddSpelling(QStringLiteral("fils"), Learning);
    Marks.AddSpelling(QStringLiteral("fils"), Known);

    CHECK(MarkedText(Marks, QStringLiteral("son fils")) == QStringList{QStringLiteral("fils:learning")});
}

TEST_CASE("Clearing the marks leaves nothing to find", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("chat"), Learning);
    Marks.Clear();

    CHECK(Marks.IsEmpty());
    CHECK(Marks.Find(QStringLiteral("chat")).empty());
}

TEST_CASE("A spelling claimed at two stages shows the earlier one", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("fils"), FTVocabularyProgress{false, 4});
    Marks.AddSpelling(QStringLiteral("fils"), FTVocabularyProgress{false, 0});
    Marks.AddSpelling(QStringLiteral("fils"), Known);

    const std::vector<FTVocabularyMark> Found{Marks.Find(QStringLiteral("son fils"))};

    REQUIRE(Found.size() == 1);
    CHECK_FALSE(Found.front().bKnown);
    CHECK(Found.front().Stage == 0);
}

TEST_CASE("A phrase mark carries its stage", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("tout à fait"), FTVocabularyProgress{false, 3});

    const std::vector<FTVocabularyMark> Found{Marks.Find(QStringLiteral("C'est tout à fait juste."))};

    REQUIRE(Found.size() == 1);
    CHECK(Found.front().Stage == 3);
}

TEST_CASE("Reverse highlighting skips placement words and every vocabulary stage", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("le"), Known);
    Marks.AddSpelling(QStringLiteral("arbre"), Known);
    Marks.AddSpelling(QStringLiteral("chat"), FTVocabularyProgress{false, -1});
    Marks.AddSpelling(QStringLiteral("noir"), Learning);
    Marks.AddSpelling(QStringLiteral("tout à fait"), Learning);

    const QString Text{QStringLiteral("Le chat noir voit l’arbre, tout à fait immobile. 42")};
    QStringList Highlighted;

    for (const FTVocabularyMark& Mark : Marks.Find(Text, true))
    {
        Highlighted.append(Text.mid(Mark.Start, Mark.Length));
    }

    CHECK(Highlighted == QStringList{QStringLiteral("voit"), QStringLiteral("immobile")});
}
