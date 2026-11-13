#pragma once

#include "core/FTCardRepository.h"
#include "core/FTCardStore.h"
#include "core/FTDictionaryRepository.h"

#include <map>
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

    std::map<std::pair<std::string, std::string>, std::set<std::string>> KnownMeanings;
};

std::vector<FTDeckWordProgress> MatchDeckWords(const std::vector<FTDeckWord>& Words,
                                               const std::vector<FTCardHandle>& Cards,
                                               const std::vector<FTKnownWord>& Known);
