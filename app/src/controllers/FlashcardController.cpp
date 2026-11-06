#include "FlashcardController.h"

#include "core/FTSettings.h"

#include <QJsonArray>

#include <memory>

FlashcardController::FlashcardController(QObject* Parent)
    : ExerciseControllerBase(Parent)
{
}

void FlashcardController::InitializeSession()
{
    ExerciseControllerBase::InitializeSession();

    History.clear();
    HistoryPosition = -1;

    if (Session->GetCurrentCardIndex() >= 0)
    {
        History.push_back(Session->GetCurrentCardIndex());
        HistoryPosition = 0;
        Session->SetResult(History.front(), true);
    }

    SpeakAutomatically(GetPrompt());
}

void FlashcardController::next()
{
    if (!Session.has_value())
    {
        return;
    }

    if (HistoryPosition + 1 < History.size())
    {
        ++HistoryPosition;
    }
    else if (!Session->IsFinished())
    {
        Session->Advance();

        if (Session->IsFinished())
        {
            emit stateChanged();

            return;
        }

        History.push_back(Session->GetCurrentCardIndex());
        ++HistoryPosition;
    }
    else
    {
        return;
    }

    Session->SetResult(History[HistoryPosition], true);

    SpeakAutomatically(GetPrompt());

    emit stateChanged();
}

void FlashcardController::previous()
{
    if (!Session.has_value() || HistoryPosition <= 0)
    {
        return;
    }

    --HistoryPosition;

    Session->SetResult(History[HistoryPosition], true);

    SpeakAutomatically(GetPrompt());

    emit stateChanged();
}

void FlashcardController::replay()
{
    Speak(GetPrompt());
}

const FTCard* FlashcardController::GetCurrentCard() const
{
    const int Index = CurrentCardIndex();

    if (Index < 0 || !Params.CardState)
    {
        return nullptr;
    }

    return Params.CardState->GetCard(Index);
}

bool FlashcardController::removeCard(const QString& CardId)
{
    if (!ExerciseControllerBase::removeCard(CardId))
    {
        return false;
    }

    if (Session.has_value() && HistoryPosition >= 0
        && HistoryPosition < static_cast<int>(History.size()))
    {
        History[static_cast<size_t>(HistoryPosition)] = Session->GetCurrentCardIndex();
    }

    return true;
}

QString FlashcardController::GetPrompt() const
{
    const int Index = CurrentCardIndex();

    if (Index < 0 || !Session.has_value())
    {
        return {};
    }

    return QString::fromStdString(Session->GetPromptFor(Index));
}

QString FlashcardController::GetAnswer() const
{
    const int Index = CurrentCardIndex();

    if (Index < 0 || !Session.has_value())
    {
        return {};
    }

    return QString::fromStdString(Session->GetAnswerFor(Index));
}

bool FlashcardController::CanGoNext() const
{
    return Session.has_value() && (HistoryPosition + 1 < History.size() || !Session->IsFinished());
}

bool FlashcardController::CanGoPrevious() const
{
    return HistoryPosition > 0;
}

int FlashcardController::GetCurrentSessionCount() const
{
    return HistoryPosition + 1;
}

QJsonObject FlashcardController::SaveExtraState() const
{
    QJsonArray HistoryJson;

    for (int Index : History)
    {
        HistoryJson.append(Index);
    }

    QJsonObject Json;
    Json["history"] = HistoryJson;
    Json["historyPosition"] = HistoryPosition;

    return Json;
}

void FlashcardController::RestoreExtraState(const QJsonObject& Json)
{
    History.clear();

    for (const QJsonValue& Index : Json["history"].toArray())
    {
        History.push_back(Index.toInt());
    }

    HistoryPosition = Json["historyPosition"].toInt(-1);
}

int FlashcardController::CurrentCardIndex() const
{
    if (HistoryPosition < 0 || HistoryPosition >= static_cast<int>(History.size()))
    {
        return -1;
    }

    const int Index = History[static_cast<size_t>(HistoryPosition)];

    if (Session.has_value() && Params.CardState && Params.CardState->IsRemoved(Index))
    {
        const int Fresh = Session->GetCurrentCardIndex();
        History[static_cast<size_t>(HistoryPosition)] = Fresh;

        return Fresh;
    }

    return Index;
}
