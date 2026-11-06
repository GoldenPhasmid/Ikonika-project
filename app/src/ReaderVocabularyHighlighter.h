#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QtQml/qqmlregistration.h>

class ReaderVocabulary;

class ReaderVocabularyHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument* textDocument READ GetTextDocument WRITE SetTextDocument NOTIFY textDocumentChanged)
    Q_PROPERTY(ReaderVocabulary* vocabulary READ GetVocabulary WRITE SetVocabulary NOTIFY vocabularyChanged)
    Q_PROPERTY(QColor learningColour READ GetLearningColour WRITE SetLearningColour NOTIFY learningColourChanged)

public:
    explicit ReaderVocabularyHighlighter(QObject* Parent = nullptr);

    QQuickTextDocument* GetTextDocument() const;
    void SetTextDocument(QQuickTextDocument* Value);

    ReaderVocabulary* GetVocabulary() const;
    void SetVocabulary(ReaderVocabulary* Value);

    QColor GetLearningColour() const;
    void SetLearningColour(const QColor& Value);

signals:
    void textDocumentChanged();
    void vocabularyChanged();
    void learningColourChanged();

protected:
    void highlightBlock(const QString& Text) override;

private:
    QPointer<QQuickTextDocument> TextDocument;
    QPointer<ReaderVocabulary> Vocabulary;
    QMetaObject::Connection VocabularyConnection;
    QColor LearningColour;
};
