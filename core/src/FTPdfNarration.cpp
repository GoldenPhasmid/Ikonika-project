#include "core/FTPdfNarration.h"

#include <algorithm>
#include <limits>

static constexpr size_t MinimumMeasuredLines = 8;
static constexpr double FitSlackCharacters = 2.0;

struct FPlacedLine
{
    int Page = -1;
    const FTPdfTextLine* Line = nullptr;
    double TextRight = 0.0;
};

static QString JoinPieces(const FTPdfTextLine& Line)
{
    QString Text;

    for (const FTPdfTextPiece& Piece : Line.Pieces)
    {
        Text += Piece.Text;
    }

    return Text;
}

static bool IsPageNumber(const FTPdfTextLine& Line)
{
    const QString Text{JoinPieces(Line).trimmed()};

    if (Text.isEmpty())
    {
        return false;
    }

    return std::all_of(Text.begin(), Text.end(), [](QChar Character) { return Character.isDigit(); });
}

double FindPdfTextWidth(const std::vector<FTPdfTextPage>& Pages)
{
    std::vector<double> Widths;

    for (const FTPdfTextPage& Page : Pages)
    {
        size_t Measured{0};
        double Left{std::numeric_limits<double>::max()};
        double Right{std::numeric_limits<double>::lowest()};

        for (const FTPdfTextLine& Line : Page.Lines)
        {
            if (!Line.bMeasured)
            {
                continue;
            }

            ++Measured;
            Left = std::min(Left, Line.Left);
            Right = std::max(Right, Line.Right);
        }

        if (Measured >= MinimumMeasuredLines)
        {
            Widths.push_back(Right - Left);
        }
    }

    if (Widths.empty())
    {
        return 0.0;
    }

    const auto Middle{Widths.begin() + static_cast<std::ptrdiff_t>(Widths.size() / 2)};
    std::nth_element(Widths.begin(), Middle, Widths.end());

    return *Middle;
}

static std::vector<FPlacedLine> PlaceLines(const std::vector<FTPdfTextPage>& Pages, int FirstPage, double TextWidth)
{
    std::vector<FPlacedLine> Placed;

    for (size_t PageIndex{0}; PageIndex < Pages.size(); ++PageIndex)
    {
        const std::vector<FTPdfTextLine>& Lines{Pages[PageIndex].Lines};
        size_t First{0};
        size_t Last{Lines.size()};

        if (First < Last && IsPageNumber(Lines[First]))
        {
            ++First;
        }

        if (First < Last && IsPageNumber(Lines[Last - 1]))
        {
            --Last;
        }

        double Left{std::numeric_limits<double>::max()};

        for (size_t Index{First}; Index < Last; ++Index)
        {
            if (Lines[Index].bMeasured)
            {
                Left = std::min(Left, Lines[Index].Left);
            }
        }

        const bool bHasLeft{Left < std::numeric_limits<double>::max()};
        const double TextRight{TextWidth > 0.0 && bHasLeft ? Left + TextWidth : 0.0};

        for (size_t Index{First}; Index < Last; ++Index)
        {
            if (!JoinPieces(Lines[Index]).trimmed().isEmpty())
            {
                Placed.push_back({FirstPage + static_cast<int>(PageIndex), &Lines[Index], TextRight});
            }
        }
    }

    return Placed;
}

static bool EndsBeforeNextWordFits(const FPlacedLine& Placed, const QString& Next)
{
    const FTPdfTextLine& Line{*Placed.Line};

    if (!Line.bMeasured || Placed.TextRight <= 0.0)
    {
        return false;
    }

    const QString Text{JoinPieces(Line)};
    const double Width{Line.Right - Line.Left};

    if (Text.isEmpty() || Width <= 0.0)
    {
        return false;
    }

    const double CharacterWidth{Width / static_cast<double>(Text.size())};
    const qsizetype Space{Next.indexOf(u' ')};
    const qsizetype NextWord{Space < 0 ? Next.size() : Space};

    return Line.Right + CharacterWidth * (static_cast<double>(NextWord) + 1.0 + FitSlackCharacters) < Placed.TextRight;
}

