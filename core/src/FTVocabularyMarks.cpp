#include "core/FTVocabularyMarks.h"

#include <algorithm>

static constexpr char16_t RightSingleQuote = 0x2019;
static constexpr char16_t ModifierApostrophe = 0x02BC;
static constexpr char16_t NonBreakingHyphen = 0x2011;

static bool IsWordCharacter(QChar Character)
{
    return Character.isLetterOrNumber() || Character.isMark();
}

static bool IsApostrophe(QChar Character)
{
    return Character == u'\'' || Character == RightSingleQuote || Character == ModifierApostrophe;
}

static bool IsHyphen(QChar Character)
{
    return Character == u'-' || Character == NonBreakingHyphen;
}

static bool IsSpaceOnly(const QString& Text, qsizetype Start, qsizetype End)
{
    for (qsizetype Index = Start; Index < End; ++Index)
    {
        if (!Text.at(Index).isSpace())
        {
            return false;
        }
    }

    return true;
}

void FTVocabularyMarks::AddSpelling(const QString& Spelling, bool bKnown)
{
    const std::vector<FToken> Tokens{Tokenize(Spelling)};

    if (Tokens.empty())
    {
        return;
    }

    if (Tokens.size() == 1)
    {
        const auto Existing = Words.find(Tokens.front().Key);

        if (Existing == Words.end())
        {
            Words.insert(Tokens.front().Key, bKnown);

            return;
        }

        Existing.value() = Existing.value() && bKnown;

        return;
    }

    QStringList Keys;

    for (const FToken& Token : Tokens)
    {
        Keys.append(Token.Key);
    }

    std::vector<FPhrase>& Candidates = Phrases[Keys.front()];

    const auto Existing = std::find_if(Candidates.begin(), Candidates.end(), [&Keys](const FPhrase& Candidate)
    {
        return Candidate.Keys == Keys;
    });

    if (Existing != Candidates.end())
    {
        Existing->bKnown = Existing->bKnown && bKnown;

        return;
    }

    Candidates.push_back(FPhrase{Keys, bKnown});

    std::stable_sort(Candidates.begin(), Candidates.end(), [](const FPhrase& Left, const FPhrase& Right)
    {
        return Left.Keys.size() > Right.Keys.size();
    });
}

void FTVocabularyMarks::Clear()
{
    Words.clear();
    Phrases.clear();
}

bool FTVocabularyMarks::IsEmpty() const
{
    return Words.isEmpty() && Phrases.isEmpty();
}

std::vector<FTVocabularyMark> FTVocabularyMarks::Find(const QString& Text) const
{
    std::vector<FTVocabularyMark> Marks;

    if (IsEmpty())
    {
        return Marks;
    }

    const std::vector<FToken> Tokens{Tokenize(Text)};

    for (size_t Index = 0; Index < Tokens.size();)
    {
        const size_t Consumed{MatchPhrase(Text, Tokens, Index, Marks)};

        if (Consumed > 0)
        {
            Index += Consumed;

            continue;
        }

        MatchToken(Text, Tokens[Index], Marks);

        ++Index;
    }

    return Marks;
}

std::vector<FTVocabularyMarks::FToken> FTVocabularyMarks::Tokenize(const QString& Text)
{
    std::vector<FToken> Tokens;
    const qsizetype Length{Text.size()};
    qsizetype Index{0};

    while (Index < Length)
    {
        if (!IsWordCharacter(Text.at(Index)))
        {
            ++Index;

            continue;
        }

        const qsizetype Start{Index};

        while (Index < Length)
        {
            const QChar Character{Text.at(Index)};

            if (IsWordCharacter(Character))
            {
                ++Index;

                continue;
            }

            const bool bJoins = (IsApostrophe(Character) || IsHyphen(Character))
                && Index + 1 < Length && IsWordCharacter(Text.at(Index + 1));

            if (!bJoins)
            {
                break;
            }

            ++Index;
        }

        Tokens.push_back(FToken{Start, Index, MakeKey(QStringView{Text}.mid(Start, Index - Start))});
    }

    return Tokens;
}

QString FTVocabularyMarks::MakeKey(QStringView Text)
{
    QString Key{Text.toString().normalized(QString::NormalizationForm_C).toCaseFolded()};
    Key.replace(QChar(RightSingleQuote), QChar(u'\''));
    Key.replace(QChar(ModifierApostrophe), QChar(u'\''));
    Key.replace(QChar(NonBreakingHyphen), QChar(u'-'));

    return Key;
}

size_t FTVocabularyMarks::MatchPhrase(const QString& Text, const std::vector<FToken>& Tokens, size_t First,
                                      std::vector<FTVocabularyMark>& Marks) const
{
    const auto Candidates = Phrases.constFind(Tokens[First].Key);

    if (Candidates == Phrases.constEnd())
    {
        return 0;
    }

    for (const FPhrase& Phrase : Candidates.value())
    {
        const size_t Count{static_cast<size_t>(Phrase.Keys.size())};

        if (First + Count > Tokens.size())
        {
            continue;
        }

        bool bMatches = true;

        for (size_t Offset = 1; Offset < Count && bMatches; ++Offset)
        {
            const FToken& Previous = Tokens[First + Offset - 1];
            const FToken& Current = Tokens[First + Offset];

            bMatches = Current.Key == Phrase.Keys[static_cast<qsizetype>(Offset)]
                && IsSpaceOnly(Text, Previous.End, Current.Start);
        }

        if (bMatches)
        {
            const qsizetype Start{Tokens[First].Start};

            Marks.push_back(FTVocabularyMark{Start, Tokens[First + Count - 1].End - Start, Phrase.bKnown});

            return Count;
        }
    }

    return 0;
}

void FTVocabularyMarks::MatchToken(const QString& Text, const FToken& Token, std::vector<FTVocabularyMark>& Marks) const
{
    const auto Whole = Words.constFind(Token.Key);

    if (Whole != Words.constEnd())
    {
        Marks.push_back(FTVocabularyMark{Token.Start, Token.End - Token.Start, Whole.value()});

        return;
    }

    qsizetype PartStart{Token.Start};

    for (qsizetype Index = Token.Start; Index <= Token.End; ++Index)
    {
        if (Index < Token.End && !IsHyphen(Text.at(Index)))
        {
            continue;
        }

        const bool bWholeToken = PartStart == Token.Start && Index == Token.End;

        if (bWholeToken || !MatchPart(Text, PartStart, Index, Marks))
        {
            for (qsizetype Elided = Index - 1; Elided > PartStart; --Elided)
            {
                if (IsApostrophe(Text.at(Elided)))
                {
                    MatchPart(Text, Elided + 1, Index, Marks);

                    break;
                }
            }
        }

        PartStart = Index + 1;
    }
}

bool FTVocabularyMarks::MatchPart(const QString& Text, qsizetype Start, qsizetype End,
                                  std::vector<FTVocabularyMark>& Marks) const
{
    if (End <= Start)
    {
        return false;
    }

    const auto Found = Words.constFind(MakeKey(QStringView{Text}.mid(Start, End - Start)));

    if (Found == Words.constEnd())
    {
        return false;
    }

    Marks.push_back(FTVocabularyMark{Start, End - Start, Found.value()});

    return true;
}
