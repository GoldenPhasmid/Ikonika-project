#pragma once

#include <string>

struct FTCard;

class FTArticleRules
{
public:
    virtual ~FTArticleRules() = default;

    virtual std::string GetDisplayWord(const FTCard& Subject) const = 0;
};

const FTArticleRules* FindArticleRules(const std::string& TargetLanguage);

bool IsFirstSyllableStressed(const std::string& Phonetic);
