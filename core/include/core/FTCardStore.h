#pragma once

#include "core/FTCard.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

using FTCardHandle = std::shared_ptr<const FTCard>;
using FTWeakCardHandle = std::weak_ptr<FTCard>;
using FTCardID = std::string;

FTCardHandle MakeCardHandle(FTCard Card);

class FTCardStore
{
public:
    FTCardHandle UpdateCard(FTCard Loaded);

    FTCardHandle FindCard(const FTCardID& CardId) const;

    void MutateCard(const FTCardHandle& Handle, const std::function<void(FTCard&)>& Change) const;

    void RemoveCard(const FTCardID& CardId);

    int GetLoadedCardCount() const;
    
    void Clear();

private:
    std::unordered_map<FTCardID, FTWeakCardHandle> Cards;
};
