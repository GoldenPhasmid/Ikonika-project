#pragma once

#include "FTParagraphModel.h"
#include "ReaderVocabulary.h"

#include "core/FTReaderDocument.h"

#include <QObject>
#include <QPointF>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <cstdint>
#include <optional>

/**
 * The book reader's library and document text for QML.
 *
 * Owns nothing about the dictionary: word lookup in the reading view is done by the same
 * DictionaryController the Dictionary screen uses. This class only lists the documents the user
 * has opened, opens one, and hands its paragraphs to the screen. PDFs render themselves in QML,
 * so for them it only hands over the file path. A reflow format's text is read off disk on a
 * worker, since a large file takes a moment to pull apart.
 */
class ReaderController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    /** The library as a list of {id, title, format, path, exists}. */
    Q_PROPERTY(QVariantList documents READ GetDocuments NOTIFY documentsChanged)

    Q_PROPERTY(QString currentId READ GetCurrentId NOTIFY currentDocumentChanged)

    /** The open document's title, or empty when none is open. */
    Q_PROPERTY(QString currentTitle READ GetCurrentTitle NOTIFY currentDocumentChanged)

    /** The open document's file path, so QML can load it (PDFs render themselves). */
    Q_PROPERTY(QString currentPath READ GetCurrentPath NOTIFY currentDocumentChanged)

    /** True when the open document is a PDF, which the screen renders instead of reflowing. */
    Q_PROPERTY(bool isPdf READ IsPdf NOTIFY currentDocumentChanged)

    /** The open document's paragraphs, in reading order (reflow formats only). */
    Q_PROPERTY(QVariantList chapters READ GetChapters NOTIFY currentDocumentChanged)

    Q_PROPERTY(FTParagraphModel* paragraphModel READ GetParagraphModel NOTIFY currentDocumentChanged)

    Q_PROPERTY(ReaderVocabulary* vocabulary READ GetVocabulary CONSTANT)

    /** True while a reflow document's text is still being read off disk. */
    Q_PROPERTY(bool loading READ IsLoading NOTIFY currentDocumentChanged)

    /** True when a document is open, readable or not. */
    Q_PROPERTY(bool reading READ IsReading NOTIFY currentDocumentChanged)

    /** True when the open document had no readable text (a reflow format that yielded nothing). */
    Q_PROPERTY(bool unreadable READ IsUnreadable NOTIFY currentDocumentChanged)

    Q_PROPERTY(int currentPosition READ GetCurrentPosition NOTIFY currentDocumentChanged)
    Q_PROPERTY(qreal currentZoom READ GetCurrentZoom NOTIFY currentDocumentChanged)
    Q_PROPERTY(QPointF currentScroll READ GetCurrentScroll NOTIFY currentDocumentChanged)

    Q_PROPERTY(bool fullScreen READ IsFullScreen WRITE setFullScreen NOTIFY fullScreenChanged)

    Q_PROPERTY(bool cloudAvailable READ IsCloudAvailable NOTIFY cloudAvailabilityChanged)
    Q_PROPERTY(QVariantMap transfers READ GetTransfers NOTIFY transfersChanged)

    Q_PROPERTY(QVariantList freeBooks READ GetFreeBooks NOTIFY documentsChanged)
    Q_PROPERTY(QVariantMap freeTransfers READ GetFreeTransfers NOTIFY transfersChanged)

    Q_PROPERTY(int textSize READ GetTextSize WRITE SetTextSize NOTIFY readingSettingsChanged)
    Q_PROPERTY(int lineSpacing READ GetLineSpacing WRITE SetLineSpacing NOTIFY readingSettingsChanged)
    Q_PROPERTY(int textMargin READ GetTextMargin WRITE SetTextMargin NOTIFY readingSettingsChanged)
    Q_PROPERTY(QString pageTone READ GetPageTone WRITE SetPageTone NOTIFY readingSettingsChanged)
    Q_PROPERTY(bool cropMargins READ IsCropMargins WRITE SetCropMargins NOTIFY readingSettingsChanged)
    Q_PROPERTY(bool volumeKeysTurnPages READ AreVolumeKeysTurningPages WRITE SetVolumeKeysTurnPages
               NOTIFY readingSettingsChanged)

