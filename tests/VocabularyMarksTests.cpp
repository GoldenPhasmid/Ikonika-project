#include <catch2/catch_test_macros.hpp>

#include "core/FTVocabularyMarks.h"

#include <vector>

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
    Marks.AddSpelling(QStringLiteral("chat"), false);

    CHECK(MarkedText(Marks, QStringLiteral("Le Chat dort, le chaton aussi.")) == QStringList{QStringLiteral("Chat:learning")});
}

TEST_CASE("Accents are part of the spelling", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("à"), true);

    CHECK(MarkedText(Marks, QStringLiteral("Il a parlé à Marie.")) == QStringList{QStringLiteral("à:known")});
}

TEST_CASE("A decomposed accent in the book matches the composed spelling", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("été"), false);

    CHECK(MarkedText(Marks, QString::fromUtf8("e\xCC\x81te\xCC\x81")).size() == 1);
}

TEST_CASE("An elided article leaves the word after it marked", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("homme"), false);
    Marks.AddSpelling(QStringLiteral("aujourd'hui"), true);

    CHECK(MarkedText(Marks, QStringLiteral("L’homme vient aujourd’hui.")) ==
          QStringList{QStringLiteral("homme:learning"), QStringLiteral("aujourd’hui:known")});
}

TEST_CASE("A hyphenated word marks its parts when the whole is unknown", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("dit"), false);
    Marks.AddSpelling(QStringLiteral("est"), true);
    Marks.AddSpelling(QStringLiteral("peut-être"), true);

    CHECK(MarkedText(Marks, QStringLiteral("Peut-être, dit-il. Qu'est-ce ?")) ==
          QStringList{QStringLiteral("Peut-être:known"), QStringLiteral("dit:learning"), QStringLiteral("est:known")});
}

TEST_CASE("A phrase matches only its words in a row", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("tout à fait"), false);
    Marks.AddSpelling(QStringLiteral("tout"), true);

    CHECK(MarkedText(Marks, QStringLiteral("C'est tout à fait juste, tout, à fait.")) ==
          QStringList{QStringLiteral("tout à fait:learning"), QStringLiteral("tout:known")});
}

TEST_CASE("A spelling claimed by a learning and a finished card counts as learning", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("fils"), true);
    Marks.AddSpelling(QStringLiteral("fils"), false);
    Marks.AddSpelling(QStringLiteral("fils"), true);

    CHECK(MarkedText(Marks, QStringLiteral("son fils")) == QStringList{QStringLiteral("fils:learning")});
}

TEST_CASE("Clearing the marks leaves nothing to find", "[reader][vocabulary]")
{
    FTVocabularyMarks Marks;
    Marks.AddSpelling(QStringLiteral("chat"), false);
    Marks.Clear();

    CHECK(Marks.IsEmpty());
    CHECK(Marks.Find(QStringLiteral("chat")).empty());
}
