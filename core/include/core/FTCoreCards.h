#pragma once

#include <string>
#include <vector>

struct FTCoreCard
{
    std::string Word;

    std::string PartOfSpeech;

    std::string Translation;

    std::string MeaningGist;
};

std::vector<FTCoreCard> ParseCoreCards(const std::string& Text);
