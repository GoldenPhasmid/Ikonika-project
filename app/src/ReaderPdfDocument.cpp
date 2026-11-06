#include "ReaderPdfDocument.h"

#include "FTPdfRenderWorker.h"

#include "core/FTLog.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QPointer>
#include <QThreadPool>

namespace
{
    constexpr int ContentSamples = 8;
}

ReaderPdfDocument::ReaderPdfDocument(QObject* Parent)
    : QObject(Parent)
{
    Worker = new FTPdfRenderWorker;
    Worker->moveToThread(&WorkerThread);

    connect(&WorkerThread, &QThread::finished, Worker, &QObject::deleteLater);
    connect(Worker, &FTPdfRenderWorker::Loaded, this, &ReaderPdfDocument::OnLoaded);
    connect(Worker, &FTPdfRenderWorker::Measured, this, &ReaderPdfDocument::OnMeasured);
    connect(Worker, &FTPdfRenderWorker::Rendered, this, &ReaderPdfDocument::rendered);
    connect(Worker, &FTPdfRenderWorker::OutlineReady, this, &ReaderPdfDocument::outlineReady);
    connect(Worker, &FTPdfRenderWorker::PageContentMeasured, this, &ReaderPdfDocument::OnPageContentMeasured);
    connect(Worker, &FTPdfRenderWorker::DocumentContentMeasured, this,
            &ReaderPdfDocument::OnDocumentContentMeasured);

    WorkerThread.start();
}

ReaderPdfDocument::~ReaderPdfDocument()
{
    WorkerThread.quit();
    WorkerThread.wait();
}

QString ReaderPdfDocument::GetSource() const
{
    return Source;
}

void ReaderPdfDocument::SetSource(const QString& NewSource)
{
    if (Source == NewSource)
    {
        return;
    }

    Source = NewSource;
    bReady = false;
    bFailed = false;
    PageCount = 0;
    PageSizes.clear();
    DefaultPageSize = QSizeF(1, 1);
    PageContent.clear();
    DocumentLeft = 0;
    DocumentRight = 1;
    Text.reset();

    emit sourceChanged();
    emit documentChanged();

    if (Source.isEmpty())
    {
        QMetaObject::invokeMethod(Worker, "Close", Qt::QueuedConnection);

        return;
    }

    QMetaObject::invokeMethod(Worker, "Load", Qt::QueuedConnection, Q_ARG(QString, Source));
}

bool ReaderPdfDocument::IsReady() const
{
    return bReady;
}

bool ReaderPdfDocument::HasFailed() const
{
    return bFailed;
}

int ReaderPdfDocument::GetPageCount() const
{
    return PageCount;
}

QSizeF ReaderPdfDocument::PageSizeAt(int Page) const
{
    return PageSizes.value(Page, DefaultPageSize);
}

bool ReaderPdfDocument::IsMeasured(int Page) const
{
    return PageSizes.contains(Page);
}

void ReaderPdfDocument::Measure(int Page)
{
    QMetaObject::invokeMethod(Worker, "Measure", Qt::QueuedConnection, Q_ARG(int, Page));
}

std::pair<qreal, qreal> ReaderPdfDocument::ContentSpan(int Page) const
{
    const auto Content = PageContent.constFind(Page);

    if (Content == PageContent.constEnd())
    {
        return {DocumentLeft, DocumentRight};
    }

    return {qMin(DocumentLeft, Content->first), qMax(DocumentRight, Content->second)};
}

bool ReaderPdfDocument::HasContentSpan(int Page) const
{
    return PageContent.contains(Page);
}

quint64 ReaderPdfDocument::Render(quint64 Owner, bool bSupersedes, int Page, QSize PageImagePixels,
                                  QRect ClipPixels, qreal Scale, uint Paper, uint Ink)
{
    quint64 RequestId = 0;

    if (bSupersedes)
    {
        RequestId = NextRequestId++;
        Worker->SetLatestRequest(Owner, RequestId);
    }

    QMetaObject::invokeMethod(Worker, "Render", Qt::QueuedConnection,
                              Q_ARG(quint64, Owner),
                              Q_ARG(quint64, RequestId),
                              Q_ARG(int, Page),
                              Q_ARG(QSize, PageImagePixels),
                              Q_ARG(QRect, ClipPixels),
                              Q_ARG(qreal, Scale),
                              Q_ARG(uint, Paper),
                              Q_ARG(uint, Ink));

    return RequestId;
}

