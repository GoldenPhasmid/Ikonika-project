#include "core/FTRepetitionBatch.h"

#include "core/FTSettings.h"
#include "core/FTTextNormalization.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>
#include <string_view>
#include <unordered_set>

#include "core/FCardSelectorBase.h"

namespace
{
    constexpr std::array<std::string_view, 4> BalancedPartsOfSpeech{"noun", "verb", "adj", "adv"};

    size_t GetBalancedQueue(const std::string& PartOfSpeech)
    {
        if (PartOfSpeech == "name")
        {
            return 0;
        }

        const auto Found = std::find(BalancedPartsOfSpeech.begin(), BalancedPartsOfSpeech.end(), PartOfSpeech);

        return static_cast<size_t>(Found - BalancedPartsOfSpeech.begin());
    }
}

int GetTargetRepetitionBatchSize()
{
    return FTSettings::Get().GetRepetitionRules().ReviewWordCount;
}

int GetMaximumRepetitionBatchSize()
{
    return (GetTargetRepetitionBatchSize() * 3 + 1) / 2;
}

std::vector<int> ComputeRepetitionBatchSizes(int TotalCount)
{
    if (TotalCount <= 0)
    {
        return {};
    }

    const int TargetSize = GetTargetRepetitionBatchSize();
    const int MaximumSize = GetMaximumRepetitionBatchSize();

    if (TotalCount <= MaximumSize)
    {
        return {TotalCount};
    }

    int Batches = std::max(
        1, static_cast<int>(std::lround(TotalCount / static_cast<double>(TargetSize))));

    // Rounding to the nearest batch count can still leave one batch at or above the cap (59
    // cards over 2 batches means one of them takes 30) — bumping by one more batch is always
    // enough to fix that for any reasonable due-card count.
    const int WorstCaseSize = (TotalCount + Batches - 1) / Batches;

    if (WorstCaseSize >= MaximumSize)
    {
        ++Batches;
    }

    const int BaseSize = TotalCount / Batches;
    const int Remainder = TotalCount % Batches;

    // The remainder is spread one-per-batch across the first Remainder batches rather than
    // piled onto the last one, so no batch ends up more than one card larger than any other.
    std::vector<int> Sizes(static_cast<size_t>(Batches), BaseSize);

    for (int Index = 0; Index < Remainder; ++Index)
    {
        ++Sizes[static_cast<size_t>(Index)];
    }

    return Sizes;
}

std::vector<std::vector<FTCardHandle>> SplitIntoRepetitionBatches(std::vector<FTCardHandle> DueCards, uint32_t Seed)
{
    std::mt19937 Generator{Seed};
    
    std::shuffle(DueCards.begin(), DueCards.end(), Generator);
    
    std::stable_sort(DueCards.begin(), DueCards.end(),
     [](const FTCardHandle& A, const FTCardHandle& B)
     {
         return A->LeitnerStage < B->LeitnerStage;
     });

    const std::vector<int> Sizes = ComputeRepetitionBatchSizes(static_cast<int>(DueCards.size()));
    const size_t BatchCount = Sizes.size();

    std::vector<std::vector<FTCardHandle>> Batches(BatchCount);
    
    std::vector<std::unordered_set<std::string>> SpellingsInBatch(BatchCount);

    for (const FTCardHandle& Card : DueCards)
    {
        const std::string Key = FoldCase(Card->TargetWord);

        // The earliest batch with room that does not already hold this spelling.
        int Chosen = -1;

        for (size_t Index = 0; Index < BatchCount; ++Index)
        {
            if (Batches[Index].size() >= static_cast<size_t>(Sizes[Index])
                || SpellingsInBatch[Index].count(Key) > 0)
            {
                continue;
            }

            Chosen = static_cast<int>(Index);
            break;
        }
        
        if (Chosen < 0)
        {
            for (size_t Index = 0; Index < BatchCount; ++Index)
            {
                if (Batches[Index].size() < static_cast<size_t>(Sizes[Index]))
                {
                    Chosen = static_cast<int>(Index);
                    break;
                }
            }
        }

        SpellingsInBatch[static_cast<size_t>(Chosen)].insert(Key);
        Batches[static_cast<size_t>(Chosen)].push_back(Card);
    }

    return Batches;
}

std::vector<FTCardHandle> PickTrainingCards(const std::vector<FTCardHandle>& Pool, size_t Count)
{
    std::vector<std::vector<FTCardHandle>> Queues(BalancedPartsOfSpeech.size() + 1);

    for (const FTCardHandle& Card : Pool)
    {
        Queues[GetBalancedQueue(Card->PartOfSpeech)].push_back(Card);
    }

    std::vector<size_t> Cursors(Queues.size(), 0);
    std::unordered_set<std::string> Spellings;
    std::vector<FTCardHandle> Picked;

    while (Picked.size() < Count)
    {
        bool bTookAny = false;

        for (size_t Queue = 0; Queue < Queues.size() && Picked.size() < Count; ++Queue)
        {
            while (Cursors[Queue] < Queues[Queue].size())
            {
                const FTCardHandle& Card = Queues[Queue][Cursors[Queue]++];

                if (Spellings.insert(FoldCase(Card->TargetWord)).second)
                {
                    Picked.push_back(Card);
                    bTookAny = true;

                    break;
                }
            }
        }

        if (!bTookAny)
        {
            break;
        }
    }

    return Picked;
}
