#include <catch2/catch_test_macros.hpp>

#include "core/FTNarration.h"

#include <QStringList>

#include <string>
#include <vector>

static const std::vector<std::string> Book{
    "Chapitre premier",
    "",
    "M. Dupont entra. Il s'assit près de la fenêtre.  ",
    "* * *",
    "« Bonjour ! » dit-il.",
};

TEST_CASE("A tapped character picks the sentence it sits in", "[reader][narration]")
{
    const std::optional<FTNarrationSentence> Sentence{FindNarrationSentenceAt(Book, 2, 20, "fr")};

    REQUIRE(Sentence.has_value());
    CHECK(Sentence->Paragraph == 2);
    CHECK(Sentence->Text == QStringLiteral("Il s'assit près de la fenêtre."));
    CHECK(QString::fromStdString(Book[2]).mid(Sentence->Start, Sentence->End - Sentence->Start) == Sentence->Text);
}

TEST_CASE("An abbreviation does not end the sentence read aloud", "[reader][narration]")
{
    const std::optional<FTNarrationSentence> Sentence{FindNarrationSentenceAt(Book, 2, 0, "fr")};

    REQUIRE(Sentence.has_value());
    CHECK(Sentence->Text == QStringLiteral("M. Dupont entra."));
}

TEST_CASE("Reading moves through sentences and skips text with nothing to say", "[reader][narration]")
{
    std::optional<FTNarrationSentence> Sentence{FindNarrationSentenceAt(Book, 0, 0, "fr")};
    QStringList Read;

    while (Sentence.has_value())
    {
        Read.append(Sentence->Text);
        Sentence = FindNarrationSentenceAfter(Book, *Sentence, "fr");
    }

    CHECK(Read == QStringList{QStringLiteral("Chapitre premier"), QStringLiteral("M. Dupont entra."),
                              QStringLiteral("Il s'assit près de la fenêtre."), QStringLiteral("« Bonjour ! » dit-il.")});
}

TEST_CASE("A tap past the last sentence of a paragraph starts the next one", "[reader][narration]")
{
    const std::optional<FTNarrationSentence> Sentence{FindNarrationSentenceAt(Book, 2, 49, "fr")};

    REQUIRE(Sentence.has_value());
    CHECK(Sentence->Paragraph == 4);
}

TEST_CASE("A paragraph out of range has no sentence", "[reader][narration]")
{
    CHECK_FALSE(FindNarrationSentenceAt(Book, 9, 0, "fr").has_value());
    CHECK_FALSE(FindNarrationSentenceAt(Book, -1, 0, "fr").has_value());
}

TEST_CASE("A faster voice shortens the estimate", "[reader][narration]")
{
    const QString Text{QStringLiteral("Il s'assit près de la fenêtre.")};

    CHECK(EstimateNarrationSeconds(Text, 1.5f) < EstimateNarrationSeconds(Text, 1.0f));
}
