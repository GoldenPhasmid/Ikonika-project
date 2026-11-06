#pragma once

#include "core/FTCardRepository.h"
#include "core/FTCardStore.h"
#include "core/FTDictionaryRepository.h"

#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct FTDeckWordProgress
{
    FTCardHandle Card;

    bool bKnown = false;
};

class FTDeckProgressMatcher
{
public:
    FTDeckProgressMatcher(const std::vector<FTCardHandle>& Cards, const std::vector<FTKnownWord>& Known);

    std::vector<FTDeckWordProgress> Match(const std::vector<FTDeckWord>& Words) const;

private:
    std::unordered_map<std::string, std::vector<FTCardHandle>> CardsByHeadword;

    std::set<std::pair<std::string, std::string>> KnownReadings;
};

std::vector<FTDeckWordProgress> MatchDeckWords(const std::vector<FTDeckWord>& Words,
                                               const std::vector<FTCardHandle>& Cards,
                                               const std::vector<FTKnownWord>& Known);
