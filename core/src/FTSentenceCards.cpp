#include "core/FTSentenceCards.h"

#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <utility>

namespace
{
    constexpr std::string_view SentenceCardIdPrefix{"sentence:"};

    constexpr std::string_view GapMark{"___"};

    constexpr std::string_view GapPause{"…"};

    constexpr std::array<std::string_view, 50> Determiners{
        "le", "la", "les", "l'", "un", "une", "des", "du", "d'", "au", "aux", "ce", "cet", "cette", "ces",
        "mon", "ma", "mes", "ton", "ta", "tes", "son", "sa", "ses", "notre", "nos", "votre", "vos", "leur", "leurs",
        "el", "los", "las", "una", "unos", "unas", "del", "al", "mi", "mis", "tu", "tus", "su", "sus",
        "the", "an", "my", "his", "her", "their"
    };

    bool IsApostrophe(QChar Character)
    {
        return Character == QLatin1Char('\'') || Character == QChar(0x2019);
    }

    bool FollowsDeterminer(const QString& Text, qsizetype Start)
    {
        qsizetype End = Start;

        while (End > 0 && Text[End - 1].isSpace())
        {
            --End;
        }

        qsizetype Begin = End;

        while (Begin > 0 && (Text[Begin - 1].isLetter() || IsApostrophe(Text[Begin - 1])))
        {
            --Begin;
        }

        QString Previous = Text.mid(Begin, End - Begin).toLower();
        Previous.replace(QChar(0x2019), QLatin1Char('\''));

        const std::string Word = Previous.toStdString();

        return std::find(Determiners.begin(), Determiners.end(), Word) != Determiners.end();
    }

    QString TrimToWord(const QString& Token)
    {
        qsizetype First = 0;
        qsizetype Last = Token.size();

        while (First < Last && !Token[First].isLetterOrNumber())
        {
            ++First;
        }

        while (Last > First && !Token[Last - 1].isLetterOrNumber())
        {
            --Last;
        }

        return Token.mid(First, Last - First);
    }

    QStringList SplitWords(const QString& Text)
    {
        QStringList Words;
        QString Token;

        const auto Flush = [&]()
        {
            const QString Word = TrimToWord(Token);

            if (!Word.isEmpty())
            {
                Words.push_back(Word);
            }

            Token.clear();
        };

        for (const QChar Character : Text)
        {
            if (Character.isSpace())
            {
                Flush();

                continue;
            }

            Token.push_back(Character);
        }

        Flush();

        return Words;
    }

    bool IsGapInside(const FTTreeSentence& Sentence, const QString& Text)
    {
        return Sentence.GapStart >= 0 && Sentence.GapLength > 0
            && Sentence.GapStart + Sentence.GapLength <= Text.size();
    }
}

std::string MakeSentenceCardId(uint32_t SentenceId)
{
    return std::string{SentenceCardIdPrefix} + std::to_string(SentenceId);
}

bool IsSentenceCardId(std::string_view CardId)
{
    return CardId.substr(0, SentenceCardIdPrefix.size()) == SentenceCardIdPrefix;
}

FTCard MakeSentenceCard(const FTTreeSentence& Sentence)
{
    FTCard Card;
    Card.Id = MakeSentenceCardId(Sentence.Id);
    Card.TargetWord = Sentence.Text;
    Card.Translation = Sentence.Translation;

    return Card;
}

std::vector<std::string> SplitSentenceWords(std::string_view Text)
{
    std::vector<std::string> Result;

    for (const QString& Word : SplitWords(QString::fromUtf8(Text.data(), static_cast<qsizetype>(Text.size()))))
    {
        Result.push_back(Word.toStdString());
    }

    return Result;
}

bool HasGap(const FTTreeSentence& Sentence)
{
    return IsGapInside(Sentence, QString::fromStdString(Sentence.Text));
}

FTGapParts SplitAtGap(const FTTreeSentence& Sentence)
{
    const QString Text = QString::fromStdString(Sentence.Text);

    FTGapParts Parts;

    if (!IsGapInside(Sentence, Text))
    {
        Parts.Before = Sentence.Text;

        return Parts;
    }

    Parts.Before = Text.left(Sentence.GapStart).toStdString();
    Parts.Word = Text.mid(Sentence.GapStart, Sentence.GapLength).toStdString();
    Parts.After = Text.mid(Sentence.GapStart + Sentence.GapLength).toStdString();

    return Parts;
}

std::string GetGapWord(const FTTreeSentence& Sentence)
{
    return SplitAtGap(Sentence).Word;
}

std::string MakeGapPrompt(const FTTreeSentence& Sentence)
{
    const FTGapParts Parts = SplitAtGap(Sentence);

    if (Parts.Word.empty())
    {
        return Sentence.Text;
    }

    return Parts.Before + std::string{GapMark} + Parts.After;
}

std::string MakeGapSpeech(const FTTreeSentence& Sentence)
{
    const FTGapParts Parts = SplitAtGap(Sentence);

    if (Parts.Word.empty())
    {
        return Sentence.Text;
    }

    return Parts.Before + std::string{GapPause} + Parts.After;
}

