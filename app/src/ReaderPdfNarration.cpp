#include "ReaderPdfNarration.h"

#include "core/FTLog.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QPointer>
#include <QRectF>
#include <QThread>
#include <QThreadPool>

#include <algorithm>
#include <map>

static constexpr QChar SoftHyphenMark{0xFFFE};

struct FRawLine
{
    qsizetype Start = 0;
    qsizetype Length = 0;
    bool bSoftHyphen = false;
};

static bool IsLineBreak(QChar Character)
{
    return Character == u'\r' || Character == u'\n';
}

static bool IsClosingMark(QChar Character)
{
    const QChar::Category Category{Character.category()};

    return Category == QChar::Punctuation_FinalQuote || Category == QChar::Punctuation_Close
        || Character == u'”' || Character == u'’';
}

static bool EndsSentence(const QString& Text)
{
    qsizetype End{Text.size()};

    while (End > 0 && IsClosingMark(Text.at(End - 1)))
    {
        --End;
    }

    return End > 0 && QStringLiteral(".!?…").contains(Text.at(End - 1));
}

static std::vector<FRawLine> SplitRawLines(const QString& Text)
{
    std::vector<FRawLine> Lines;
    qsizetype Start{0};

    const auto Add = [&Lines, &Text](qsizetype From, qsizetype To, bool bSoftHyphen)
    {
        while (From < To && Text.at(From).isSpace())
        {
            ++From;
        }

        while (To > From && Text.at(To - 1).isSpace())
        {
            --To;
        }

        if (To > From)
        {
            Lines.push_back({From, To - From, bSoftHyphen});
        }
    };

    for (qsizetype Index{0}; Index < Text.size(); ++Index)
    {
        const QChar Character{Text.at(Index)};

        if (Character == SoftHyphenMark)
        {
            Add(Start, Index, true);
            Start = Index + 1;

            continue;
        }

        if (IsLineBreak(Character))
        {
            Add(Start, Index, false);
            Start = Index + 1;
        }
    }

    Add(Start, Text.size(), false);

    return Lines;
}

static QList<QRectF> FindVisualLines(const QPdfSelection& All)
{
    QList<QRectF> Lines;
    QRectF Previous;

    for (const QPolygonF& Bound : All.bounds())
    {
        const QRectF Box{Bound.boundingRect()};

        if (Box.isEmpty())
        {
            continue;
        }

        if (!Lines.isEmpty() && Box.left() >= Previous.left() && Box.top() <= Lines.last().bottom())
        {
            Lines.last() = Lines.last().united(Box);
            Previous = Box;

            continue;
        }

        Previous = Box;
        Lines.append(Box);
    }

    return Lines;
}

static FTPdfTextPage ReadTextPage(QPdfDocument& Document, int Page, bool& bMeasuredExactly)
{
    const QPdfSelection All{Document.getAllText(Page)};
    const QString Text{All.text()};
    const qsizetype Base{All.startIndex()};
    FTPdfTextPage Result;

    for (const FRawLine& Raw : SplitRawLines(Text))
    {
        const FTPdfTextPiece Piece{Text.mid(Raw.Start, Raw.Length), Base + Raw.Start};

        if (!Result.Lines.empty() && !Result.Lines.back().bSoftHyphen && IsClosingMark(Piece.Text.at(0)))
        {
            Result.Lines.back().Pieces.push_back(Piece);
            Result.Lines.back().bSoftHyphen = Raw.bSoftHyphen;

            continue;
        }

        FTPdfTextLine Line;
        Line.Pieces.push_back(Piece);
        Line.bSoftHyphen = Raw.bSoftHyphen;
        Result.Lines.push_back(std::move(Line));
    }

    const QList<QRectF> Visual{FindVisualLines(All)};

    bMeasuredExactly = Visual.size() != static_cast<qsizetype>(Result.Lines.size());

    for (size_t Index{0}; Index < Result.Lines.size(); ++Index)
    {
        FTPdfTextLine& Line{Result.Lines[Index]};
        QRectF Box;

        if (!bMeasuredExactly)
        {
            Box = Visual.at(static_cast<qsizetype>(Index));
        }
        else if (!Line.bSoftHyphen && !EndsSentence(Line.Pieces.back().Text))
        {
            const FTPdfTextPiece& First{Line.Pieces.front()};
            const FTPdfTextPiece& Last{Line.Pieces.back()};
            const qsizetype End{Last.PageStart + Last.Text.size()};

            Box = Document.getSelectionAtIndex(Page, static_cast<int>(First.PageStart),
                                               static_cast<int>(End - First.PageStart)).boundingRectangle();
        }

        Line.Left = Box.left();
        Line.Right = Box.right();
        Line.bMeasured = !Box.isEmpty();
    }

    return Result;
}

