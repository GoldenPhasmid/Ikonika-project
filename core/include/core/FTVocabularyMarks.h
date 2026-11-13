#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include <vector>

struct FTVocabularyProgress
{
    bool bKnown = false;
    int Stage = 0;
};

FTVocabularyProgress MergeVocabularyProgress(const FTVocabularyProgress& Left, const FTVocabularyProgress& Right);

struct FTVocabularyMark
{
    qsizetype Start = 0;
    qsizetype Length = 0;
    bool bKnown = false;
    int Stage = 0;
};

class FTVocabularyMarks
{
public:
    void AddSpelling(const QString& Spelling, const FTVocabularyProgress& Progress);

    void Clear();

    bool IsEmpty() const;

    std::vector<FTVocabularyMark> Find(const QString& Text, bool bFindUnknown = false) const;

private:
    struct FToken
    {
        qsizetype Start = 0;
        qsizetype End = 0;
        QString Key;
    };

    struct FPhrase
    {
        QStringList Keys;
        FTVocabularyProgress Progress;
    };

    static FTVocabularyMark MakeMark(qsizetype Start, qsizetype Length, const FTVocabularyProgress& Progress);

    static std::vector<FToken> Tokenize(const QString& Text);

    static QString MakeKey(QStringView Text);

    size_t MatchPhrase(const QString& Text, const std::vector<FToken>& Tokens, size_t First,
                       std::vector<FTVocabularyMark>& Marks) const;

    void MatchToken(const QString& Text, const FToken& Token, std::vector<FTVocabularyMark>& Marks,
                    bool bFindUnknown) const;

    bool MatchPart(const QString& Text, qsizetype Start, qsizetype End, std::vector<FTVocabularyMark>& Marks) const;

    QHash<QString, FTVocabularyProgress> Words;
    QHash<QString, std::vector<FPhrase>> Phrases;
};
