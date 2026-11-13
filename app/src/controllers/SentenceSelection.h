#pragma once

#include "core/FTCardRepository.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTSentenceTree.h"

#include <cstdint>
#include <string>
#include <vector>

struct FSentenceSources
{
    const FTDictionaryRepository* Dictionary = nullptr;
    const FTCardRepository* Cards = nullptr;
    std::string TargetLanguage;
    std::vector<std::string> EssentialVerbs;
};

FSentenceSources MakeSentenceSources();

FTWordIdSet BuildKnownSentenceWords(const FTSentenceTree& Tree, const FSentenceSources& Sources);

FTWordIdSet BuildKnownSentenceWords(const FTSentenceTree& Tree);

void AddPresentTenseKeys(const FTSentenceTree& Tree, const std::vector<uint32_t>& VerbWordIds, FTWordIdSet& Known,
                         const std::string& TargetLanguage);

void AddPresentTenseKeys(const FTSentenceTree& Tree, const std::vector<uint32_t>& VerbWordIds, FTWordIdSet& Known);

const char* FindUnusableSentenceReason(const FTTreeSentence& Sentence);