FTPdfNarration BuildPdfNarration(const std::vector<FTPdfTextPage>& Pages)
{
    return BuildPdfNarration(Pages, 0, FindPdfTextWidth(Pages));
}

FTPdfNarration BuildPdfNarration(const std::vector<FTPdfTextPage>& Pages, int FirstPage, double TextWidth)
{
    const std::vector<FPlacedLine> Lines{PlaceLines(Pages, FirstPage, TextWidth)};
    FTPdfNarration Narration;
    QString Unit;
    std::vector<FTPdfNarrationSpan> Spans;

    const auto Finish = [&Narration, &Unit, &Spans]()
    {
        if (!Unit.trimmed().isEmpty())
        {
            Narration.Units.push_back(Unit.toStdString());
            Narration.Spans.push_back(Spans);
        }

        Unit.clear();
        Spans.clear();
    };

    for (size_t Index{0}; Index < Lines.size(); ++Index)
    {
        const FPlacedLine& Placed{Lines[Index]};

        for (const FTPdfTextPiece& Piece : Placed.Line->Pieces)
        {
            if (Piece.Text.isEmpty())
            {
                continue;
            }

            Spans.push_back({Placed.Page, Piece.PageStart, Unit.size(), Piece.Text.size()});
            Unit += Piece.Text;
        }

        if (Placed.Line->bSoftHyphen || Index + 1 == Lines.size())
        {
            continue;
        }

        if (EndsBeforeNextWordFits(Placed, JoinPieces(*Lines[Index + 1].Line)))
        {
            Finish();

            continue;
        }

        Unit += u' ';
    }

    Finish();

    return Narration;
}

std::optional<FTPdfPagePlace> FindPdfPagePlace(const FTPdfNarration& Narration, int Unit, qsizetype Character)
{
    if (Unit < 0 || Unit >= static_cast<int>(Narration.Spans.size()))
    {
        return std::nullopt;
    }

    const std::vector<FTPdfNarrationSpan>& Spans{Narration.Spans[static_cast<size_t>(Unit)]};

    for (const FTPdfNarrationSpan& Span : Spans)
    {
        if (Character < Span.UnitStart + Span.Length)
        {
            return FTPdfPagePlace{Span.Page, Span.PageStart + std::max<qsizetype>(0, Character - Span.UnitStart)};
        }
    }

    if (Spans.empty())
    {
        return std::nullopt;
    }

    return FTPdfPagePlace{Spans.back().Page, Spans.back().PageStart + Spans.back().Length};
}

std::optional<FTPdfUnitPlace> FindPdfUnitPlace(const FTPdfNarration& Narration, int Page, qsizetype Character)
{
    for (size_t Unit{0}; Unit < Narration.Spans.size(); ++Unit)
    {
        for (const FTPdfNarrationSpan& Span : Narration.Spans[Unit])
        {
            if (Span.Page > Page || (Span.Page == Page && Span.PageStart + Span.Length > Character))
            {
                const qsizetype Offset{Span.Page == Page ? std::max<qsizetype>(0, Character - Span.PageStart) : 0};

                return FTPdfUnitPlace{static_cast<int>(Unit), Span.UnitStart + Offset};
            }
        }
    }

    return std::nullopt;
}

std::vector<FTPdfNarrationSpan> FindPdfSpans(const FTPdfNarration& Narration, int Unit, qsizetype Start, qsizetype End)
{
    std::vector<FTPdfNarrationSpan> Found;

    if (Unit < 0 || Unit >= static_cast<int>(Narration.Spans.size()))
    {
        return Found;
    }

    for (const FTPdfNarrationSpan& Span : Narration.Spans[static_cast<size_t>(Unit)])
    {
        const qsizetype From{std::max(Start, Span.UnitStart)};
        const qsizetype To{std::min(End, Span.UnitStart + Span.Length)};

        if (From >= To)
        {
            continue;
        }

        Found.push_back({Span.Page, Span.PageStart + (From - Span.UnitStart), From, To - From});
    }

    return Found;
}
