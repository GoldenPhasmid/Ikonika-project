#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QPdfDocument>
#include <QRect>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QThread>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <utility>

class FTPdfRenderWorker;

class ReaderPdfDocument : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString source READ GetSource WRITE SetSource NOTIFY sourceChanged)
    Q_PROPERTY(bool ready READ IsReady NOTIFY documentChanged)
    Q_PROPERTY(bool failed READ HasFailed NOTIFY documentChanged)
    Q_PROPERTY(int pageCount READ GetPageCount NOTIFY documentChanged)

public:
    explicit ReaderPdfDocument(QObject* Parent = nullptr);
    ~ReaderPdfDocument() override;

    QString GetSource() const;
    void SetSource(const QString& NewSource);
    bool IsReady() const;
    bool HasFailed() const;
    int GetPageCount() const;

    QSizeF PageSizeAt(int Page) const;
    bool IsMeasured(int Page) const;
    void Measure(int Page);

    std::pair<qreal, qreal> ContentSpan(int Page) const;
    bool HasContentSpan(int Page) const;

    quint64 Render(quint64 Owner, bool bSupersedes, int Page, QSize PageImagePixels, QRect ClipPixels,
                   qreal Scale, uint Paper, uint Ink);

    Q_INVOKABLE void requestOutline();

    QPdfDocument* TextDocument();

signals:
    void sourceChanged();
    void documentChanged();
    void contentMeasured();
    void outlineReady(QVariantList Entries);
    void pageMeasured(int Page, bool bChanged);
    void pageContentMeasured(int Page);
    void rendered(quint64 Owner, quint64 RequestId, int Page, qreal Scale, QSize PageImagePixels,
                  QRect ClipPixels, const QImage& Image);

private:
    void OnLoaded(bool bSucceeded, int NewPageCount, QSizeF FirstPageSize);
    void OnMeasured(int MeasuredPage, QSizeF Size);
    void OnPageContentMeasured(int MeasuredPage, qreal Left, qreal Right);
    void OnDocumentContentMeasured(qreal Left, qreal Right);
    void LoadTextDocumentInBackground();

    QThread WorkerThread;
    FTPdfRenderWorker* Worker = nullptr;

    QString Source;
    bool bReady = false;
    bool bFailed = false;
    int PageCount = 0;

    QHash<int, QSizeF> PageSizes;
    QSizeF DefaultPageSize{1, 1};

    QHash<int, std::pair<qreal, qreal>> PageContent;
    qreal DocumentLeft = 0;
    qreal DocumentRight = 1;

    quint64 NextRequestId = 1;

    std::unique_ptr<QPdfDocument> Text;
};
