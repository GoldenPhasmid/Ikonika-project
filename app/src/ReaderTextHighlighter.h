#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QtQml/qqmlregistration.h>

class ReaderVocabulary;

Q_MOC_INCLUDE("ReaderVocabulary.h")

class ReaderTextHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument* textDocument READ GetTextDocument WRITE SetTextDocument NOTIFY textDocumentChanged)
    Q_PROPERTY(ReaderVocabulary* vocabulary READ GetVocabulary WRITE SetVocabulary NOTIFY vocabularyChanged)
    Q_PROPERTY(QColor learningColour READ GetLearningColour WRITE SetLearningColour NOTIFY learningColourChanged)
    Q_PROPERTY(bool darkPaper READ IsDarkPaper WRITE SetDarkPaper NOTIFY darkPaperChanged)
    Q_PROPERTY(int sentenceStart READ GetSentenceStart WRITE SetSentenceStart NOTIFY sentenceChanged)
    Q_PROPERTY(int sentenceEnd READ GetSentenceEnd WRITE SetSentenceEnd NOTIFY sentenceChanged)
    Q_PROPERTY(QColor sentenceColour READ GetSentenceColour WRITE SetSentenceColour NOTIFY sentenceChanged)

public:
    explicit ReaderTextHighlighter(QObject* Parent = nullptr);

    QQuickTextDocument* GetTextDocument() const;
    void SetTextDocument(QQuickTextDocument* Value);

    ReaderVocabulary* GetVocabulary() const;
    void SetVocabulary(ReaderVocabulary* Value);

    QColor GetLearningColour() const;
    void SetLearningColour(const QColor& Value);

    bool IsDarkPaper() const;
    void SetDarkPaper(bool bValue);

    int GetSentenceStart() const;
    void SetSentenceStart(int Value);

    int GetSentenceEnd() const;
    void SetSentenceEnd(int Value);

    QColor GetSentenceColour() const;
    void SetSentenceColour(const QColor& Value);

signals:
    void textDocumentChanged();
    void vocabularyChanged();
    void learningColourChanged();
    void darkPaperChanged();
    void sentenceChanged();

protected:
    void highlightBlock(const QString& Text) override;

private:
    QColor ShadeForStage(int Stage) const;

private:
    QPointer<QQuickTextDocument> TextDocument;
    QPointer<ReaderVocabulary> Vocabulary;
    QMetaObject::Connection VocabularyConnection;
    QColor LearningColour;
    bool bDarkPaper = false;
    QColor SentenceColour;
    int SentenceStart = -1;
    int SentenceEnd = -1;
};
