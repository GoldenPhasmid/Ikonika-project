#include "core/FTCssStyle.h"

#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <optional>

namespace
{
    constexpr float PixelsPerEm = 16.0f;
    constexpr float PointsPerEm = 12.0f;

    const char* const XhtmlDefaults =
        "h1 { font-size: 2em; font-weight: bold; margin-top: 0.67em; margin-bottom: 0.67em }"
        "h2 { font-size: 1.5em; font-weight: bold; margin-top: 0.83em; margin-bottom: 0.83em }"
        "h3 { font-size: 1.17em; font-weight: bold; margin-top: 1em; margin-bottom: 1em }"
        "h4 { font-weight: bold; margin-top: 1.33em; margin-bottom: 1.33em }"
        "h5 { font-size: 0.83em; font-weight: bold; margin-top: 1.67em; margin-bottom: 1.67em }"
        "h6 { font-size: 0.67em; font-weight: bold; margin-top: 2.33em; margin-bottom: 2.33em }"
        "p, blockquote, pre, ul, ol, dl, table { margin-top: 1em; margin-bottom: 1em }"
        "i, em, cite, dfn, var, address { font-style: italic }"
        "b, strong, th { font-weight: bold }"
        "center, th { text-align: center }"
        "sup { vertical-align: super }"
        "sub { vertical-align: sub }";

    const char* const Fb2Defaults =
        "title { font-size: 1.3em; font-weight: bold; text-align: center; margin-top: 1.5em; margin-bottom: 0.5em }"
        "subtitle { font-weight: bold; text-align: center; margin-top: 1em; margin-bottom: 0.5em }"
        "p { text-indent: 1.5em; margin-top: 0.3em }"
        "title p, subtitle p, text-author { text-indent: 0 }"
        "epigraph { font-style: italic; margin-top: 1em; margin-bottom: 1em }"
        "text-author { font-weight: bold; text-align: right }"
        "poem, stanza { margin-top: 1em }"
        "emphasis { font-style: italic }"
        "strong { font-weight: bold }"
        "sup { vertical-align: super }"
        "sub { vertical-align: sub }";

    std::optional<FTCssLength> ParseLength(const QString& Value, float FontScale)
    {
        static const QRegularExpression Length(QStringLiteral("^(-?[0-9]*\\.?[0-9]+)\\s*(em|rem|ex|px|pt|%)?$"));

        const QRegularExpressionMatch Match = Length.match(Value.trimmed());

        if (!Match.hasMatch())
        {
            return std::nullopt;
        }

        const float Number = Match.captured(1).toFloat();
        const QString Unit = Match.captured(2);

        if (Unit == QStringLiteral("%"))
        {
            return FTCssLength{0.0f, Number / 100.0f};
        }

        if (Unit == QStringLiteral("px"))
        {
            return FTCssLength{Number / PixelsPerEm, 0.0f};
        }

        if (Unit == QStringLiteral("pt"))
        {
            return FTCssLength{Number / PointsPerEm, 0.0f};
        }

        if (Unit == QStringLiteral("rem"))
        {
            return FTCssLength{Number, 0.0f};
        }

        if (Unit == QStringLiteral("ex"))
        {
            return FTCssLength{Number * 0.5f * FontScale, 0.0f};
        }

        if (Unit.isEmpty() && Number != 0.0f)
        {
            return std::nullopt;
        }

        return FTCssLength{Number * FontScale, 0.0f};
    }

    FTCssLength NotNegative(const FTCssLength& Length)
    {
        return {std::max(0.0f, Length.Em), std::max(0.0f, Length.Width)};
    }

