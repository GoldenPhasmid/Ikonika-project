#include <catch2/catch_test_macros.hpp>

#include "core/FTPdfNarration.h"

#include <QStringList>

#include <string>
#include <vector>

static constexpr double CharacterWidth = 10.0;

static FTPdfTextLine MakeLine(const QString& Text, qsizetype PageStart, double Left = 0.0)
{
    FTPdfTextLine Line;
    Line.Pieces.push_back({Text, PageStart});
    Line.Left = Left;
    Line.Right = Left + CharacterWidth * static_cast<double>(Text.size());
    Line.bMeasured = true;

    return Line;
}

static FTPdfTextPage MakePage(const QStringList& Texts)
{
    FTPdfTextPage Page;
    qsizetype Start{0};

    for (const QString& Text : Texts)
    {
        Page.Lines.push_back(MakeLine(Text, Start, Text.startsWith(u' ') ? 30.0 : 0.0));
        Start += Text.size() + 2;
    }

    return Page;
}

static const QString Full{QStringLiteral("aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj")};

TEST_CASE("A short heading line ends its unit and wrapped lines join into one", "[reader][pdfnarration]")
{
    const FTPdfTextPage Page{MakePage({QStringLiteral("Chapter 1"), QStringLiteral("LOG ENTRY: SOL 6"),
                                       QStringLiteral("I am pretty much stuck here on this planet, and I"),
                                       QStringLiteral("think nobody will read this."), Full, Full, Full, Full})};

    const FTPdfNarration Narration{BuildPdfNarration({Page})};

    REQUIRE(Narration.Units.size() >= 3);
    CHECK(Narration.Units[0] == "Chapter 1");
    CHECK(Narration.Units[1] == "LOG ENTRY: SOL 6");
    CHECK(Narration.Units[2] == "I am pretty much stuck here on this planet, and I think nobody will read this.");
}

TEST_CASE("A sentence carries over a page break and a page number is dropped", "[reader][pdfnarration]")
{
    const FTPdfTextPage First{MakePage({Full, Full, Full, Full, Full, Full, Full,
                                        QStringLiteral("The rover crossed the crater slowly, and the dust"),
                                        QStringLiteral("12")})};
    const FTPdfTextPage Second{MakePage({QStringLiteral("rose behind it."), Full, Full, Full, Full, Full, Full, Full})};

    const FTPdfNarration Narration{BuildPdfNarration({First, Second})};

    REQUIRE(Narration.Units.size() == 2);
    CHECK(QString::fromStdString(Narration.Units[0]).endsWith(QStringLiteral("and the dust rose behind it.")));
    CHECK(QString::fromStdString(Narration.Units[0]).indexOf(QStringLiteral("12")) < 0);
}

TEST_CASE("A soft hyphen joins the two halves of a word", "[reader][pdfnarration]")
{
    FTPdfTextPage Page{MakePage({QStringLiteral("The lander was soft"), QStringLiteral("landed near the base."),
                                 Full, Full, Full, Full, Full, Full})};
    Page.Lines[0].Right = 500.0;
    Page.Lines[0].bSoftHyphen = true;

    const FTPdfNarration Narration{BuildPdfNarration({Page})};

    REQUIRE_FALSE(Narration.Units.empty());
    CHECK(Narration.Units[0] == "The lander was softlanded near the base.");
}

TEST_CASE("Text without geometry joins into one unit", "[reader][pdfnarration]")
{
    FTPdfTextPage Page{MakePage({QStringLiteral("Chapter 1"), QStringLiteral("It began.")})};

    for (FTPdfTextLine& Line : Page.Lines)
    {
        Line.bMeasured = false;
    }

    const FTPdfNarration Narration{BuildPdfNarration({Page})};

    REQUIRE(Narration.Units.size() == 1);
    CHECK(Narration.Units[0] == "Chapter 1 It began.");
}

TEST_CASE("Unit places map to page places and back", "[reader][pdfnarration]")
{
    const FTPdfTextPage First{MakePage({Full, Full, Full, Full, Full, Full, Full,
                                        QStringLiteral("The rover crossed the crater slowly, and the dust")})};
    const FTPdfTextPage Second{MakePage({QStringLiteral("rose behind it."), Full, Full, Full, Full, Full, Full, Full})};

    const FTPdfNarration Narration{BuildPdfNarration({First, Second})};
    const QString Unit{QString::fromStdString(Narration.Units[0])};
    const qsizetype Rose{Unit.indexOf(QStringLiteral("rose"))};

    const std::optional<FTPdfPagePlace> Page{FindPdfPagePlace(Narration, 0, Rose)};

    REQUIRE(Page.has_value());
    CHECK(Page->Page == 1);
    CHECK(Page->Character == 0);

    const std::optional<FTPdfUnitPlace> Back{FindPdfUnitPlace(Narration, 1, 0)};

    REQUIRE(Back.has_value());
    CHECK(Back->Unit == 0);
    CHECK(Back->Character == Rose);

    const std::vector<FTPdfNarrationSpan> Spans{FindPdfSpans(Narration, 0, Rose - 5, Rose + 4)};

    REQUIRE(Spans.size() == 2);
    CHECK(Spans[0].Page == 0);
    CHECK(Spans[1].Page == 1);
    CHECK(Spans[1].PageStart == 0);
    CHECK(Spans[1].Length == 4);
}

TEST_CASE("Adding a page keeps the units already built", "[reader][pdfnarration]")
{
    const std::vector<FTPdfTextPage> Pages{
        MakePage({QStringLiteral("Chapter 1"), Full, Full, Full, Full, Full, Full,
                  QStringLiteral("The rover crossed the crater slowly, and the dust")}),
        MakePage({QStringLiteral("rose behind it."), Full, Full, Full, Full, Full, Full, QStringLiteral("The end.")}),
        MakePage({QStringLiteral("Chapter 2"), Full, Full, Full, Full, Full, Full, Full})};

    const double Width{FindPdfTextWidth(Pages)};
    const FTPdfNarration Window{BuildPdfNarration({Pages[0]}, 4, Width)};
    const FTPdfNarration Extended{BuildPdfNarration({Pages[0], Pages[1], Pages[2]}, 4, Width)};

    REQUIRE(Window.Units.size() == 2);
    REQUIRE(Extended.Units.size() == 5);
    CHECK(Extended.Units[0] == Window.Units[0]);
    CHECK(QString::fromStdString(Extended.Units[1]).startsWith(QString::fromStdString(Window.Units[1])));
    CHECK(Extended.Spans[1].back().Page == 5);
    CHECK(Extended.Units[3] == "Chapter 2");
}
