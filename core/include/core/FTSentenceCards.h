#pragma once

#include "core/FTAnswerComparison.h"
#include "core/FTCard.h"
#include "core/FTDictionaryEntry.h"
#include "core/FTSentenceTree.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

std::string MakeSentenceCardId(uint32_t SentenceId);

bool IsSentenceCardId(std::string_view CardId);

FTCard MakeSentenceCard(const FTTreeSentence& Sentence);

std::vector<std::string> SplitSentenceWords(std::string_view Text);

struct FTGapParts
{
    std::string Before;

    std::string Word;

    std::string After;
};

bool HasGap(const FTTreeSentence& Sentence);

FTGapParts SplitAtGap(const FTTreeSentence& Sentence);

std::string GetGapWord(const FTTreeSentence& Sentence);

std::string MakeGapPrompt(const FTTreeSentence& Sentence);

FTTreeSentence MakeMeaningSentence(const FTDictionarySense& Meaning, const std::vector<std::string>& Spellings,
                                   bool bNoun = false);

bool PlaceGap(FTTreeSentence& Sentence, const std::vector<std::string>& Spellings, bool bNoun = false);

EFTAnswerGrade CompareSentence(std::string_view Given, std::string_view Expected,
                               const FTAnswerComparisonRules& Rules = {});

std::vector<FTTreeSentence> PickBalancedSentences(std::vector<FTTreeSentence> Candidates,
                                                  size_t Count, uint64_t Seed);
