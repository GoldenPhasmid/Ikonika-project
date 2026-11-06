#include "ReaderPdfPage.h"

#include "core/FTLog.h"
#include "core/FTSentenceSplitter.h"
#include "core/FTSettings.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

#include <algorithm>
#include <cmath>

namespace
{
    /** The longest side of the whole-page picture. Only ever seen stretched, while the sharp one
        is still being made, so it is sized for cheapness rather than for reading. */
    constexpr int BackdropLongestSide = 1000;

    /**
     * How far past the viewport the sharp picture reaches, so a pan needs no new render.
     *
     * Generous downwards and mean sideways, because reading moves down a page and only a zoomed-in
     * reader moves across one.
     */
    constexpr qreal DetailMarginDown = 0.5;
    constexpr qreal DetailMarginAcross = 0.2;

    /**
     * When the whole page is no bigger than this, it is rendered whole rather than clipped.
     *
     * A page at a width that fits the window is only about half again the viewport's own pixels,
     * and rendering all of it once means scrolling through it needs nothing further. Clipping to
     * the viewport there bought nothing and made every few notches of the wheel fall outside the
     * sharp region until a new render caught up.
     */
    constexpr qreal WholePageViewportRatio = 3.0;
    constexpr int WholePagePixels = 8000000;

    /** A ceiling on one render, in case a viewport is enormous. */
    constexpr int MaximumDetailPixels = 12000000;

    /** Requests wait this long for the scale or the viewport to stop moving. */
    constexpr int SettleMilliseconds = 130;

    /**
     * The pages either side are asked for only after this much quiet.
     *
     * Long, deliberately. The worker takes requests in order, so a reader holding the page-forward
     * key would otherwise put a queue of neighbours in front of the page they are actually on.
     * Waiting means the prefetch only happens once they have settled somewhere, which is the only
     * time it is worth anything.
     */
    constexpr int PrefetchMilliseconds = 500;

    /** How far either side of the page being read a whole-page picture is worth keeping. */
    constexpr int CacheReach = 2;

    bool IsWordCharacter(QChar Character)
    {
        return Character.isLetter() || Character.isDigit()
            || Character == QLatin1Char('\'') || Character == QChar(0x2019)
            || Character == QLatin1Char('-');
    }
}

ReaderPdfPage::ReaderPdfPage(QQuickItem* Parent)
    : QQuickItem(Parent)
{
    setFlag(ItemHasContents, true);

    SettleTimer.setSingleShot(true);
    SettleTimer.setInterval(SettleMilliseconds);
    connect(&SettleTimer, &QTimer::timeout, this, &ReaderPdfPage::IssueRender);

    PrefetchTimer.setSingleShot(true);
    PrefetchTimer.setInterval(PrefetchMilliseconds);
    connect(&PrefetchTimer, &QTimer::timeout, this, &ReaderPdfPage::RequestPrefetch);
}

quint64 ReaderPdfPage::OwnerKey() const
{
    return static_cast<quint64>(reinterpret_cast<quintptr>(this));
}

QString ReaderPdfPage::GetSource() const
{
    return Document ? Document->GetSource() : QString();
}

void ReaderPdfPage::SetSource(const QString& NewSource)
{
    if (!Document)
    {
        OwnedDocument = new ReaderPdfDocument(this);
        SetDocument(OwnedDocument);
    }

    if (Document != OwnedDocument)
    {
        qCWarning(LogApp) << "Reader: a page reading a shared document was given a source of its own" << NewSource;

        return;
    }

    Document->SetSource(NewSource);
}

ReaderPdfDocument* ReaderPdfPage::GetDocument() const
{
    return Document;
}

void ReaderPdfPage::SetDocument(ReaderPdfDocument* NewDocument)
{
    if (Document == NewDocument)
    {
        return;
    }

    if (Document)
    {
        disconnect(Document, nullptr, this, nullptr);
    }

    if (OwnedDocument && OwnedDocument != NewDocument)
    {
        OwnedDocument->deleteLater();
        OwnedDocument = nullptr;
    }

    Document = NewDocument;

    if (Document)
    {
        connect(Document, &ReaderPdfDocument::sourceChanged, this, &ReaderPdfPage::OnSourceChanged);
        connect(Document, &ReaderPdfDocument::documentChanged, this, &ReaderPdfPage::OnDocumentChanged);
        connect(Document, &ReaderPdfDocument::contentMeasured, this, &ReaderPdfPage::cropChanged);
        connect(Document, &ReaderPdfDocument::pageMeasured, this, &ReaderPdfPage::OnPageMeasured);
        connect(Document, &ReaderPdfDocument::pageContentMeasured, this, &ReaderPdfPage::OnPageContentMeasured);
        connect(Document, &ReaderPdfDocument::rendered, this, &ReaderPdfPage::OnRendered);
        connect(Document, &ReaderPdfDocument::outlineReady, this, &ReaderPdfPage::outlineReady);
    }

    OnSourceChanged();
}