void ReaderPdfDocument::requestOutline()
{
    QMetaObject::invokeMethod(Worker, "Outline", Qt::QueuedConnection);
}

void ReaderPdfDocument::OnLoaded(bool bSucceeded, int NewPageCount, QSizeF FirstPageSize)
{
    PageCount = NewPageCount;

    if (!bSucceeded || PageCount <= 0 || FirstPageSize.isEmpty())
    {
        bFailed = true;

        emit documentChanged();

        return;
    }

    DefaultPageSize = FirstPageSize;
    PageSizes.insert(0, FirstPageSize);
    LoadTextDocumentInBackground();

    QList<int> SamplePages;

    for (int Sample = 0; Sample < ContentSamples; ++Sample)
    {
        const int SamplePage = PageCount > 2
            ? 1 + (Sample * (PageCount - 2)) / (ContentSamples - 1)
            : Sample;

        if (SamplePage < PageCount && !SamplePages.contains(SamplePage))
        {
            SamplePages.append(SamplePage);
        }
    }

    QMetaObject::invokeMethod(Worker, "MeasureContent", Qt::QueuedConnection,
                              Q_ARG(QList<int>, SamplePages));
}

void ReaderPdfDocument::OnMeasured(int MeasuredPage, QSizeF Size)
{
    if (Size.isEmpty())
    {
        return;
    }

    const QSizeF Assumed = PageSizeAt(MeasuredPage);

    PageSizes.insert(MeasuredPage, Size);
    DefaultPageSize = Size;

    emit pageMeasured(MeasuredPage, Assumed != Size);
}

void ReaderPdfDocument::OnPageContentMeasured(int MeasuredPage, qreal Left, qreal Right)
{
    PageContent.insert(MeasuredPage, {Left, Right});

    emit pageContentMeasured(MeasuredPage);
}

void ReaderPdfDocument::OnDocumentContentMeasured(qreal Left, qreal Right)
{
    DocumentLeft = Left;
    DocumentRight = Right;

    if (!bReady)
    {
        bReady = true;

        emit documentChanged();
    }

    emit contentMeasured();
}

QPdfDocument* ReaderPdfDocument::TextDocument()
{
    if (Text)
    {
        return Text->status() == QPdfDocument::Status::Ready ? Text.get() : nullptr;
    }

    if (Source.isEmpty())
    {
        return nullptr;
    }

    Text = std::make_unique<QPdfDocument>();

    if (Text->load(Source) != QPdfDocument::Error::None)
    {
        qCWarning(LogApp) << "Reader: could not open the text layer of" << Source;

        return nullptr;
    }

    return Text.get();
}

void ReaderPdfDocument::LoadTextDocumentInBackground()
{
    if (Text)
    {
        return;
    }

    const QPointer<ReaderPdfDocument> Guard(this);
    const QString Path = Source;
    QThread* const InterfaceThread = thread();

    QThreadPool::globalInstance()->start([Guard, Path, InterfaceThread]()
    {
        QElapsedTimer Timer;
        Timer.start();

        QPdfDocument* const Loaded = new QPdfDocument;
        const bool bLoaded = Loaded->load(Path) == QPdfDocument::Error::None;
        Loaded->moveToThread(InterfaceThread);

        qCDebug(LogApp) << "Reader: text layer of" << Path << "loaded in" << Timer.elapsed()
                        << "ms, succeeded" << bLoaded;

        QMetaObject::invokeMethod(QCoreApplication::instance(), [Guard, Path, Loaded, bLoaded]()
        {
            if (!Guard || Guard->Source != Path || Guard->Text || !bLoaded)
            {
                delete Loaded;

                return;
            }

            Guard->Text.reset(Loaded);
        }, Qt::QueuedConnection);
    });
}
