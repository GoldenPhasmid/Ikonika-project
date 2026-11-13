#pragma once

#include <QString>

#include <string>
#include <vector>

struct FTSentenceRange
{
    qsizetype Start{0};
    qsizetype End{0};
};

std::vector<FTSentenceRange> SplitSentences(const QString& Text, const std::string& LanguageCode);

QString FindHintAt(const QString& Text, qsizetype Position, const std::string& LanguageCode);