FTTreeSentence MakeMeaningSentence(const FTDictionarySense& Meaning, const std::vector<std::string>& Spellings,
                                   bool bNoun)
{
    FTTreeSentence Sentence;
    Sentence.Text = Meaning.Example;
    Sentence.Translation = Meaning.ExampleTranslation;

    PlaceGap(Sentence, Spellings, bNoun);

    return Sentence;
}

bool PlaceGap(FTTreeSentence& Sentence, const std::vector<std::string>& Spellings, bool bNoun)
{
    Sentence.GapStart = -1;
    Sentence.GapLength = 0;

    const QString Text = QString::fromStdString(Sentence.Text);

    const auto IsWordCharacter = [&Text](qsizetype Index)
    {
        return Index >= 0 && Index < Text.size() && Text[Index].isLetterOrNumber();
    };

    for (const std::string& Spelling : Spellings)
    {
        const QString Word = QString::fromStdString(Spelling);

        if (Word.isEmpty() || Word.size() <= Sentence.GapLength)
        {
            continue;
        }

        std::optional<qsizetype> Chosen;

        for (qsizetype Found = Text.indexOf(Word, 0, Qt::CaseInsensitive); Found >= 0;
             Found = Text.indexOf(Word, Found + 1, Qt::CaseInsensitive))
        {
            if (IsWordCharacter(Found - 1) || IsWordCharacter(Found + Word.size()))
            {
                continue;
            }

            if (!Chosen.has_value() || FollowsDeterminer(Text, Found))
            {
                Chosen = Found;
            }

            if (!bNoun || FollowsDeterminer(Text, Found))
            {
                break;
            }
        }

        if (Chosen.has_value())
        {
            Sentence.GapStart = static_cast<int>(*Chosen);
            Sentence.GapLength = static_cast<int>(Word.size());
        }
    }

    return Sentence.GapStart >= 0;
}

EFTAnswerGrade CompareSentence(std::string_view Given, std::string_view Expected,
                               const FTAnswerComparisonRules& Rules)
{
    const auto Join = [](std::string_view Text)
    {
        return SplitWords(QString::fromUtf8(Text.data(), static_cast<qsizetype>(Text.size())))
            .join(QLatin1Char(' ')).toStdString();
    };

    return CompareAnswer(Join(Given), Join(Expected), Rules);
}

std::vector<FTTreeSentence> PickBalancedSentences(std::vector<FTTreeSentence> Candidates,
                                                  size_t Count, uint64_t Seed)
{
    if (Candidates.size() <= Count || Count == 0)
    {
        Candidates.resize(std::min(Candidates.size(), Count));

        return Candidates;
    }

    std::vector<size_t> Lengths;
    Lengths.reserve(Candidates.size());

    for (const FTTreeSentence& Candidate : Candidates)
    {
        Lengths.push_back(SplitSentenceWords(Candidate.Text).size());
    }

    std::mt19937_64 Engine{Seed};

    std::vector<size_t> Order(Candidates.size());
    std::iota(Order.begin(), Order.end(), size_t{0});
    std::shuffle(Order.begin(), Order.end(), Engine);

    std::stable_sort(Order.begin(), Order.end(),
        [&](size_t Left, size_t Right)
        {
            if (Lengths[Left] != Lengths[Right])
            {
                return Lengths[Left] < Lengths[Right];
            }

            return Candidates[Left].ChangedCount < Candidates[Right].ChangedCount;
        });

    using FTWindowScore = std::pair<size_t, int>;

    std::vector<size_t> BestStarts;
    FTWindowScore BestScore{std::numeric_limits<size_t>::max(), 0};

    for (size_t Start = 0; Start + Count <= Order.size(); ++Start)
    {
        const auto [Fewest, Most] = std::minmax_element(
            Order.begin() + static_cast<std::ptrdiff_t>(Start),
            Order.begin() + static_cast<std::ptrdiff_t>(Start + Count),
            [&](size_t Left, size_t Right)
            {
                return Candidates[Left].ChangedCount < Candidates[Right].ChangedCount;
            });

        const FTWindowScore Score{
            Lengths[Order[Start + Count - 1]] - Lengths[Order[Start]],
            Candidates[*Most].ChangedCount - Candidates[*Fewest].ChangedCount};

        if (Score < BestScore)
        {
            BestStarts.clear();
            BestScore = Score;
        }

        if (Score == BestScore)
        {
            BestStarts.push_back(Start);
        }
    }

    const size_t BestStart = BestStarts[Engine() % BestStarts.size()];

    std::vector<size_t> Picked(Order.begin() + static_cast<std::ptrdiff_t>(BestStart),
                               Order.begin() + static_cast<std::ptrdiff_t>(BestStart + Count));
    std::sort(Picked.begin(), Picked.end());

    std::vector<FTTreeSentence> Result;
    Result.reserve(Count);

    for (const size_t Index : Picked)
    {
        Result.push_back(std::move(Candidates[Index]));
    }

    return Result;
}