void ReaderPdfPage::ResetPage()
{
    Page = 0;
    bWasReady = false;
    Backdrop = FRenderedLayer{};
    Detail = FRenderedLayer{};
    WholePages.clear();

    emit pageChanged();
    emit cropChanged();

    update();
}

void ReaderPdfPage::OnSourceChanged()
{
    ResetPage();

    emit sourceChanged();

    OnDocumentChanged();
}

void ReaderPdfPage::OnDocumentChanged()
{
    emit documentChanged();

    if (!IsReady() || bWasReady)
    {
        bWasReady = IsReady();

        return;
    }

    bWasReady = true;

    emit pageChanged();
    emit cropChanged();

    IssueRender();
}

void ReaderPdfPage::OnPageContentMeasured(int MeasuredPage)
{
    if (MeasuredPage == Page)
    {
        emit cropChanged();
    }
}

qreal ReaderPdfPage::GetCropLeft() const
{
    return Document ? Document->ContentSpan(Page).first * GetPageSize().width() : 0;
}

qreal ReaderPdfPage::GetCropRight() const
{
    return Document ? Document->ContentSpan(Page).second * GetPageSize().width() : GetPageSize().width();
}

bool ReaderPdfPage::IsPrefetching() const
{
    return bPrefetch;
}

void ReaderPdfPage::SetPrefetching(bool bValue)
{
    if (bPrefetch == bValue)
    {
        return;
    }

    bPrefetch = bValue;

    emit prefetchChanged();
}

QColor ReaderPdfPage::GetPaperColour() const
{
    return Paper;
}

void ReaderPdfPage::SetPaperColour(const QColor& Colour)
{
    if (Paper == Colour)
    {
        return;
    }

    Paper = Colour;
    DropRenders();
}

QColor ReaderPdfPage::GetInkColour() const
{
    return Ink;
}

void ReaderPdfPage::SetInkColour(const QColor& Colour)
{
    if (Ink == Colour)
    {
        return;
    }

    Ink = Colour;
    DropRenders();
}

void ReaderPdfPage::DropRenders()
{
    Backdrop = FRenderedLayer{};
    Detail = FRenderedLayer{};
    WholePages.clear();

    emit toneChanged();

    IssueRender();
    update();
}

void ReaderPdfPage::OnPageMeasured(int MeasuredPage, bool bChanged)
{
    if (!bChanged)
    {
        return;
    }

    WholePages.remove(MeasuredPage);

    if (MeasuredPage != Page)
    {
        return;
    }

    Backdrop = FRenderedLayer{};
    Detail = FRenderedLayer{};

    emit pageChanged();

    IssueRender();
    update();
}

bool ReaderPdfPage::IsReady() const
{
    return Document && Document->IsReady();
}

bool ReaderPdfPage::HasFailed() const
{
    return Document && Document->HasFailed();
}

int ReaderPdfPage::GetPageCount() const
{
    return Document ? Document->GetPageCount() : 0;
}

int ReaderPdfPage::GetPage() const
{
    return Page;
}

void ReaderPdfPage::SetPage(int NewPage)
{
    const int Clamped = qBound(0, NewPage, qMax(0, GetPageCount() - 1));

    if (Page == Clamped)
    {
        return;
    }

    Page = Clamped;

    emit pageChanged();
    emit cropChanged();

    // Whatever was prefetched for this page, up straight away. This is the whole point of holding
    // the neighbours: a turn shows the page rather than an empty sheet waiting for a render.
    ShowCachedPage();
    TrimCache();

    // Its real size, if this page has never been shown. The render below assumes the size of the
    // page that stood in for it, which is right for every document that prints uniformly.
    if (Document && !Document->IsMeasured(Page))
    {
        Document->Measure(Page);
    }

    // Straight away rather than through the settle timer: a page turn has nothing to show at all
    // until something is rendered, so there is no picture worth delaying for.
    IssueRender();
    update();
}

