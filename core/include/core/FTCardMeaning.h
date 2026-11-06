#pragma once

#include "core/FTCard.h"
#include "core/FTDictionaryEntry.h"

#include <optional>
#include <string>
#include <vector>

bool MeaningFitsGender(const FTDictionarySense& Meaning, const std::optional<int>& Gender);

const FTDictionarySense* FindCardMeaning(const FTCard& Card, const std::vector<FTDictionarySense>& Meanings);

std::string MatchMeaningGist(const FTCard& Card, const std::vector<FTDictionarySense>& Meanings);

std::string PickPopularMeaningGist(const FTCard& Card, const std::vector<FTDictionarySense>& Meanings);

void SortMeaningsByUse(std::vector<FTDictionarySense>& Meanings);
