#pragma once

#include "core/FTSentenceTree.h"

#include <cstdint>
#include <vector>

FTWordIdSet BuildKnownSentenceWords(const FTSentenceTree& Tree);

void AddPresentTenseKeys(const FTSentenceTree& Tree, const std::vector<uint32_t>& VerbWordIds, FTWordIdSet& Known);

const char* FindUnusableSentenceReason(const FTTreeSentence& Sentence);