qreal ReaderPdfPage::GetRenderScale() const
{
    return RenderScale;
}

void ReaderPdfPage::SetRenderScale(qreal NewScale)
{
    if (qFuzzyCompare(RenderScale, NewScale) || NewScale <= 0)
    {
        return;
    }

    RenderScale = NewScale;

    emit renderScaleChanged();

    // The bitmaps already held are redrawn at the new size at once; a sharper one is asked for
    // only when the scale stops changing.
    ScheduleRender();
    update();
}

QSizeF ReaderPdfPage::GetPageSize() const
{
    return pageSizeAt(Page);
}

QSizeF ReaderPdfPage::pageSizeAt(int InPage) const
{
    return Document ? Document->PageSizeAt(InPage) : QSizeF(1, 1);
}

QRectF ReaderPdfPage::GetVisibleArea() const
{
    return VisibleArea;
}

void ReaderPdfPage::SetVisibleArea(const QRectF& NewArea)
{
    if (VisibleArea == NewArea)
    {
        return;
    }

    VisibleArea = NewArea;

    emit visibleAreaChanged();

    ScheduleRender();
}

qreal ReaderPdfPage::DevicePixels() const
{
    return window() ? window()->effectiveDevicePixelRatio() : 1.0;
}

void ReaderPdfPage::ScheduleRender()
{
    if (!IsReady())
    {
        return;
    }

    SettleTimer.start();
}

void ReaderPdfPage::IssueRender()
{
    if (!IsReady())
    {
        return;
    }

    SettleTimer.stop();

    // The sharp one first. The worker takes them in order, and what the reader is looking at should
    // not wait behind the stand-in for it, let alone behind a page they have not turned to.
    RequestDetail();
    RequestBackdrop();

    if (bPrefetch)
    {
        PrefetchTimer.start();
    }
}

bool ReaderPdfPage::ShowCachedPage()
{
    const auto Cached = WholePages.constFind(Page);

    if (Cached == WholePages.constEnd())
    {
        return false;
    }

    Backdrop.Image = Cached->Image;
    Backdrop.Page = Page;
    Backdrop.Scale = Cached->Scale;
    Backdrop.ClipPixels = QRect(QPoint(0, 0), Cached->Image.size());
    Backdrop.bDirty = true;

    update();

    return true;
}

void ReaderPdfPage::TrimCache()
{
    for (auto Entry = WholePages.begin(); Entry != WholePages.end();)
    {
        if (qAbs(Entry.key() - Page) > CacheReach)
        {
            Entry = WholePages.erase(Entry);

            continue;
        }

        ++Entry;
    }
}

void ReaderPdfPage::RequestPrefetch()
{
    if (!IsReady() || !bPrefetch)
    {
        return;
    }

    for (const int Neighbour : {Page - 1, Page + 1})
    {
        if (Neighbour < 0 || Neighbour >= GetPageCount() || WholePages.contains(Neighbour))
        {
            continue;
        }

        // Measured as well as rendered: a page of a different size would otherwise be drawn at the
        // wrong one for as long as it took its own measurement to come back.
        if (!Document->IsMeasured(Neighbour))
        {
            Document->Measure(Neighbour);
        }

        const QSizeF Points = pageSizeAt(Neighbour);

        if (Points.isEmpty())
        {
            continue;
        }

        const qreal Scale = BackdropLongestSide / qMax(Points.width(), Points.height());
        const QSize Pixels(qMax(1, qRound(Points.width() * Scale)),
                           qMax(1, qRound(Points.height() * Scale)));

        Document->Render(OwnerKey(), false, Neighbour, Pixels, QRect(QPoint(0, 0), Pixels), Scale, Paper.rgb(),
                         Ink.rgb());
    }
}

QRectF ReaderPdfPage::ViewportInPixels() const
{
    const qreal Device = DevicePixels();

    return QRectF(VisibleArea.x() * Device, VisibleArea.y() * Device,
                  VisibleArea.width() * Device, VisibleArea.height() * Device);
}

