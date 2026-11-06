#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include <vector>

struct FTVocabularyMark
{
    qsizetype Start = 0;
    qsizetype Length = 0;
    bool bKnown = false;
};

class FTVocabularyMarks
{
public:
    void AddSpelling(const QString& Spelling, bool bKnown);

    void Clear();

    bool IsEmpty() const;

    std::vector<FTVocabularyMark> Find(const QString& Text) const;

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
        bool bKnown = false;
    };

    static std::vector<FToken> Tokenize(const QString& Text);

    static QString MakeKey(QStringView Text);

    size_t MatchPhrase(const QString& Text, const std::vector<FToken>& Tokens, size_t First,
                       std::vector<FTVocabularyMark>& Marks) const;

    void MatchToken(const QString& Text, const FToken& Token, std::vector<FTVocabularyMark>& Marks) const;

    bool MatchPart(const QString& Text, qsizetype Start, qsizetype End, std::vector<FTVocabularyMark>& Marks) const;

    QHash<QString, bool> Words;
    QHash<QString, std::vector<FPhrase>> Phrases;
};
