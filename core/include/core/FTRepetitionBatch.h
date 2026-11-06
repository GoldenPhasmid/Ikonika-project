#pragma once

#include "core/FTCardStore.h"

#include <cstdint>
#include <vector>

int GetTargetRepetitionBatchSize();

int GetMaximumRepetitionBatchSize();

std::vector<int> ComputeRepetitionBatchSizes(int TotalCount);

std::vector<std::vector<FTCardHandle>> SplitIntoRepetitionBatches(std::vector<FTCardHandle> DueCards, uint32_t Seed);

std::vector<FTCardHandle> PickTrainingCards(const std::vector<FTCardHandle>& Pool, size_t Count);