void ReaderPdfPage::RequestBackdrop()
{
    if ((Backdrop.Page == Page && !Backdrop.Image.isNull()) || WholePages.contains(Page))
    {
        return;
    }

    const QSizeF Points = GetPageSize();

    if (Points.isEmpty())
    {
        return;
    }

    // Nothing to stand in for: the sharp picture about to be asked for already covers the whole
    // page, which is what happens at any zoom that fits it. Rendering it twice would only make a
    // page turn cost double.
    const qreal ShowingAt = RenderScale * DevicePixels();
    const QRectF WholePage(0, 0, Points.width() * ShowingAt, Points.height() * ShowingAt);

    if (!VisibleArea.isEmpty() && ViewportInPixels().contains(WholePage) && Document->HasContentSpan(Page))
    {
        return;
    }

    const qreal Scale = BackdropLongestSide / qMax(Points.width(), Points.height());
    const QSize BackdropPixels(qMax(1, qRound(Points.width() * Scale)),
                               qMax(1, qRound(Points.height() * Scale)));

    // Not droppable: this is the only picture a freshly turned page has, and it is cheap.
    Document->Render(OwnerKey(), false, Page, BackdropPixels, QRect(QPoint(0, 0), BackdropPixels), Scale,
                     Paper.rgb(), Ink.rgb());
}

void ReaderPdfPage::RequestDetail()
{
    const QSizeF Points = GetPageSize();

    if (Points.isEmpty() || VisibleArea.isEmpty())
    {
        return;
    }

    const qreal Pixels = RenderScale * DevicePixels();
    const QRectF PageRectangle(0, 0, Points.width() * Pixels, Points.height() * Pixels);
    const QRectF Viewport = ViewportInPixels();

    // Rounded outwards, then held inside the page: a clip a pixel taller than the page comes back
    // a pixel short, and the bitmap would then be drawn very slightly stretched.
    const QRect WholePage(0, 0, static_cast<int>(PageRectangle.width()),
                          static_cast<int>(PageRectangle.height()));

    // The whole page when that is not much more work than the part on show, so scrolling through
    // it asks for nothing further. Asked for even while the page is out of view, which is what lets
    // a page next to the one being read arrive sharp when it is scrolled to.
    const qreal PagePixels = PageRectangle.width() * PageRectangle.height();
    const qreal ViewportPixels = qMax(qreal(1), Viewport.width() * Viewport.height());
    const bool bWholePage = PagePixels <= WholePagePixels && PagePixels <= ViewportPixels * WholePageViewportRatio;

    // What has to be sharp right now. What is asked for is wider, so that panning a little needs no
    // new render at all.
    const QRect Needed = bWholePage ? WholePage : Viewport.intersected(PageRectangle).toAlignedRect().intersected(WholePage);

    if (Needed.isEmpty())
    {
        return;
    }

    // The picture in hand already covers it at this scale, which is the whole point of asking for
    // more than the viewport. Without this a wheel notch would re-render on every notch.
    if (Detail.Page == Page && qFuzzyCompare(Detail.Scale, Pixels) && Detail.ClipPixels.contains(Needed))
    {
        return;
    }

    QRectF Wanted = PageRectangle;

    if (!bWholePage)
    {
        Wanted = Viewport;
        Wanted.adjust(-Wanted.width() * DetailMarginAcross, -Wanted.height() * DetailMarginDown,
                      Wanted.width() * DetailMarginAcross, Wanted.height() * DetailMarginDown);
    }

    const QRect Clip = Wanted.intersected(PageRectangle).toAlignedRect().intersected(WholePage);

    if (Clip.isEmpty() || static_cast<qint64>(Clip.width()) * Clip.height() > MaximumDetailPixels)
    {
        return;
    }

    Document->Render(OwnerKey(), true, Page, PageRectangle.size().toSize(), Clip, Pixels, Paper.rgb(), Ink.rgb());
}