    std::optional<float> ParseFontSize(const QString& Value, float ParentScale)
    {
        static const QRegularExpression Size(QStringLiteral("^([0-9]*\\.?[0-9]+)\\s*(em|rem|%|px|pt)$"));
        static const std::vector<std::pair<QString, float>> Keywords{
            {QStringLiteral("xx-small"), 0.6f}, {QStringLiteral("x-small"), 0.75f}, {QStringLiteral("small"), 0.89f},
            {QStringLiteral("medium"), 1.0f}, {QStringLiteral("large"), 1.2f}, {QStringLiteral("x-large"), 1.5f},
            {QStringLiteral("xx-large"), 2.0f}
        };

        for (const auto& [Keyword, Scale] : Keywords)
        {
            if (Value == Keyword)
            {
                return Scale;
            }
        }

        if (Value == QStringLiteral("smaller"))
        {
            return ParentScale / 1.2f;
        }

        if (Value == QStringLiteral("larger"))
        {
            return ParentScale * 1.2f;
        }

        const QRegularExpressionMatch Match = Size.match(Value);

        if (!Match.hasMatch())
        {
            return std::nullopt;
        }

        const float Number = Match.captured(1).toFloat();
        const QString Unit = Match.captured(2);

        if (Unit == QStringLiteral("%"))
        {
            return Number / 100.0f * ParentScale;
        }

        if (Unit == QStringLiteral("px"))
        {
            return Number / PixelsPerEm;
        }

        if (Unit == QStringLiteral("pt"))
        {
            return Number / PointsPerEm;
        }

        return Unit == QStringLiteral("rem") ? Number : Number * ParentScale;
    }

    std::array<std::optional<FTCssLength>, 4> ParseBox(const QString& Value, float FontScale)
    {
        const QStringList Parts = Value.split(QLatin1Char(' '), Qt::SkipEmptyParts);

        if (Parts.isEmpty())
        {
            return {};
        }

        const QString Top = Parts[0];
        const QString Right = Parts.size() >= 2 ? Parts[1] : Parts[0];
        const QString Bottom = Parts.size() >= 3 ? Parts[2] : Parts[0];
        const QString Left = Parts.size() >= 4 ? Parts[3] : Right;

        return {ParseLength(Top, FontScale), ParseLength(Right, FontScale), ParseLength(Bottom, FontScale),
                ParseLength(Left, FontScale)};
    }

    bool IsBoldWeight(const QString& Value, bool bInherited)
    {
        if (Value == QStringLiteral("bold") || Value == QStringLiteral("bolder"))
        {
            return true;
        }

        if (Value == QStringLiteral("normal") || Value == QStringLiteral("lighter"))
        {
            return false;
        }

        bool bNumber = false;
        const int Weight = Value.toInt(&bNumber);

        return bNumber ? Weight >= 600 : bInherited;
    }
}

FTCssComputedStyle FTCssComputedStyle::Inherit() const
{
    FTCssComputedStyle Child{*this};
    Child.MarginTop = {};
    Child.MarginBottom = {};
    Child.PaddingTop = {};
    Child.PaddingBottom = {};
    Child.MarginLeft = {};
    Child.MarginRight = {};
    Child.PaddingLeft = {};
    Child.PaddingRight = {};
    Child.VerticalAlign.clear();
    Child.bHidden = false;

    return Child;
}

