#include "core/FTNarration.h"

#include "core/FTSentenceSplitter.h"

#include <algorithm>

static bool IsSpeakable(const QString& Text, qsizetype Start, qsizetype End)
{
    for (qsizetype Index{Start}; Index < End; ++Index)
    {
        if (Text.at(Index).isLetterOrNumber())
        {
            return true;
        }
    }

    return false;
}

static QString MakeSpokenText(const QString& Text)
{
    QString Spoken{Text};
    Spoken.replace(u'—', QStringLiteral(" — "));

    return Spoken.simplified();
}

static std::vector<FTNarrationSentence> FindSpeakableSentences(const std::vector<std::string>& Paragraphs, int Paragraph,
                                                               const std::string& LanguageCode)
{
    std::vector<FTNarrationSentence> Sentences;
    const QString Text{QString::fromStdString(Paragraphs[static_cast<size_t>(Paragraph)])};

    for (const FTSentenceRange& Range : SplitSentences(Text, LanguageCode))
    {
        qsizetype Start{Range.Start};
        qsizetype End{Range.End};

        while (Start < End && Text.at(Start).isSpace())
        {
            ++Start;
        }

        while (End > Start && Text.at(End - 1).isSpace())
        {
            --End;
        }

        if (!IsSpeakable(Text, Start, End))
        {
            continue;
        }

        Sentences.push_back({Paragraph, Start, End, MakeSpokenText(Text.mid(Start, End - Start))});
    }

    return Sentences;
}

static std::optional<FTNarrationSentence> FindFirstSentenceFrom(const std::vector<std::string>& Paragraphs, int Paragraph,
                                                                const std::string& LanguageCode)
{
    for (int Index{Paragraph}; Index < static_cast<int>(Paragraphs.size()); ++Index)
    {
        std::vector<FTNarrationSentence> Sentences{FindSpeakableSentences(Paragraphs, Index, LanguageCode)};

        if (!Sentences.empty())
        {
            return Sentences.front();
        }
    }

    return std::nullopt;
}

std::optional<FTNarrationSentence> FindNarrationSentenceAt(const std::vector<std::string>& Paragraphs, int Paragraph,
                                                           qsizetype Character, const std::string& LanguageCode)
{
    if (Paragraph < 0 || Paragraph >= static_cast<int>(Paragraphs.size()))
    {
        return std::nullopt;
    }

    for (const FTNarrationSentence& Sentence : FindSpeakableSentences(Paragraphs, Paragraph, LanguageCode))
    {
        if (Character < Sentence.End)
        {
            return Sentence;
        }
    }

    return FindFirstSentenceFrom(Paragraphs, Paragraph + 1, LanguageCode);
}

std::optional<FTNarrationSentence> FindNarrationSentenceAfter(const std::vector<std::string>& Paragraphs,
                                                              const FTNarrationSentence& Previous,
                                                              const std::string& LanguageCode)
{
    if (Previous.Paragraph < 0 || Previous.Paragraph >= static_cast<int>(Paragraphs.size()))
    {
        return std::nullopt;
    }

    for (const FTNarrationSentence& Sentence : FindSpeakableSentences(Paragraphs, Previous.Paragraph, LanguageCode))
    {
        if (Sentence.Start >= Previous.End)
        {
            return Sentence;
        }
    }

    return FindFirstSentenceFrom(Paragraphs, Previous.Paragraph + 1, LanguageCode);
}

std::optional<FTNarrationSentence> FindNarrationSentenceBefore(const std::vector<std::string>& Paragraphs,
                                                               const FTNarrationSentence& Next,
                                                               const std::string& LanguageCode)
{
    if (Next.Paragraph < 0 || Next.Paragraph >= static_cast<int>(Paragraphs.size()))
    {
        return std::nullopt;
    }

    const std::vector<FTNarrationSentence> Sentences{FindSpeakableSentences(Paragraphs, Next.Paragraph, LanguageCode)};

    for (auto Sentence{Sentences.rbegin()}; Sentence != Sentences.rend(); ++Sentence)
    {
        if (Sentence->End <= Next.Start)
        {
            return *Sentence;
        }
    }

    for (int Index{Next.Paragraph - 1}; Index >= 0; --Index)
    {
        const std::vector<FTNarrationSentence> Earlier{FindSpeakableSentences(Paragraphs, Index, LanguageCode)};

        if (!Earlier.empty())
        {
            return Earlier.back();
        }
    }

    return std::nullopt;
}

double EstimateNarrationSeconds(const QString& Text, float Speed)
{
    return static_cast<double>(Text.size()) / (NarrationCharactersPerSecond * std::max(Speed, 0.1f));
}
