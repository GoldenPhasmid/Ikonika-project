#include "core/FTCardStore.h"

#include "core/FTLog.h"

#include <ranges>
#include <utility>

FTCardHandle MakeCardHandle(FTCard Card)
{
    return std::make_shared<const FTCard>(std::move(Card));
}

FTCardHandle FTCardStore::UpdateCard(FTCard Loaded)
{
    if (Loaded.Id.empty())
    {
        return std::make_shared<const FTCard>(std::move(Loaded));
    }

    const auto FoundCard = Cards.find(Loaded.Id);

    if (FoundCard != Cards.end())
    {
        if (const std::shared_ptr<FTCard> Live = FoundCard->second.lock())
        {
            qCDebug(LogDatabase) << "card refreshed in place:" << QString::fromStdString(Loaded.Id);

            *Live = std::move(Loaded);
            return Live;
        }

        Cards.erase(FoundCard);
    }

    const std::string CardId = Loaded.Id;
    std::shared_ptr<FTCard> Created = std::make_shared<FTCard>(std::move(Loaded));

    Cards.emplace(CardId, Created);

    return Created;
}

FTCardHandle FTCardStore::FindCard(const FTCardID& CardId) const
{
    const auto Found = Cards.find(CardId);

    if (Found == Cards.end())
    {
        return nullptr;
    }

    return Found->second.lock();
}

void FTCardStore::MutateCard(const FTCardHandle& Handle, const std::function<void(FTCard&)>& Change) const
{
    if (Handle == nullptr || !Change)
    {
        return;
    }

    qCDebug(LogDatabase) << "card mutated in place:" << QString::fromStdString(Handle->Id);

    Change(const_cast<FTCard&>(*Handle));
}

void FTCardStore::RemoveCard(const FTCardID& CardId)
{
    qCDebug(LogDatabase) << "card removed from store:" << QString::fromStdString(CardId);

    Cards.erase(CardId);
}

void FTCardStore::Clear()
{
    Cards.clear();
}

int FTCardStore::GetLoadedCardCount() const
{
    int Live = 0;

    for (const auto& CardHandle : Cards | std::views::values)
    {
        if (!CardHandle.expired())
        {
            ++Live;
        }
    }

    return Live;
}
