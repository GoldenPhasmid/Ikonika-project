#include "core/FTCoreCards.h"

#include "core/FTTsv.h"

#include <utility>

namespace
{
    constexpr int RequiredColumnCount = 3;
}

std::vector<FTCoreCard> ParseCoreCards(const std::string& Text)
{
    std::vector<FTCoreCard> Cards;

    for (const std::vector<std::string>& Columns : ParseTsvRows(Text))
    {
        if (static_cast<int>(Columns.size()) < RequiredColumnCount || Columns[0].empty() || Columns[1].empty()
            || Columns[2].empty())
        {
            continue;
        }

        FTCoreCard Card;
        Card.Word = Columns[0];
        Card.PartOfSpeech = Columns[1];
        Card.Translation = Columns[2];
        Card.MeaningGist = Columns.size() > 3 ? Columns[3] : std::string();

        Cards.push_back(std::move(Card));
    }

    return Cards;
}
