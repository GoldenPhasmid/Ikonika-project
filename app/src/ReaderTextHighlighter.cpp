#include "ReaderTextHighlighter.h"

#include "ReaderVocabulary.h"

#include <QTextBlock>
#include <QTextCharFormat>

#include <algorithm>

ReaderTextHighlighter::ReaderTextHighlighter(QObject* Parent)
    : QSyntaxHighlighter(Parent)
{
}

QQuickTextDocument* ReaderTextHighlighter::GetTextDocument() const
{
    return TextDocument;
}

void ReaderTextHighlighter::SetTextDocument(QQuickTextDocument* Value)
{
    if (TextDocument == Value)
    {
        return;
    }

    TextDocument = Value;

    setDocument(Value != nullptr ? Value->textDocument() : nullptr);

    emit textDocumentChanged();
}

ReaderVocabulary* ReaderTextHighlighter::GetVocabulary() const
{
    return Vocabulary;
}

void ReaderTextHighlighter::SetVocabulary(ReaderVocabulary* Value)
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

QColor ReaderTextHighlighter::GetLearningColour() const
{
    return LearningColour;
}

void ReaderTextHighlighter::SetLearningColour(const QColor& Value)
{
    if (LearningColour == Value)
    {
        return;
    }

    LearningColour = Value;

    rehighlight();

    emit learningColourChanged();
}

int ReaderTextHighlighter::GetSentenceStart() const
{
    return SentenceStart;
}

void ReaderTextHighlighter::SetSentenceStart(int Value)
{
    if (SentenceStart == Value)
    {
        return;
    }

    SentenceStart = Value;

    rehighlight();

    emit sentenceChanged();
}

int ReaderTextHighlighter::GetSentenceEnd() const
{
    return SentenceEnd;
}

void ReaderTextHighlighter::SetSentenceEnd(int Value)
{
    if (SentenceEnd == Value)
    {
        return;
    }

    SentenceEnd = Value;

    rehighlight();

    emit sentenceChanged();
}

QColor ReaderTextHighlighter::GetSentenceColour() const
{
    return SentenceColour;
}

void ReaderTextHighlighter::SetSentenceColour(const QColor& Value)
{
    if (SentenceColour == Value)
    {
        return;
    }

    SentenceColour = Value;

    rehighlight();

    emit sentenceChanged();
}

void ReaderTextHighlighter::highlightBlock(const QString& Text)
{
    const int BlockStart{currentBlock().position()};
    const int SentenceFrom{std::clamp(SentenceStart - BlockStart, 0, static_cast<int>(Text.size()))};
    const int SentenceTo{std::clamp(SentenceEnd - BlockStart, 0, static_cast<int>(Text.size()))};
    const bool bHasSentence{SentenceStart >= 0 && SentenceTo > SentenceFrom};

    if (bHasSentence)
    {
        QTextCharFormat Sentence;
        Sentence.setBackground(SentenceColour);

        setFormat(SentenceFrom, SentenceTo - SentenceFrom, Sentence);
    }

    if (Vocabulary == nullptr || !Vocabulary->IsEnabled())
    {
        return;
    }

    for (const FTVocabularyMark& Mark : Vocabulary->Find(Text))
    {
        const int MarkStart{static_cast<int>(Mark.Start)};
        const bool bInSentence{bHasSentence && MarkStart >= SentenceFrom && MarkStart < SentenceTo};

        QTextCharFormat Format;

        if (Mark.bKnown)
        {
            Format.setUnderlineStyle(QTextCharFormat::SingleUnderline);

            if (bInSentence)
            {
                Format.setBackground(SentenceColour);
            }
        }
        else
        {
            Format.setBackground(LearningColour);
        }

        setFormat(MarkStart, static_cast<int>(Mark.Length), Format);
    }
}