static constexpr int SamplePages = 8;
static constexpr int ExtendPagesAtMost = 5;

static FTPdfTextPage ReadTextPage(QPdfDocument& Document, int Page)
{
    bool bMeasuredExactly{false};

    return ReadTextPage(Document, Page, bMeasuredExactly);
}

static std::vector<int> ChooseSamplePages(int PageCount)
{
    std::vector<int> Chosen;

    for (int Sample{0}; Sample < SamplePages; ++Sample)
    {
        const int Page{PageCount > 2 ? 1 + (Sample * (PageCount - 2)) / (SamplePages - 1) : Sample};

        if (Page < PageCount && std::find(Chosen.begin(), Chosen.end(), Page) == Chosen.end())
        {
            Chosen.push_back(Page);
        }
    }

    return Chosen;
}

ReaderPdfNarration::ReaderPdfNarration(QObject* Parent)
    : QObject(Parent)
{
}

ReaderPdfNarration::~ReaderPdfNarration() = default;

void ReaderPdfNarration::prepare(int Page)
{
    if (!bAvailable || !Document || Page < 0 || Page >= Document->pageCount())
    {
        return;
    }

    if (Page >= FirstPage && Page <= LastPage)
    {
        return;
    }

    QElapsedTimer Timer;
    Timer.start();

    FirstPage = std::max(0, Page - 1);
    LastPage = FirstPage - 1;
    Pages.clear();

    while (LastPage < Page)
    {
        AppendPage();
    }

    qCInfo(LogApp) << "Reader: read aloud text prepared for pages" << FirstPage << "to" << LastPage << "in"
                   << Timer.elapsed() << "ms," << Narration.Units.size() << "units";

    emit windowChanged();
}

QVariantList ReaderPdfNarration::pageRanges(int Unit, int Start, int End) const
{
    std::map<int, std::pair<qsizetype, qsizetype>> Ranges;

    for (const FTPdfNarrationSpan& Span : FindPdfSpans(Narration, Unit, Start, End))
    {
        const qsizetype SpanEnd{Span.PageStart + Span.Length};
        const auto Found{Ranges.find(Span.Page)};

        if (Found == Ranges.end())
        {
            Ranges.emplace(Span.Page, std::make_pair(Span.PageStart, SpanEnd));

            continue;
        }

        Found->second.first = std::min(Found->second.first, Span.PageStart);
        Found->second.second = std::max(Found->second.second, SpanEnd);
    }

    QVariantList Result;

    for (const auto& [Page, Range] : Ranges)
    {
        Result.append(QVariantMap{{QStringLiteral("page"), Page},
                                  {QStringLiteral("start"), static_cast<int>(Range.first)},
                                  {QStringLiteral("length"), static_cast<int>(Range.second - Range.first)}});
    }

    return Result;
}

QVariantMap ReaderPdfNarration::unitPlace(int Page, int Character) const
{
    const std::optional<FTPdfUnitPlace> Place{ToUnitPlace(Page, Character)};

    if (!Place.has_value())
    {
        return {};
    }

    return {{QStringLiteral("paragraph"), Place->Unit}, {QStringLiteral("character"), static_cast<int>(Place->Character)}};
}