void FTCssComputedStyle::Apply(const FTCssDeclarations& Declarations, QHash<QString, int>* Unhandled)
{
    const float ParentScale = FontScale;

    const auto Report = [Unhandled](const QString& Entry)
    {
        if (Unhandled != nullptr)
        {
            ++(*Unhandled)[Entry];
        }
    };

    for (const auto& [Property, Value] : Declarations)
    {
        if (Property == QStringLiteral("font-size"))
        {
            const std::optional<float> Scale = ParseFontSize(Value.toLower(), ParentScale);

            if (!Scale.has_value())
            {
                Report(Property + QStringLiteral(": ") + Value);
            }

            FontScale = Scale.value_or(FontScale);
        }
    }

    const auto SetLength = [this, &Report](FTCssLength& Target, const QString& Property, const QString& Value)
    {
        if (const std::optional<FTCssLength> Length = ParseLength(Value, FontScale))
        {
            Target = NotNegative(*Length);

            return;
        }

        Report(Property + QStringLiteral(": ") + Value);
    };

    for (const auto& [Property, RawValue] : Declarations)
    {
        const QString Value = RawValue.toLower();

        if (Property == QStringLiteral("font-style"))
        {
            bItalic = Value == QStringLiteral("italic") || Value == QStringLiteral("oblique");
        }
        else if (Property == QStringLiteral("font-weight"))
        {
            bBold = IsBoldWeight(Value, bBold);
        }
        else if (Property == QStringLiteral("font-variant"))
        {
            bSmallCaps = Value == QStringLiteral("small-caps");
        }
        else if (Property == QStringLiteral("text-indent"))
        {
            if (const std::optional<FTCssLength> Length = ParseLength(Value, FontScale))
            {
                TextIndent = *Length;

                continue;
            }

            Report(Property + QStringLiteral(": ") + Value);
        }
        else if (Property == QStringLiteral("text-align"))
        {
            TextAlign = Value == QStringLiteral("start") ? QStringLiteral("left")
                        : Value == QStringLiteral("end") ? QStringLiteral("right") : Value;
        }
        else if (Property == QStringLiteral("text-transform"))
        {
            TextTransform = Value == QStringLiteral("none") ? QString() : Value;
        }
        else if (Property == QStringLiteral("vertical-align"))
        {
            VerticalAlign = Value == QStringLiteral("super") || Value == QStringLiteral("sub") ? Value : QString();
        }
        else if (Property == QStringLiteral("margin-top"))
        {
            SetLength(MarginTop, Property, Value);
        }
        else if (Property == QStringLiteral("margin-bottom"))
        {
            SetLength(MarginBottom, Property, Value);
        }
        else if (Property == QStringLiteral("padding-top"))
        {
            SetLength(PaddingTop, Property, Value);
        }
        else if (Property == QStringLiteral("padding-bottom"))
        {
            SetLength(PaddingBottom, Property, Value);
        }
        else if (Property == QStringLiteral("margin-left"))
        {
            SetLength(MarginLeft, Property, Value);
        }
        else if (Property == QStringLiteral("margin-right"))
        {
            SetLength(MarginRight, Property, Value);
        }
        else if (Property == QStringLiteral("padding-left"))
        {
            SetLength(PaddingLeft, Property, Value);
        }
        else if (Property == QStringLiteral("padding-right"))
        {
            SetLength(PaddingRight, Property, Value);
        }
        else if (Property == QStringLiteral("margin") || Property == QStringLiteral("padding"))
        {
            const bool bMargin = Property == QStringLiteral("margin");
            const std::array<std::optional<FTCssLength>, 4> Sides = ParseBox(Value, FontScale);
            const std::array<FTCssLength*, 4> Targets{bMargin ? &MarginTop : &PaddingTop, bMargin ? &MarginRight : &PaddingRight,
                                                      bMargin ? &MarginBottom : &PaddingBottom, bMargin ? &MarginLeft : &PaddingLeft};

            for (size_t Side = 0; Side < Sides.size(); ++Side)
            {
                if (Sides[Side].has_value())
                {
                    *Targets[Side] = NotNegative(*Sides[Side]);
                }
            }
        }
        else if (Property == QStringLiteral("display") || Property == QStringLiteral("visibility"))
        {
            bHidden = Value == QStringLiteral("none") || Value == QStringLiteral("hidden");
        }
        else if (Property != QStringLiteral("font-size"))
        {
            Report(Property);
        }
    }
}

QString FTCssComputedStyle::InlineDifference(const FTCssComputedStyle& Parent) const
{
    QStringList Parts;

    if (bItalic != Parent.bItalic)
    {
        Parts.append(bItalic ? QStringLiteral("font-style:italic") : QStringLiteral("font-style:normal"));
    }

    if (bBold != Parent.bBold)
    {
        Parts.append(bBold ? QStringLiteral("font-weight:700") : QStringLiteral("font-weight:400"));
    }

    if (bSmallCaps != Parent.bSmallCaps)
    {
        Parts.append(bSmallCaps ? QStringLiteral("font-variant:small-caps") : QStringLiteral("font-variant:normal"));
    }

    if (TextTransform != Parent.TextTransform)
    {
        Parts.append(QStringLiteral("text-transform:") + (TextTransform.isEmpty() ? QStringLiteral("none") : TextTransform));
    }

    if (!VerticalAlign.isEmpty())
    {
        Parts.append(QStringLiteral("vertical-align:") + VerticalAlign);
    }

    return Parts.join(QLatin1Char(';'));
}

FTCssStyleSheet FTCssStyleSheet::ForXhtml()
{
    FTCssStyleSheet Sheet;
    Sheet.Append(QString::fromLatin1(XhtmlDefaults));

    return Sheet;
}

FTCssStyleSheet FTCssStyleSheet::ForFb2()
{
    FTCssStyleSheet Sheet;
    Sheet.Append(QString::fromLatin1(Fb2Defaults));

    return Sheet;
}

void FTCssStyleSheet::Append(const QString& Css)
{
    static const QRegularExpression Comment(QStringLiteral("/\\*.*?\\*/"),
                                            QRegularExpression::DotMatchesEverythingOption);

    AppendRules(QString(Css).remove(Comment));
}

