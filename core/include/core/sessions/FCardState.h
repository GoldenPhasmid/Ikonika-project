#pragma once

#include "core/FTCard.h"
#include "core/FTCardStore.h"

#include <string>
#include <unordered_map>
#include <vector>

class FCardState
{
public:
    explicit FCardState(std::vector<FTCardHandle> InCards)
        : Cards(std::move(InCards))
        , Removed(Cards.size(), false)
        , ActiveCount(static_cast<int>(Cards.size()))
    {
    }

    int GetCount() const { return static_cast<int>(Cards.size()); }
    int GetActiveCount() const { return ActiveCount; }

    const FTCard* GetCard(int Index) const
    {
        if (Index < 0 || Index >= GetCount())
        {
            return nullptr;
        }

        return Cards[static_cast<size_t>(Index)].get();
    }

    bool IsRemoved(int Index) const
    {
        if (Index < 0 || Index >= GetCount())
        {
            return true;
        }

        return Removed[static_cast<size_t>(Index)];
    }

    int MarkRemoved(const std::string& CardId)
    {
        for (size_t Index = 0; Index < Cards.size(); ++Index)
        {
            if (Removed[Index] || !Cards[Index] || Cards[Index]->Id != CardId)
            {
                continue;
            }

            Removed[Index] = true;
            --ActiveCount;

            return static_cast<int>(Index);
        }

        return -1;
    }

    const std::vector<FTCardHandle>& GetCards() const { return Cards; }

    std::vector<FTCardHandle> GetActiveCards() const
    {
        std::vector<FTCardHandle> Result;
        Result.reserve(static_cast<size_t>(ActiveCount));

        for (size_t Index = 0; Index < Cards.size(); ++Index)
        {
            if (!Removed[Index])
            {
                Result.push_back(Cards[Index]);
            }
        }

        return Result;
    }

    std::vector<std::string> GetRemovedCardIds() const
    {
        std::vector<std::string> Result;

        for (size_t Index = 0; Index < Cards.size(); ++Index)
        {
            if (Removed[Index] && Cards[Index])
            {
                Result.push_back(Cards[Index]->Id);
            }
        }

        return Result;
    }

private:
    std::vector<FTCardHandle> Cards;
    std::vector<bool> Removed;
    int ActiveCount = 0;
};