void ReaderPdfPage::OnRendered(quint64 Owner, quint64 RequestId, int RenderedPage, qreal Scale,
                               QSize PageImagePixels, QRect ClipPixels, const QImage& Image)
{
    if (Image.isNull() || (RequestId != 0 && Owner != OwnerKey()))
    {
        return;
    }

    const QSizeF Points = pageSizeAt(RenderedPage);
    const QSizeF Expected(Points.width() * Scale, Points.height() * Scale);

    if (qAbs(Expected.width() - PageImagePixels.width()) > 1
        || qAbs(Expected.height() - PageImagePixels.height()) > 1)
    {
        qCDebug(LogApp) << "Reader: dropped a render of page" << RenderedPage << "made at"
                        << PageImagePixels << "for a page now known to be" << Expected;

        if (RenderedPage == Page)
        {
            IssueRender();
        }

        return;
    }

    // A whole-page picture is kept whichever page it is of: for the page being read it is what
    // stands in until the sharp one lands, and for a neighbour it is what makes turning there
    // instant.
    if (RequestId == 0)
    {
        WholePages.insert(RenderedPage, FCachedPage{Image, Scale});
        TrimCache();

        if (RenderedPage != Page)
        {
            return;
        }

        ShowCachedPage();

        emit rendered(false);

        return;
    }

    if (RenderedPage != Page)
    {
        return;
    }

    Detail.Image = Image;
    Detail.Page = RenderedPage;
    Detail.Scale = Scale;
    Detail.ClipPixels = ClipPixels;
    Detail.bDirty = true;

    emit rendered(true);

    update();
}

QSGNode* ReaderPdfPage::updatePaintNode(QSGNode* OldNode, UpdatePaintNodeData*)
{
    QSGNode* Root = OldNode;

    if (!Root)
    {
        Root = new QSGNode;
    }

    // Two children in a fixed order, so the sharp picture always draws over the cheap one.
    for (int Index = 0; Index < 2; ++Index)
    {
        FRenderedLayer& Layer = Index == 0 ? Backdrop : Detail;
        QSGSimpleTextureNode* Node = static_cast<QSGSimpleTextureNode*>(
            Root->childCount() > Index ? Root->childAtIndex(Index) : nullptr);

        const bool bUsable = Layer.Page == Page && !Layer.Image.isNull() && Layer.Scale > 0;

        if (!bUsable)
        {
            if (Node)
            {
                Root->removeChildNode(Node);
                delete Node;
            }

            continue;
        }

        if (!Node)
        {
            Node = new QSGSimpleTextureNode;
            Node->setOwnsTexture(true);
            Node->setFiltering(QSGTexture::Linear);
            Root->appendChildNode(Node);
            Layer.bDirty = true;
        }

        if (Layer.bDirty)
        {
            Node->setTexture(window()->createTextureFromImage(Layer.Image));
            Layer.bDirty = false;
        }

        // Where that bitmap belongs at the scale showing now. Zooming changes only this, which is
        // why it costs a frame rather than a render.
        const qreal Ratio = RenderScale / Layer.Scale;

        Node->setRect(QRectF(Layer.ClipPixels.x() * Ratio, Layer.ClipPixels.y() * Ratio,
                             Layer.ClipPixels.width() * Ratio, Layer.ClipPixels.height() * Ratio));
    }

    return Root;
}

QString ReaderPdfPage::wordAt(QPointF ItemPoint)
{
    return selectWordAt(ItemPoint).value(QStringLiteral("text")).toString();
}

