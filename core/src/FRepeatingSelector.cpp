#include "core/FRepeatingSelector.h"

#include <algorithm>

FRepeatingSelector::FRepeatingSelector(FactoryFunction InMakeInner, int InLimit)
    : MakeInner(std::move(InMakeInner))
    , Limit(std::max(InLimit, 1))
    , Inner(MakeInner())
{
}

std::optional<int> FRepeatingSelector::Next()
{
    if (SelectedCount >= Limit)
    {
        return std::nullopt;
    }

    if (PendingDraw.has_value())
    {
        const int Value = *PendingDraw;
        PendingDraw.reset();
        ++SelectedCount;
        LastIndex = Value;

        return Value;
    }

    std::optional<int> Result = Inner->Next();

    if (!Result.has_value())
    {
        Inner = MakeInner();
        Result = Inner->Next();

        // The seam between passes is the one place a fresh, independently shuffled pass could
        // repeat the card the previous pass just ended on. Swap it out for the pass's second
        // draw and hold the first back for next time, rather than allow a back-to-back repeat.
        if (Result.has_value() && Result == LastIndex && Inner->GetCardCount() > 1)
        {
            PendingDraw = Result;
            Result = Inner->Next();
        }
    }

    if (Result.has_value())
    {
        ++SelectedCount;
        LastIndex = Result;
    }

    return Result;
}

bool FRepeatingSelector::IsFinished() const
{
    return SelectedCount >= Limit;
}

int FRepeatingSelector::GetCardCount() const
{
    return Inner->GetCardCount();
}

int FRepeatingSelector::GetSelectedCount() const
{
    return SelectedCount;
}

void FRepeatingSelector::AdjustWeight(int Index, double Delta)
{
    Inner->AdjustWeight(Index, Delta);
}

double FRepeatingSelector::GetWeight(int Index) const
{
    return Inner->GetWeight(Index);
}
