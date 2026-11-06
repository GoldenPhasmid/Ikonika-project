#pragma once

#include <QHash>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPdfDocument>
#include <QRect>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QVariantList>
#include <memory>

/**
 * Off-UI worker thread for asynchronous PDF page rendering via PDFium.
 *
 * Key behaviors:
 * - Thread Isolation: Prevents PDFium's per-document serialization from stalling the UI.
 * - Request Dropping: Automatically discards stale render requests (e.g., during rapid scrolling) to only process the latest ID.
 * - Region Clipping: Renders viewport-clipped regions rather than full pages, keeping performance independent of zoom level.
 */
class FTPdfRenderWorker : public QObject
{
    Q_OBJECT

public:
    explicit FTPdfRenderWorker(QObject* Parent = nullptr);

    void SetLatestRequest(quint64 Owner, quint64 RequestId);

public slots:
    /**
     * Opens a file and reports how many pages it has and how big the first one is.
     *
     * Only the first: asking pdfium for a page's size makes it parse that page, and doing it for a
     * thousand of them took longer than everything else about opening a book put together. The
     * rest are measured when something needs them.
     */
    void Load(const QString& FilePath);

    void Close();

    /** Reports one page's size. Most documents print every page the same, so this rarely differs. */
    void Measure(int Page);

    /**
     * Rasterises part of one page. PageImagePixels is the whole page at the wanted scale;
     * ClipPixels is the part of it to produce, in that same pixel space.
     */
    void Render(quint64 Owner, quint64 RequestId, int Page, QSize PageImagePixels, QRect ClipPixels, qreal Scale,
                uint Paper, uint Ink);

    void MeasureContent(QList<int> Pages);

    void Outline();

signals:
    void Loaded(bool bSucceeded, int PageCount, QSizeF FirstPageSize);
    void Measured(int Page, QSizeF PageSize);
    void Rendered(quint64 Owner, quint64 RequestId, int Page, qreal Scale, QSize PageImagePixels, QRect ClipPixels,
                  const QImage& Image);
    void OutlineReady(QVariantList Entries);
    void PageContentMeasured(int Page, qreal Left, qreal Right);
    void DocumentContentMeasured(qreal Left, qreal Right);

private:
    bool IsSuperseded(quint64 Owner, quint64 RequestId);

    std::unique_ptr<QPdfDocument> Document;

    QMutex LatestMutex;
    QHash<quint64, quint64> LatestRequests;
};