QVariantMap ReaderPdfPage::selectWordAt(QPointF ItemPoint)
{
    QVariantMap Empty;
    Empty[QStringLiteral("text")] = QString();
    Empty[QStringLiteral("rectangles")] = QVariantList();

    QPdfDocument* const TextLayer = Document ? Document->TextDocument() : nullptr;

    if (!TextLayer || RenderScale <= 0)
    {
        return Empty;
    }

    const QPointF Point(ItemPoint.x() / RenderScale, ItemPoint.y() / RenderScale);

    // Widening reaches, because a bare point finds nothing in most documents and a click rarely
    // lands exactly on a character's own box. The first reach that finds text is used.
    QPdfSelection At = TextLayer->getSelection(Page, Point, Point);

    for (const qreal Reach : {qreal(1), qreal(3), qreal(6)})
    {
        if (At.isValid() && !At.text().isEmpty())
        {
            break;
        }

        At = TextLayer->getSelection(Page, QPointF(Point.x() - Reach, Point.y()),
                                        QPointF(Point.x() + Reach, Point.y()));
    }

    if (!At.isValid() || At.text().isEmpty())
    {
        qCDebug(LogApp) << "Reader: no text at" << Point << "on page" << Page;

        return Empty;
    }

    // The middle of what the reach found, which is the character the click was actually over. Its
    // start would be a character or two to the left, and near a word's edge that is the word
    // before it.
    const int Clicked = (At.startIndex() + At.endIndex()) / 2;

    // Widen to a run of text around that character, then walk out to the word's own edges. Doing
    // it on the text is what makes a word a word here: the document's own runs split wherever the
    // typography did, which is not where a reader would say a word ends.
    constexpr int Reach = 40;
    const int Start = qMax(0, Clicked - Reach);
    const QPdfSelection Around = TextLayer->getSelectionAtIndex(Page, Start, Reach * 2);

    if (!Around.isValid())
    {
        qCDebug(LogApp) << "Reader: no run around index" << Clicked << "on page" << Page;

        return Empty;
    }

    const QString Text = Around.text();
    const int Index = Clicked - Around.startIndex();

    if (Index < 0 || Index >= Text.length() || !IsWordCharacter(Text.at(Index)))
    {
        qCDebug(LogApp) << "Reader: index" << Index << "is not on a word in" << Text;

        return Empty;
    }

    int First = Index;
    int Last = Index;

    while (First > 0 && IsWordCharacter(Text.at(First - 1)))
    {
        --First;
    }

    while (Last + 1 < Text.length() && IsWordCharacter(Text.at(Last + 1)))
    {
        ++Last;
    }

    // Asked for again by index, this time as just the word, so it comes back with the shapes that
    // cover it and a double-click can show what it picked.
    const QPdfSelection Word = TextLayer->getSelectionAtIndex(Page, Around.startIndex() + First,
                                                                Last - First + 1);

    if (!Word.isValid())
    {
        return Empty;
    }

    qCDebug(LogApp) << "Reader: word at" << Point << "is" << Word.text();

    return ToSelectionMap(Word);
}

QVariantMap ReaderPdfPage::selectionBetween(QPointF FromItem, QPointF ToItem)
{
    QVariantMap Empty;
    Empty[QStringLiteral("text")] = QString();
    Empty[QStringLiteral("rectangles")] = QVariantList();

    QPdfDocument* const TextLayer = Document ? Document->TextDocument() : nullptr;

    if (!TextLayer || RenderScale <= 0)
    {
        return Empty;
    }

    const QPointF From(FromItem.x() / RenderScale, FromItem.y() / RenderScale);
    const QPointF To(ToItem.x() / RenderScale, ToItem.y() / RenderScale);

    const QPdfSelection Selection = TextLayer->getSelection(Page, From, To);

    if (!Selection.isValid())
    {
        return Empty;
    }

    return ToSelectionMap(Selection);
}

void ReaderPdfPage::requestOutline()
{
    if (Document)
    {
        Document->requestOutline();
    }
}

QVariantMap ReaderPdfPage::ToSelectionMap(const QPdfSelection& Selection) const
{
    // The document gives one shape per character, which drawn as they come reads as a row of
    // separate blocks rather than as a selection. Characters sharing a line are merged into the
    // run that covers them, the way selected text looks anywhere else.
    QList<QRectF> Lines;

    for (const QPolygonF& Bound : Selection.bounds())
    {
        const QRectF Character = Bound.boundingRect();

        if (Character.isEmpty())
        {
            continue;
        }

        const auto SameLine = std::find_if(Lines.begin(), Lines.end(), [&Character](const QRectF& Line)
        {
            // Overlapping by most of their height is what puts two characters on one line. Their
            // tops rarely agree exactly, since a letter with a tail is taller than one without.
            const qreal Overlap = qMin(Line.bottom(), Character.bottom())
                                - qMax(Line.top(), Character.top());

            return Overlap > qMin(Line.height(), Character.height()) * 0.5;
        });

        if (SameLine == Lines.end())
        {
            Lines.append(Character);

            continue;
        }

        *SameLine = SameLine->united(Character);
    }

    QVariantList Rectangles;

    for (const QRectF& Line : Lines)
    {
        Rectangles.append(QRectF(Line.x() * RenderScale, Line.y() * RenderScale,
                                 Line.width() * RenderScale, Line.height() * RenderScale));
    }

    QVariantMap Result;
    Result[QStringLiteral("text")] = Selection.text();
    const QPdfSelection PageText = Document->TextDocument()->getAllText(Page);
    Result[QStringLiteral("sentence")] = FindSentenceAt(PageText.text(), Selection.startIndex() - PageText.startIndex(),
                                                        FTSettings::Get().GetLanguageRules().TargetLanguage);
    Result[QStringLiteral("rectangles")] = Rectangles;

    return Result;
}
