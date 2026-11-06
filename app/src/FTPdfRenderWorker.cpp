#include "FTPdfRenderWorker.h"

#include "core/FTLog.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QPdfBookmarkModel>
#include <QPdfDocumentRenderOptions>
#include <QSizeF>
#include <QVariantMap>
#include <QVector>

#include <array>
#include <optional>
#include <utility>

namespace
{
    constexpr int OutlineEntryCap = 5000;
    constexpr qint64 OutlineTimeCapMs = 3000;

    constexpr int ContentSampleLongestSide = 400;
    constexpr int InkThreshold = 200;
    constexpr qreal FullBleedWidth = 0.96;

    QImage RenderOnWhite(QPdfDocument& Document, int Page, QSize PageImagePixels, QRect ClipPixels)
    {
        QPdfDocumentRenderOptions Options;
        Options.setScaledSize(PageImagePixels);
        Options.setScaledClipRect(ClipPixels);

        const QImage Transparent = Document.render(Page, ClipPixels.size(), Options);

        QImage Image(Transparent.size(), QImage::Format_RGB32);
        Image.fill(Qt::white);

        QPainter Painter(&Image);
        Painter.drawImage(0, 0, Transparent);
        Painter.end();

        return Image;
    }

    bool HasInk(QRgb Pixel)
    {
        return qMin(qRed(Pixel), qMin(qGreen(Pixel), qBlue(Pixel))) < InkThreshold;
    }

    std::optional<std::pair<qreal, qreal>> MeasureInkColumns(const QImage& Image)
    {
        int First = -1;
        int Last = -1;

        for (int Y = 0; Y < Image.height(); Y += 2)
        {
            const QRgb* Line = reinterpret_cast<const QRgb*>(Image.constScanLine(Y));

            for (int X = 0; X < Image.width(); ++X)
            {
                if (!HasInk(Line[X]))
                {
                    continue;
                }

                First = First < 0 ? X : qMin(First, X);
                Last = qMax(Last, X);
            }
        }

        if (First < 0 || Image.width() <= 0)
        {
            return std::nullopt;
        }

        return std::make_pair(qreal(First) / Image.width(), qreal(Last + 1) / Image.width());
    }

    void ApplyTone(QImage& Image, QRgb Paper, QRgb Ink)
    {
        if (Paper == qRgb(255, 255, 255) && Ink == qRgb(0, 0, 0))
        {
            return;
        }

        std::array<std::array<uchar, 256>, 3> Lookup;

        for (int Level = 0; Level < 256; ++Level)
        {
            const auto Mix = [Level](int From, int To)
            {
                return static_cast<uchar>(From + ((To - From) * Level) / 255);
            };

            Lookup[0][Level] = Mix(qRed(Ink), qRed(Paper));
            Lookup[1][Level] = Mix(qGreen(Ink), qGreen(Paper));
            Lookup[2][Level] = Mix(qBlue(Ink), qBlue(Paper));
        }

        for (int Y = 0; Y < Image.height(); ++Y)
        {
            QRgb* Line = reinterpret_cast<QRgb*>(Image.scanLine(Y));

            for (int X = 0; X < Image.width(); ++X)
            {
                const QRgb Pixel = Line[X];
                Line[X] = qRgb(Lookup[0][qRed(Pixel)], Lookup[1][qGreen(Pixel)], Lookup[2][qBlue(Pixel)]);
            }
        }
    }
}

FTPdfRenderWorker::FTPdfRenderWorker(QObject* Parent)
    : QObject(Parent)
{
}

void FTPdfRenderWorker::Load(const QString& FilePath)
{
    QElapsedTimer Timer;
    Timer.start();

    Document = std::make_unique<QPdfDocument>();

    const QPdfDocument::Error Result = Document->load(FilePath);

    if (Result != QPdfDocument::Error::None)
    {
        qCWarning(LogApp) << "Reader: could not load" << FilePath << "error" << int(Result);

        emit Loaded(false, 0, {});

        return;
    }

    qCInfo(LogApp) << "Reader: loaded" << Document->pageCount() << "pages in"
                   << Timer.elapsed() << "ms";

    emit Loaded(true, Document->pageCount(), Document->pagePointSize(0));
}

void FTPdfRenderWorker::Close()
{
    if (!Document)
    {
        return;
    }

    Document.reset();

    qCInfo(LogApp) << "Reader: closed the rendered document";
}

void FTPdfRenderWorker::Measure(int Page)
{
    if (!Document || Document->status() != QPdfDocument::Status::Ready
        || Page < 0 || Page >= Document->pageCount())
    {
        return;
    }

    emit Measured(Page, Document->pagePointSize(Page));
}

void FTPdfRenderWorker::SetLatestRequest(quint64 Owner, quint64 RequestId)
{
    const QMutexLocker Locker(&LatestMutex);

    LatestRequests.insert(Owner, RequestId);
}

