#pragma once

#include "core/FTCard.h"
#include "core/sessions/FCardState.h"

#include <QString>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/FCardSelectorBase.h"

class FRandomSelectorBase;

struct FTSessionStartParams
{
    QString Title;
    std::string TargetLanguage;
    std::string NativeLanguage;
    std::shared_ptr<FCardState> CardState;
    std::shared_ptr<FRandomSelectorBase> Selector;
    std::optional<int> FixedLength;
};

struct FTExerciseSessionState
{
    std::vector<int32> RemainingEntries;
    int32 CurrentIndex = -1;
    int32 Count = 0;
    bool bFinished = false;
};

class FTExerciseSession
{
public:
    FTExerciseSession() = default;
    FTExerciseSession(const FTExerciseSession&) = default;
    FTExerciseSession(const FTSessionStartParams& InParams)
        : Params(InParams)
    {
        Update();
    }

    FTExerciseSession(const FTSessionStartParams& InParams, const FTExerciseSessionState& InState)
        : Params(InParams)
    {
        if (auto* EachOnce = dynamic_cast<FEachOnceSelector*>(Params.Selector.get()))
        {
            EachOnce->SetEntries(InState.RemainingEntries);
        }

        CurrentIndex = InState.CurrentIndex >= 0 ? std::optional<int>(InState.CurrentIndex) : std::nullopt;
        Count = InState.Count;
        bFinished = InState.bFinished;
    }

    FTExerciseSessionState SaveState() const
    {
        FTExerciseSessionState State;

        if (const auto* EachOnce = dynamic_cast<const FEachOnceSelector*>(Params.Selector.get()))
        {
            State.RemainingEntries = EachOnce->GetEntries();
        }

        State.CurrentIndex = CurrentIndex.value_or(-1);
        State.Count = Count;
        State.bFinished = bFinished;

        return State;
    }

    bool IsFinished() const
    {
        EnsureCurrentValid();

        return bFinished;
    }

    int GetDrawnCount() const { return Count; }
    int GetCurrentCardIndex() const
    {
        EnsureCurrentValid();

        return CurrentIndex.value_or(-1);
    }

    const FTCard* GetCurrentCard() const
    {
        EnsureCurrentValid();

        if (CurrentIndex.has_value())
        {
            return Params.CardState->GetCard(*CurrentIndex);
        }

        return nullptr;
    }

    std::string GetPrompt() const
    {
        EnsureCurrentValid();

        if (CurrentIndex.has_value())
        {
            return GetPromptFor(*CurrentIndex);
        }

        return {};
    }

    std::string GetAnswer() const
    {
        EnsureCurrentValid();

        if (CurrentIndex.has_value())
        {
            return GetAnswerFor(*CurrentIndex);
        }

        return {};

    }

    bool NotifyCardRemoved(const std::string& CardId)
    {
        const int RemovedIndex = Params.CardState->MarkRemoved(CardId);

        if (RemovedIndex < 0)
        {
            return false;
        }

        if (CurrentIndex.has_value() && *CurrentIndex == RemovedIndex)
        {
            SkipRemoved();
        }

        return true;
    }

    std::string GetPromptFor(int Index) const
    {
        if (const FTCard* Card = Params.CardState->GetCard(Index))
        {
            return GetDisplayWord(*Card, Params.TargetLanguage);
        }

        return {};
    }

    std::string GetAnswerFor(int Index) const
    {
        if (const FTCard* Card = Params.CardState->GetCard(Index))
        {
            return GetDisplayTranslation(*Card, Params.NativeLanguage);
        }

        return {};
    }

    void SetResult(int CardIndex, bool bValue)
    {
        // @todo: implement
    }
    
    void SetResult(bool bValue)
    {
        if (CurrentIndex.has_value())
        {
            SetResult(*CurrentIndex, bValue);
        }
    }

    void Advance()
    {
        if (IsFinished())
        {
            return;
        }

        Update();
    }

    const FCardState& GetCardState() const { return *Params.CardState; }
    const std::vector<FTCardHandle>& GetCards() const { return Params.CardState->GetCards(); }

private:

    bool DrawNextValid() const
    {
        for (;;)
        {
            CurrentIndex = Params.Selector->Next();

            if (!CurrentIndex.has_value())
            {
                bFinished = true;
                return false;
            }

            if (!Params.CardState->IsRemoved(*CurrentIndex))
            {
                return true;
            }

            if (Params.CardState->GetActiveCount() <= 0)
            {
                bFinished = true;
                CurrentIndex.reset();
                return false;
            }
        }
    }

    void SkipRemoved() const
    {
        if (CurrentIndex.has_value() && Params.CardState->IsRemoved(*CurrentIndex))
        {
            DrawNextValid();
        }
    }

    void EnsureCurrentValid() const
    {
        SkipRemoved();
    }

    void Update()
    {
        if (bFinished)
        {
            return;
        }

        if (Params.FixedLength.has_value() && *Params.FixedLength == Count)
        {
            bFinished = true;
            return;
        }

        if (DrawNextValid())
        {
            ++Count;
        }
    }

    FTSessionStartParams Params;
    mutable std::optional<int> CurrentIndex;
    int Count = 0;
    mutable bool bFinished = false;
};
