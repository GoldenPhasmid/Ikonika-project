#pragma once

#include <QString>

#include <optional>
#include <string>
#include <vector>

inline constexpr double NarrationCharactersPerSecond = 15.0;

struct FTNarrationSentence
{
    int Paragraph = -1;
    qsizetype Start = 0;
    qsizetype End = 0;
    QString Text;
};

std::optional<FTNarrationSentence> FindNarrationSentenceAt(const std::vector<std::string>& Paragraphs, int Paragraph,
                                                           qsizetype Character, const std::string& LanguageCode);

std::optional<FTNarrationSentence> FindNarrationSentenceAfter(const std::vector<std::string>& Paragraphs,
                                                              const FTNarrationSentence& Previous,
                                                              const std::string& LanguageCode);

double EstimateNarrationSeconds(const QString& Text, float Speed);