int ReaderPdfNarration::pageOf(int Unit, int Character) const
{
    const std::optional<FTPdfPagePlace> Place{ToPagePlace(Unit, Character)};

    return Place.has_value() ? Place->Page : -1;
}

void ReaderPdfNarration::Load(const QString& FilePath)
{
    if (FilePath == Source)
    {
        return;
    }

    Clear();
    Source = FilePath;

    const quint64 LoadGeneration{Generation};
    const QPointer<ReaderPdfNarration> Guard{this};
    QThread* const InterfaceThread{thread()};

    QThreadPool::globalInstance()->start([Guard, FilePath, LoadGeneration, InterfaceThread]()
    {
        QElapsedTimer Timer;
        Timer.start();

        QPdfDocument* const Loaded{new QPdfDocument};
        bool bHasText{false};
        double Width{0.0};

        if (Loaded->load(FilePath) == QPdfDocument::Error::None)
        {
            std::vector<FTPdfTextPage> Samples;

            for (const int Page : ChooseSamplePages(Loaded->pageCount()))
            {
                Samples.push_back(ReadTextPage(*Loaded, Page));
                bHasText = bHasText || !Samples.back().Lines.empty();
            }

            Width = FindPdfTextWidth(Samples);
        }

        qCInfo(LogApp) << "Reader: checked the text layer of" << FilePath << "in" << Timer.elapsed()
                       << "ms, text found" << bHasText;

        Loaded->moveToThread(InterfaceThread);

        QMetaObject::invokeMethod(QCoreApplication::instance(), [Guard, LoadGeneration, Loaded, bHasText, Width]()
        {
            if (!Guard)
            {
                delete Loaded;

                return;
            }

            Guard->OnLoaded(LoadGeneration, Loaded, bHasText, Width);
        }, Qt::QueuedConnection);
    });
}

void ReaderPdfNarration::Clear()
{
    ++Generation;
    Source.clear();

    const bool bWasAvailable{bAvailable};

    Document.reset();
    bAvailable = false;
    TextWidth = 0.0;
    FirstPage = -1;
    LastPage = -1;
    Pages.clear();
    Narration = {};

    if (bWasAvailable)
    {
        emit changed();
    }
}

void ReaderPdfNarration::ExtendAfter(int Unit)
{
    if (!Document || LastPage < 0)
    {
        return;
    }

    int Added{0};

    while (LastPage + 1 < Document->pageCount() && Added < ExtendPagesAtMost
           && Unit >= static_cast<int>(Narration.Units.size()) - 2)
    {
        AppendPage();
        ++Added;
    }

    if (Added > 0)
    {
        qCDebug(LogApp) << "Reader: read aloud text extended to page" << LastPage;
    }
}

bool ReaderPdfNarration::IsAvailable() const
{
    return bAvailable;
}

const std::vector<std::string>* ReaderPdfNarration::GetUnits() const
{
    return bAvailable ? &Narration.Units : nullptr;
}

std::optional<FTPdfPagePlace> ReaderPdfNarration::ToPagePlace(int Unit, int Character) const
{
    return FindPdfPagePlace(Narration, Unit, Character);
}

std::optional<FTPdfUnitPlace> ReaderPdfNarration::ToUnitPlace(int Page, int Character) const
{
    if (Page < FirstPage || Page > LastPage)
    {
        return std::nullopt;
    }

    return FindPdfUnitPlace(Narration, Page, Character);
}

void ReaderPdfNarration::OnLoaded(quint64 LoadedGeneration, QPdfDocument* Loaded, bool bHasText, double Width)
{
    if (LoadedGeneration != Generation)
    {
        delete Loaded;

        return;
    }

    Document.reset(Loaded);
    bAvailable = bHasText;
    TextWidth = Width;

    if (!bHasText)
    {
        qCInfo(LogApp) << "Reader: this PDF has no text layer to read aloud";
    }

    emit changed();
}

void ReaderPdfNarration::AppendPage()
{
    ++LastPage;
    Pages.push_back(ReadTextPage(*Document, LastPage));
    Narration = BuildPdfNarration(Pages, FirstPage, TextWidth);
}
