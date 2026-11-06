#include <catch2/catch_test_macros.hpp>

#include "core/FTCssStyle.h"

#include <vector>

namespace
{
    FTCssElement Element(const char* Tag, const char* Classes = "")
    {
        return {QString::fromLatin1(Tag), QString(),
                QString::fromLatin1(Classes).split(QLatin1Char(' '), Qt::SkipEmptyParts)};
    }

    FTCssComputedStyle Resolve(const FTCssStyleSheet& Sheet, const std::vector<FTCssElement>& Path,
                               const FTCssElement* PreviousSibling = nullptr)
    {
        FTCssComputedStyle Style;
        Style.Apply(Sheet.Match(Path, PreviousSibling));

        return Style;
    }
}

TEST_CASE("A child selector matches only a direct child", "[reader][css]")
{
    FTCssStyleSheet Sheet;
    Sheet.Append(QStringLiteral("body > p { text-indent: 1em }"));

    CHECK(Resolve(Sheet, {Element("body"), Element("p")}).TextIndent.Em == 1.0f);
    CHECK(Resolve(Sheet, {Element("body"), Element("div"), Element("p")}).TextIndent.Em == 0.0f);
}

TEST_CASE("A descendant selector matches through any depth", "[reader][css]")
{
    FTCssStyleSheet Sheet;
    Sheet.Append(QStringLiteral(".chapter p { font-style: italic }"));

    CHECK(Resolve(Sheet, {Element("body"), Element("div", "chapter"), Element("blockquote"), Element("p")}).bItalic);
    CHECK_FALSE(Resolve(Sheet, {Element("body"), Element("div"), Element("p")}).bItalic);
}

TEST_CASE("An adjacent sibling selector looks at the element just before", "[reader][css]")
{
    FTCssStyleSheet Sheet;
    Sheet.Append(QStringLiteral("p { text-indent: 1em } h2 + p { text-indent: 0 }"));

    const FTCssElement Heading = Element("h2");
    const FTCssElement Paragraph = Element("p");

    CHECK(Resolve(Sheet, {Element("body"), Element("p")}, &Heading).TextIndent.Em == 0.0f);
    CHECK(Resolve(Sheet, {Element("body"), Element("p")}, &Paragraph).TextIndent.Em == 1.0f);
    CHECK(Resolve(Sheet, {Element("body"), Element("p")}).TextIndent.Em == 1.0f);
}

TEST_CASE("The more specific rule wins, and a later rule wins a tie", "[reader][css]")
{
    FTCssStyleSheet Sheet;
    Sheet.Append(QStringLiteral(".note { text-align: right } p { text-align: center } p { text-align: justify }"));

    CHECK(Resolve(Sheet, {Element("p", "note")}).TextAlign == QStringLiteral("right"));
    CHECK(Resolve(Sheet, {Element("p")}).TextAlign == QStringLiteral("justify"));
}

TEST_CASE("Rules the engine cannot match and print rules are left out", "[reader][css]")
{
    FTCssStyleSheet Sheet;
    Sheet.Append(QStringLiteral("/* a comment */ p:first-child { text-indent: 3em } p[lang] { text-indent: 4em }"
                                "@media print { p { text-indent: 5em } } @media screen { p { margin-top: 2em } }"
                                "@font-face { font-family: Book; src: url(book.ttf) } @charset \"utf-8\";"));

    const FTCssComputedStyle Style = Resolve(Sheet, {Element("p")});

    CHECK(Style.TextIndent.Em == 0.0f);
    CHECK(Style.MarginTop.Em == 2.0f);
}

TEST_CASE("Lengths and font sizes resolve against the text size", "[reader][css]")
{
    FTCssComputedStyle Parent;
    Parent.Apply(FTCssStyleSheet::ParseDeclarations(QStringLiteral("font-size: 1.5em")));

    FTCssComputedStyle Child = Parent.Inherit();
    Child.Apply(FTCssStyleSheet::ParseDeclarations(
        QStringLiteral("margin: 2em 0 1em; text-indent: 16px; font-weight: 700")));

    CHECK(Child.FontScale == 1.5f);
    CHECK(Child.MarginTop.Em == 3.0f);
    CHECK(Child.MarginBottom.Em == 1.5f);
    CHECK(Child.TextIndent.Em == 1.0f);
    CHECK(Child.bBold);
}

TEST_CASE("Margins are not inherited, text styles are", "[reader][css]")
{
    FTCssComputedStyle Parent;
    Parent.Apply(FTCssStyleSheet::ParseDeclarations(
        QStringLiteral("margin-top: 2em; font-style: italic; text-align: center")));

    const FTCssComputedStyle Child = Parent.Inherit();

    CHECK(Child.MarginTop.Em == 0.0f);
    CHECK(Child.bItalic);
    CHECK(Child.TextAlign == QStringLiteral("center"));
}

TEST_CASE("An inline element writes only what differs from its parent", "[reader][css]")
{
    FTCssComputedStyle Parent;
    Parent.Apply(FTCssStyleSheet::ParseDeclarations(QStringLiteral("font-style: italic")));

    FTCssComputedStyle Child = Parent.Inherit();
    Child.Apply(FTCssStyleSheet::ParseDeclarations(QStringLiteral("font-style: normal; font-variant: small-caps")));

    CHECK(Child.InlineDifference(Parent) == QStringLiteral("font-style:normal;font-variant:small-caps"));
    CHECK(Parent.Inherit().InlineDifference(Parent).isEmpty());
}

TEST_CASE("A percentage length is a share of the page width", "[reader][css]")
{
    FTCssComputedStyle Style;
    Style.Apply(FTCssStyleSheet::ParseDeclarations(QStringLiteral("text-indent: 2%; margin-top: 15%; margin-bottom: 1em")));

    CHECK(Style.TextIndent.Em == 0.0f);
    CHECK(Style.TextIndent.Width == 0.02f);
    CHECK(Style.MarginTop.Width == 0.15f);
    CHECK(Style.MarginBottom.Em == 1.0f);
    CHECK(Style.MarginBottom.Width == 0.0f);
}

TEST_CASE("The box shorthand sets each side the way CSS orders them", "[reader][css]")
{
    FTCssComputedStyle Style;
    Style.Apply(FTCssStyleSheet::ParseDeclarations(QStringLiteral("margin: 1em 5% 2em; padding: 0 0 0 1.5em; text-indent: -1.5em")));

    CHECK(Style.MarginTop.Em == 1.0f);
    CHECK(Style.MarginRight.Width == 0.05f);
    CHECK(Style.MarginBottom.Em == 2.0f);
    CHECK(Style.MarginLeft.Width == 0.05f);
    CHECK(Style.PaddingLeft.Em == 1.5f);
    CHECK(Style.TextIndent.Em == -1.5f);
}