void FTCssStyleSheet::AppendRules(const QString& Css)
{
    qsizetype Position = 0;

    while (Position < Css.size())
    {
        while (Position < Css.size() && Css[Position].isSpace())
        {
            ++Position;
        }

        if (Position >= Css.size())
        {
            return;
        }

        const qsizetype Open = Css.indexOf(QLatin1Char('{'), Position);

        if (Css[Position] == QLatin1Char('@'))
        {
            const qsizetype Semicolon = Css.indexOf(QLatin1Char(';'), Position);

            if (Open < 0 || (Semicolon >= 0 && Semicolon < Open))
            {
                Position = Semicolon < 0 ? Css.size() : Semicolon + 1;

                continue;
            }

            int Depth = 1;
            qsizetype Close = Open + 1;

            for (; Close < Css.size() && Depth > 0; ++Close)
            {
                Depth += Css[Close] == QLatin1Char('{') ? 1 : (Css[Close] == QLatin1Char('}') ? -1 : 0);
            }

            const QString Header = Css.mid(Position, Open - Position).toLower();
            const bool bScreenMedia = Header.startsWith(QStringLiteral("@media"))
                                      && (!Header.contains(QStringLiteral("print")) || Header.contains(QStringLiteral("screen"))
                                          || Header.contains(QStringLiteral("all")));

            if (bScreenMedia)
            {
                AppendRules(Css.mid(Open + 1, Close - Open - 2));
            }
            else
            {
                SkippedRules.append(Header.simplified());
            }

            Position = Close;

            continue;
        }

        if (Open < 0)
        {
            return;
        }

        const qsizetype Close = Css.indexOf(QLatin1Char('}'), Open);

        if (Close < 0)
        {
            return;
        }

        const FTCssDeclarations Declarations = ParseDeclarations(Css.mid(Open + 1, Close - Open - 1));

        for (const QString& Text : Css.mid(Position, Open - Position).split(QLatin1Char(',')))
        {
            FTCssSelector Selector;

            if (ParseSelector(Text, Selector))
            {
                Rules.push_back({Selector, Declarations, NextOrder++});

                continue;
            }

            SkippedRules.append(Text.simplified());
        }

        Position = Close + 1;
    }
}

FTCssDeclarations FTCssStyleSheet::ParseDeclarations(const QString& Block)
{
    static const QRegularExpression Important(QStringLiteral("\\s*!\\s*important\\s*$"),
                                              QRegularExpression::CaseInsensitiveOption);

    FTCssDeclarations Declarations;

    for (const QString& Entry : Block.split(QLatin1Char(';')))
    {
        const qsizetype Colon = Entry.indexOf(QLatin1Char(':'));

        if (Colon <= 0)
        {
            continue;
        }

        const QString Property = Entry.left(Colon).trimmed().toLower();
        const QString Value = Entry.mid(Colon + 1).remove(Important).trimmed();

        if (!Property.isEmpty() && !Value.isEmpty())
        {
            Declarations.emplace_back(Property, Value);
        }
    }

    return Declarations;
}

bool FTCssStyleSheet::ParseSelector(const QString& Text, FTCssSelector& Selector)
{
    static const QRegularExpression Unsupported(QStringLiteral("[:\\[~]"));
    static const QRegularExpression CompoundPattern(
        QStringLiteral("^([a-zA-Z][a-zA-Z0-9_-]*|\\*)?((?:[#.][a-zA-Z_][a-zA-Z0-9_-]*)*)$"));
    static const QRegularExpression Part(QStringLiteral("([#.])([a-zA-Z_][a-zA-Z0-9_-]*)"));

    if (Text.trimmed().isEmpty() || Text.contains(Unsupported))
    {
        return false;
    }

    QString Spaced = Text;
    Spaced.replace(QLatin1Char('>'), QStringLiteral(" > ")).replace(QLatin1Char('+'), QStringLiteral(" + "));

    QChar Pending = QLatin1Char(' ');

    for (const QString& Token : Spaced.split(QLatin1Char(' '), Qt::SkipEmptyParts))
    {
        if (Token == QStringLiteral(">") || Token == QStringLiteral("+"))
        {
            if (Selector.Compounds.empty())
            {
                return false;
            }

            Pending = Token[0];

            continue;
        }

        const QRegularExpressionMatch Match = CompoundPattern.match(Token);

        if (!Match.hasMatch())
        {
            return false;
        }

        FTCssCompound Compound;
        Compound.Tag = Match.captured(1) == QStringLiteral("*") ? QString() : Match.captured(1).toLower();
        Selector.Specificity += Compound.Tag.isEmpty() ? 0 : 1;

        QRegularExpressionMatchIterator Parts = Part.globalMatch(Match.captured(2));

        while (Parts.hasNext())
        {
            const QRegularExpressionMatch Found = Parts.next();

            if (Found.captured(1) == QStringLiteral("#"))
            {
                Compound.Id = Found.captured(2);
                Selector.Specificity += 100;
            }
            else
            {
                Compound.Classes.append(Found.captured(2));
                Selector.Specificity += 10;
            }
        }

        if (!Selector.Compounds.empty())
        {
            Selector.Combinators.push_back(Pending);
        }

        Selector.Compounds.push_back(Compound);
        Pending = QLatin1Char(' ');
    }

    return !Selector.Compounds.empty();
}