bool FTPdfRenderWorker::IsSuperseded(quint64 Owner, quint64 RequestId)
{
    const QMutexLocker Locker(&LatestMutex);

    return RequestId < LatestRequests.value(Owner, 0);
}

void FTPdfRenderWorker::Render(quint64 Owner, quint64 RequestId, int Page, QSize PageImagePixels, QRect ClipPixels,
                               qreal Scale, uint Paper, uint Ink)
{
    // Superseded while it waited in the queue. Rendering it would delay the one that matters.
    // Identifier zero is the whole-page picture, which is never superseded: it is cheap, and it is
    // the only thing a freshly turned page has to show.
    if (RequestId != 0 && IsSuperseded(Owner, RequestId))
    {
        return;
    }

    if (!Document || Document->status() != QPdfDocument::Status::Ready
        || Page < 0 || Page >= Document->pageCount()
        || ClipPixels.isEmpty() || PageImagePixels.isEmpty())
    {
        return;
    }

    QElapsedTimer Timer;
    Timer.start();

    QImage Image = RenderOnWhite(*Document, Page, PageImagePixels, ClipPixels);

    if (RequestId == 0)
    {
        if (const auto Columns = MeasureInkColumns(Image); Columns.has_value())
        {
            emit PageContentMeasured(Page, Columns->first, Columns->second);
        }
    }

    ApplyTone(Image, Paper, Ink);

    qCDebug(LogApp) << "Reader: rendered page" << Page << "clip" << ClipPixels
                    << "of" << PageImagePixels << "in" << Timer.elapsed() << "ms";

    emit Rendered(Owner, RequestId, Page, Scale, PageImagePixels, ClipPixels, Image);
}

void FTPdfRenderWorker::MeasureContent(QList<int> Pages)
{
    if (!Document || Document->status() != QPdfDocument::Status::Ready)
    {
        return;
    }

    QElapsedTimer Timer;
    Timer.start();

    qreal Left = 1;
    qreal Right = 0;

    for (const int Page : Pages)
    {
        if (Page < 0 || Page >= Document->pageCount())
        {
            continue;
        }

        const QSizeF Points = Document->pagePointSize(Page);

        if (Points.isEmpty())
        {
            continue;
        }

        const qreal Scale = ContentSampleLongestSide / qMax(Points.width(), Points.height());
        const QSize Pixels(qMax(1, qRound(Points.width() * Scale)), qMax(1, qRound(Points.height() * Scale)));
        const auto Columns = MeasureInkColumns(RenderOnWhite(*Document, Page, Pixels, QRect(QPoint(0, 0), Pixels)));

        if (!Columns.has_value() || Columns->second - Columns->first >= FullBleedWidth)
        {
            continue;
        }

        Left = qMin(Left, Columns->first);
        Right = qMax(Right, Columns->second);
    }

    if (Right <= Left)
    {
        Left = 0;
        Right = 1;
    }

    qCInfo(LogApp) << "Reader: text spans" << Left << "to" << Right << "of the page width, measured over"
                   << Pages.size() << "pages in" << Timer.elapsed() << "ms";

    emit DocumentContentMeasured(Left, Right);
}

void FTPdfRenderWorker::Outline()
{
    QVariantList Result;

    if (!Document || Document->status() != QPdfDocument::Status::Ready)
    {
        emit OutlineReady(Result);

        return;
    }

    QElapsedTimer Timer;
    Timer.start();

    QPdfBookmarkModel BookmarkModel;
    BookmarkModel.setDocument(Document.get());

    const qint64 BuildElapsed = Timer.elapsed();

    QVector<QModelIndex> Stack;

    for (int Row = BookmarkModel.rowCount(QModelIndex()) - 1; Row >= 0; --Row)
    {
        Stack.append(BookmarkModel.index(Row, 0, QModelIndex()));
    }

    bool bTruncated = false;

    while (!Stack.isEmpty())
    {
        if (Result.size() >= OutlineEntryCap || Timer.elapsed() - BuildElapsed > OutlineTimeCapMs)
        {
            bTruncated = true;

            break;
        }

        const QModelIndex Index = Stack.takeLast();

        QVariantMap Entry;
        Entry[QStringLiteral("title")] =
            BookmarkModel.data(Index, int(QPdfBookmarkModel::Role::Title)).toString();
        Entry[QStringLiteral("page")] =
            BookmarkModel.data(Index, int(QPdfBookmarkModel::Role::Page)).toInt();
        Entry[QStringLiteral("level")] =
            BookmarkModel.data(Index, int(QPdfBookmarkModel::Role::Level)).toInt();

        Result.append(Entry);

        for (int Row = BookmarkModel.rowCount(Index) - 1; Row >= 0; --Row)
        {
            Stack.append(BookmarkModel.index(Row, 0, Index));
        }
    }

    qCDebug(LogApp) << "Reader: outline" << Result.size() << "entries, build" << BuildElapsed
                    << "ms walk" << Timer.elapsed() - BuildElapsed << "ms truncated" << bTruncated;

    emit OutlineReady(Result);
}
