#pragma once

#include <QColor>
#include <QHash>
#include <QImage>
#include <QPdfDocument>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "ReaderPdfDocument.h"

/**
 * One page of a PDF, drawn from a bitmap that a worker thread produces.
 *
 * The item never rasterises anything itself and never blocks. What it holds is two pictures of the
 * current page: a cheap one of the whole page, and a sharp one of the part the viewport shows.
 * Both are drawn stretched to wherever the current zoom puts them, so a zoom step is a change of
 * geometry the graphics card applies within a frame, and the sharper bitmap replaces it once the
 * worker has it. The alternative - waiting for the new bitmap before showing anything - is what
 * made zooming feel like it had stopped responding.
 *
 * Rendering a clipped region rather than a whole page is what keeps the cost flat: at any zoom the
 * work is one viewport's worth of pixels, not a whole page's worth.
 *
 * Text queries use a second document of the same file, opened on this thread the first time one is
 * asked for. Sharing the worker's document would mean every click waiting behind a render, which
 * is the stall this class exists to remove.
 */
class ReaderPdfPage : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    /** The file to read. A local path, not a URL. */
    Q_PROPERTY(QString source READ GetSource WRITE SetSource NOTIFY sourceChanged)
    Q_PROPERTY(ReaderPdfDocument* document READ GetDocument WRITE SetDocument NOTIFY documentChanged)

    Q_PROPERTY(bool ready READ IsReady NOTIFY documentChanged)
    Q_PROPERTY(bool failed READ HasFailed NOTIFY documentChanged)
    Q_PROPERTY(int pageCount READ GetPageCount NOTIFY documentChanged)

    /** The page on show, counted from zero. */
    Q_PROPERTY(int page READ GetPage WRITE SetPage NOTIFY pageChanged)

    /** Pixels per point. One point is a seventy-second of an inch, as the file measures it. */
    Q_PROPERTY(qreal renderScale READ GetRenderScale WRITE SetRenderScale NOTIFY renderScaleChanged)

    /** The current page's size in points. */
    Q_PROPERTY(QSizeF pageSize READ GetPageSize NOTIFY pageChanged)

    /** What the viewport shows, in this item's own coordinates. Only this part is rendered sharp. */
    Q_PROPERTY(QRectF visibleArea READ GetVisibleArea WRITE SetVisibleArea NOTIFY visibleAreaChanged)

    Q_PROPERTY(QColor paperColour READ GetPaperColour WRITE SetPaperColour NOTIFY toneChanged)
    Q_PROPERTY(QColor inkColour READ GetInkColour WRITE SetInkColour NOTIFY toneChanged)

    /** Whether the pages either side are rendered ahead. Off when a view already shows them itself. */
    Q_PROPERTY(bool prefetch READ IsPrefetching WRITE SetPrefetching NOTIFY prefetchChanged)

    Q_PROPERTY(qreal cropLeft READ GetCropLeft NOTIFY cropChanged)
    Q_PROPERTY(qreal cropRight READ GetCropRight NOTIFY cropChanged)

public:
    explicit ReaderPdfPage(QQuickItem* Parent = nullptr);

    /** The size of any page in points, from the list read when the file was opened. */
    Q_INVOKABLE QSizeF pageSizeAt(int Page) const;

    /** The word under a point given in this item's coordinates, or empty. */
    Q_INVOKABLE QString wordAt(QPointF ItemPoint);

    /** The same word, with the rectangles covering it, so it can be shown as selected. */
    Q_INVOKABLE QVariantMap selectWordAt(QPointF ItemPoint);

    /** The text between two points, with the rectangles covering it, in this item's coordinates. */
    Q_INVOKABLE QVariantMap selectionBetween(QPointF FromItem, QPointF ToItem);

    Q_INVOKABLE void requestOutline();

    QString GetSource() const;
    void SetSource(const QString& NewSource);
    ReaderPdfDocument* GetDocument() const;
    void SetDocument(ReaderPdfDocument* NewDocument);
    bool IsReady() const;
    bool HasFailed() const;
    int GetPageCount() const;
    int GetPage() const;
    void SetPage(int NewPage);
    qreal GetRenderScale() const;
    void SetRenderScale(qreal NewScale);
    QSizeF GetPageSize() const;
    QRectF GetVisibleArea() const;
    void SetVisibleArea(const QRectF& NewArea);
    QColor GetPaperColour() const;
    void SetPaperColour(const QColor& Colour);
    QColor GetInkColour() const;
    void SetInkColour(const QColor& Colour);
    bool IsPrefetching() const;
    void SetPrefetching(bool bValue);
    qreal GetCropLeft() const;
    qreal GetCropRight() const;

signals:
    /** A new bitmap arrived. True when it is the sharp one rather than the whole-page backdrop. */
    void rendered(bool bDetail);

    void sourceChanged();
    void documentChanged();
    void pageChanged();
    void renderScaleChanged();
    void visibleAreaChanged();
    void toneChanged();
    void prefetchChanged();
    void cropChanged();
    void outlineReady(QVariantList Entries);

protected:
    QSGNode* updatePaintNode(QSGNode* OldNode, UpdatePaintNodeData* Data) override;

private:
    /** One bitmap and what it is a picture of. */
    struct FRenderedLayer
    {
        QImage Image;
        int Page = -1;
        qreal Scale = 0;
        QRect ClipPixels;
        bool bDirty = false;
    };

    void OnDocumentChanged();
    void OnSourceChanged();
    void OnPageMeasured(int MeasuredPage, bool bChanged);
    void OnPageContentMeasured(int MeasuredPage);
    void DropRenders();
    void OnRendered(quint64 Owner, quint64 RequestId, int Page, qreal Scale, QSize PageImagePixels,
                    QRect ClipPixels, const QImage& Image);
    void ResetPage();
    quint64 OwnerKey() const;

    /** The viewport in the page's own pixel space at the scale showing now. */
    QRectF ViewportInPixels() const;

    /** Asks for whatever the current page, scale and viewport need, once things stop changing. */
    void ScheduleRender();
    void IssueRender();

    /** The whole page at a size that costs little, so a zoom or a flip has something to show. */
    void RequestBackdrop();

    /** The pages either side, so turning to one shows it at once instead of after a render. */
    void RequestPrefetch();

    /** Puts the cached whole-page picture of the current page up, if there is one. */
    bool ShowCachedPage();

    /** Forgets whole-page pictures too far from the page being read to be turned to. */
    void TrimCache();

    /** The viewport's own region at full sharpness. */
    void RequestDetail();

    /** A selection as QML wants it: its text, and one rectangle per line in item coordinates. */
    QVariantMap ToSelectionMap(const QPdfSelection& Selection) const;

    qreal DevicePixels() const;

    QPointer<ReaderPdfDocument> Document;
    ReaderPdfDocument* OwnedDocument = nullptr;
    bool bWasReady = false;

    int Page = 0;
    qreal RenderScale = 1;
    QRectF VisibleArea;
    bool bPrefetch = true;

    /** One whole-page picture, held against the page being turned to rather than being read. */
    struct FCachedPage
    {
        QImage Image;
        qreal Scale = 0;
    };

    FRenderedLayer Backdrop;
    FRenderedLayer Detail;

    QHash<int, FCachedPage> WholePages;

    QColor Paper{Qt::white};
    QColor Ink{Qt::black};

    QTimer SettleTimer;
    QTimer PrefetchTimer;
};