public:
    explicit ReaderController(QObject* Parent = nullptr);
    ~ReaderController() override;

    /** Re-reads the library, refreshing the exists flags. */
    Q_INVOKABLE void refresh();

    /** Opens the system file picker and, on a choice, adds the file to the library and opens it.
        Returns true when a document was opened. */
    Q_INVOKABLE bool pickAndOpen();

    /** Opens a document already in the library. True when it was found and could be read. */
    Q_INVOKABLE bool openDocument(const QString& Id);

    /** Adds a file by path and opens it, the same as picking it from the dialog. This is what a
        file dropped on the library goes through. True when a document was opened. */
    Q_INVOKABLE bool openPath(const QString& FilePath);

    Q_INVOKABLE void openExternal(const QString& FilePath, const QString& DisplayName);

    /** Leaves the reading view back to the library. The document itself is untouched. */
    Q_INVOKABLE void closeDocument();

    /** Removes one document from the library. The file itself is untouched. */
    Q_INVOKABLE void removeDocument(const QString& Id);

    Q_INVOKABLE void locateDocument(const QString& Id);

    Q_INVOKABLE void showOnDevice(const QString& Id);

    Q_INVOKABLE void savePosition(int Position);
    Q_INVOKABLE void saveZoom(qreal Zoom);
    Q_INVOKABLE void saveScroll(QPointF Scroll);
    Q_INVOKABLE void savePositionCount(int Count);

    Q_INVOKABLE void setFullScreen(bool bEnabled);

    Q_INVOKABLE void uploadDocument(const QString& Id);
    Q_INVOKABLE void downloadBook(const QString& CloudId);
    Q_INVOKABLE void deleteCloudBook(const QString& CloudId);
    Q_INVOKABLE void cancelTransfer(const QString& Key);

    Q_INVOKABLE void refreshFreeBooks();
    Q_INVOKABLE void downloadFreeBook(const QString& Id);
    Q_INVOKABLE void cancelFreeBook(const QString& Id);

    QVariantList GetDocuments() const;
    QVariantList GetFreeBooks() const;
    QVariantMap GetFreeTransfers() const;
    QVariantList GetChapters() const;
    QString GetCurrentId() const;
    QString GetCurrentTitle() const;
    QString GetCurrentPath() const;
    bool IsPdf() const;
    FTParagraphModel* GetParagraphModel();
    ReaderVocabulary* GetVocabulary();
    bool IsLoading() const;
    bool IsReading() const;
    bool IsUnreadable() const;
    int GetCurrentPosition() const;
    qreal GetCurrentZoom() const;
    QPointF GetCurrentScroll() const;
    bool IsFullScreen() const;
    bool IsCloudAvailable() const;
    QVariantMap GetTransfers() const;

    int GetTextSize() const;
    void SetTextSize(int Value);
    int GetLineSpacing() const;
    void SetLineSpacing(int Value);
    int GetTextMargin() const;
    void SetTextMargin(int Value);
    QString GetPageTone() const;
    void SetPageTone(const QString& Value);
    bool IsCropMargins() const;
    void SetCropMargins(bool bValue);
    bool AreVolumeKeysTurningPages() const;
    void SetVolumeKeysTurnPages(bool bValue);

#if defined(Q_OS_ANDROID)
    void HandleAndroidPicked(const QString& Uri, const QString& DisplayName);
    void HandleAndroidPickFailed(const QString& Message);
#endif

signals:
    void documentsChanged();
    void currentDocumentChanged();
    void fullScreenChanged();
    void readingSettingsChanged();
    void volumePageTurn(bool bForward);
    void cloudAvailabilityChanged();
    void transfersChanged();
    void cloudFailed(const QString& Message);
    void freeBookFailed(const QString& Message);

private:
    void OpenFile(const QString& FilePath, const QString& DisplayName = QString());

    void ApplyKeepScreenOn(bool bEnabled);

    void SaveCloudPlace();

    void UpdateVolumeKeys();

    void StartReflowLoad(quint64 Generation, const QString& FilePath, EFTDocumentFormat Format,
                         bool bRecordTitle);

    void OnReflowExtracted(quint64 Generation, const QString& FilePath, EFTDocumentFormat Format,
                           bool bRecordTitle, std::optional<FTReaderDocument> Extracted);

#if defined(Q_OS_ANDROID)
    void AndroidPickAndOpen();
    void AndroidShowOnDevice(const QString& Uri);
#endif

    QVariantList Documents;
    QVariantList FreeBooks;
    FTParagraphModel ParagraphModel;
    ReaderVocabulary Vocabulary;
    QVariantList Chapters;
    QString CurrentId;
    QString CurrentCloudId;
    QString CurrentTitle;
    QString CurrentPath;
    quint64 PendingGeneration = 0;
    int CurrentPosition = 0;
    int CurrentPositionCount = 0;
    qreal CurrentZoom = 0.0;
    QPointF CurrentScroll;
    bool bIsPdf = false;
    bool bLoading = false;
    bool bReading = false;
    bool bUnreadable = false;
    bool bKeepScreenOn = false;
    bool bFullScreen = false;
};