bool FTCssStyleSheet::MatchesCompound(const FTCssCompound& Compound, const FTCssElement& Element)
{
    if (!Compound.Tag.isEmpty() && Compound.Tag != Element.Tag)
    {
        return false;
    }

    if (!Compound.Id.isEmpty() && Compound.Id != Element.Id)
    {
        return false;
    }

    return std::all_of(Compound.Classes.begin(), Compound.Classes.end(),
                       [&Element](const QString& Class) { return Element.Classes.contains(Class); });
}

bool FTCssStyleSheet::MatchesFrom(const FTCssSelector& Selector, int CompoundIndex,
                                  const std::vector<FTCssElement>& Path, int PathIndex,
                                  const FTCssElement* PreviousSibling)
{
    if (!MatchesCompound(Selector.Compounds[static_cast<size_t>(CompoundIndex)], Path[static_cast<size_t>(PathIndex)]))
    {
        return false;
    }

    if (CompoundIndex == 0)
    {
        return true;
    }

    const QChar Combinator = Selector.Combinators[static_cast<size_t>(CompoundIndex - 1)];

    if (Combinator == QLatin1Char('>'))
    {
        return PathIndex > 0 && MatchesFrom(Selector, CompoundIndex - 1, Path, PathIndex - 1, nullptr);
    }

    if (Combinator == QLatin1Char('+'))
    {
        if (PreviousSibling == nullptr)
        {
            return false;
        }

        std::vector<FTCssElement> SiblingPath(Path.begin(), Path.begin() + PathIndex);
        SiblingPath.push_back(*PreviousSibling);

        return MatchesFrom(Selector, CompoundIndex - 1, SiblingPath, PathIndex, nullptr);
    }

    for (int Ancestor = PathIndex - 1; Ancestor >= 0; --Ancestor)
    {
        if (MatchesFrom(Selector, CompoundIndex - 1, Path, Ancestor, nullptr))
        {
            return true;
        }
    }

    return false;
}

FTCssDeclarations FTCssStyleSheet::Match(const std::vector<FTCssElement>& Path, const FTCssElement* PreviousSibling) const
{
    if (Path.empty())
    {
        return {};
    }

    std::vector<const FTCssRule*> Matched;
    const int Last = static_cast<int>(Path.size()) - 1;

    for (const FTCssRule& Rule : Rules)
    {
        if (MatchesFrom(Rule.Selector, static_cast<int>(Rule.Selector.Compounds.size()) - 1, Path, Last, PreviousSibling))
        {
            Matched.push_back(&Rule);
        }
    }

    std::stable_sort(Matched.begin(), Matched.end(), [](const FTCssRule* Left, const FTCssRule* Right)
    {
        return Left->Selector.Specificity != Right->Selector.Specificity
                   ? Left->Selector.Specificity < Right->Selector.Specificity
                   : Left->Order < Right->Order;
    });

    FTCssDeclarations Declarations;

    for (const FTCssRule* Rule : Matched)
    {
        Declarations.insert(Declarations.end(), Rule->Declarations.begin(), Rule->Declarations.end());
    }

    return Declarations;
}

const QStringList& FTCssStyleSheet::GetSkippedRules() const
{
    return SkippedRules;
}
