#include "ReaderVocabularyHighlighter.h"

#include "ReaderVocabulary.h"

#include <QTextCharFormat>

ReaderVocabularyHighlighter::ReaderVocabularyHighlighter(QObject* Parent)
    : QSyntaxHighlighter(Parent)
{
}

QQuickTextDocument* ReaderVocabularyHighlighter::GetTextDocument() const
{
    return TextDocument;
}

void ReaderVocabularyHighlighter::SetTextDocument(QQuickTextDocument* Value)
{
    if (TextDocument == Value)
    {
        return;
    }

    TextDocument = Value;

    setDocument(Value != nullptr ? Value->textDocument() : nullptr);

    emit textDocumentChanged();
}

ReaderVocabulary* ReaderVocabularyHighlighter::GetVocabulary() const
{
    return Vocabulary;
}

void ReaderVocabularyHighlighter::SetVocabulary(ReaderVocabulary* Value)
{
    if (Vocabulary == Value)
    {
        return;
    }

    disconnect(VocabularyConnection);

    Vocabulary = Value;

    if (Value != nullptr)
    {
        VocabularyConnection = connect(Value, &ReaderVocabulary::changed, this, &QSyntaxHighlighter::rehighlight);
    }

    rehighlight();

    emit vocabularyChanged();
}

QColor ReaderVocabularyHighlighter::GetLearningColour() const
{
    return LearningColour;
}

void ReaderVocabularyHighlighter::SetLearningColour(const QColor& Value)
{
    if (LearningColour == Value)
    {
        return;
    }

    LearningColour = Value;

    rehighlight();

    emit learningColourChanged();
}

void ReaderVocabularyHighlighter::highlightBlock(const QString& Text)
{
    if (Vocabulary == nullptr || !Vocabulary->IsEnabled())
    {
        return;
    }

    QTextCharFormat Learning;
    Learning.setBackground(LearningColour);

    QTextCharFormat Known;
    Known.setUnderlineStyle(QTextCharFormat::SingleUnderline);

    for (const FTVocabularyMark& Mark : Vocabulary->Find(Text))
    {
        setFormat(static_cast<int>(Mark.Start), static_cast<int>(Mark.Length), Mark.bKnown ? Known : Learning);
    }
}
