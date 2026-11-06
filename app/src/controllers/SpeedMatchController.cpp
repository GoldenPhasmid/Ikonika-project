#include "SpeedMatchController.h"
#include "AppCards.h"

#include "core/FCardSelectorBase.h"
#include "core/FTCard.h"
#include "core/FTLog.h"

#include <QJsonArray>

#include <algorithm>
#include <limits>
#include <memory>

SpeedMatchController::SpeedMatchController(QObject* Parent)
    : ExerciseControllerBase(Parent)
{
    SpeedMatchController::ApplySessionSettings();
}

void SpeedMatchController::ApplySessionSettings()
{
    SetVoicePrompt(GetSessionRules().bMatchVoice);
}

void SpeedMatchController::SetPairText(FMatchPairText InPairText)
{
    PairText = std::move(InPairText);
}

void SpeedMatchController::SetPairCount(int InPairCount)
{
    MatchPairCount = std::max(InPairCount, 1);
}

void SpeedMatchController::SetScreenKey(FMatchScreenKey InScreenKey)
{
    ScreenKey = std::move(InScreenKey);
}

void SpeedMatchController::SetScreenHeading(FMatchHeading InHeading)
{
    HeadingText = std::move(InHeading);
}

QString SpeedMatchController::GetHeading() const
{
    return Heading;
}

const FTCard* SpeedMatchController::FindScreenCard() const
{
    if (!Session.has_value())
    {
        return nullptr;
    }

    const int CardCount = static_cast<int>(Session->GetCards().size());

    for (const std::vector<int>* Side : {&LeftCardIndices, &RightCardIndices})
    {
        for (const int CardIndex : *Side)
        {
            if (CardIndex >= 0 && CardIndex < CardCount)
            {
                return Session->GetCards()[static_cast<size_t>(CardIndex)].get();
            }
        }
    }

    return nullptr;
}

const FTCard* SpeedMatchController::GetFormsCard() const
{
    return FindScreenCard();
}

void SpeedMatchController::UpdateHeading()
{
    Heading.clear();

    if (!HeadingText || !Session.has_value())
    {
        return;
    }

    const FTCard* ScreenCard = FindScreenCard();

    if (ScreenCard == nullptr)
    {
        qCWarning(LogSession) << "SpeedMatch heading has no unmatched card to read from";

        return;
    }

    Heading = HeadingText(*ScreenCard);
}

void SpeedMatchController::speakHeading()
{
    const QString Heading = GetHeading();

    if (Heading.isEmpty())
    {
        qCWarning(LogSession) << "SpeedMatch heading clicked with no heading on this screen";

        return;
    }

    Speak(Heading);
}

void SpeedMatchController::SetOrderedSides(bool bInLeftOrdered, bool bInRightOrdered)
{
    bLeftOrdered = bInLeftOrdered;
    bRightOrdered = bInRightOrdered;
}

void SpeedMatchController::InitializeSession()
{
    ExerciseControllerBase::InitializeSession();
    
    MatchedCount = 0;
    TotalMatchedCount = 0;
    BuildScreen();
}

void SpeedMatchController::BuildScreen()
{
    LeftCardIndices.clear();
    RightCardIndices.clear();
    LeftItems.clear();
    RightItems.clear();
    RightMarkups.clear();
    LeftMarkups.clear();
    RightTextByCard.clear();
    Heading.clear();
    SelectedLeftRow = -1;
    SelectedRightRow = -1;
    FlashedLeftRow = -1;
    FlashedRightRow = -1;
    MatchedCount = 0;

    if (!Session.has_value() || Session->IsFinished())
    {
        return;
    }

    std::vector<int> Batch;

    const int CardCount = static_cast<int>(Session->GetCards().size());
    const int TotalEntries = Params.FixedLength.value_or(CardCount);
    const int RemainingEntries = TotalEntries - Session->GetDrawnCount() + 1;
    const int PassEntries = CountEntriesLeftInPass();
    const int ScreenEntries = std::min(RemainingEntries, PassEntries);

    int TargetBatchSize = ScreenKey ? CardCount : std::min(MatchPairCount, CardCount);

    if (!ScreenKey && ScreenEntries - TargetBatchSize == 1 && TargetBatchSize < CardCount)
    {
        ++TargetBatchSize;
    }

    const auto KeyOf = [this](int CardIndex)
    {
        return ScreenKey(*Session->GetCards()[static_cast<size_t>(CardIndex)]);
    };

    while (static_cast<int>(Batch.size()) < TargetBatchSize && !Session->IsFinished())
    {
        if (ScreenKey && !Batch.empty() && KeyOf(Session->GetCurrentCardIndex()) != KeyOf(Batch.front()))
        {
            break;
        }

        const bool bLastOfPass = CountEntriesLeftInPass() == 1;

        Batch.push_back(Session->GetCurrentCardIndex());
        Session->Advance();

        if (bLastOfPass)
        {
            break;
        }
    }

    qCInfo(LogSession) << "SpeedMatch batch of" << static_cast<int>(Batch.size()) << "from"
                       << RemainingEntries << "remaining entries," << PassEntries << "left in this pass";

    LeftCardIndices = Batch;
    RightCardIndices = Batch;
    UpdateHeading();

    const auto Arrange = [this](std::vector<int>& Side, bool bOrdered)
    {
        if (bOrdered)
        {
            std::sort(Side.begin(), Side.end());

            return;
        }

        std::shuffle(Side.begin(), Side.end(), ScreenGenerator);
    };

    Arrange(LeftCardIndices, bLeftOrdered);
    Arrange(RightCardIndices, bRightOrdered);

    if (PairText)
    {
        for (const int CardIndex : LeftCardIndices)
        {
            const FMatchPair Pair = PairText(*Session->GetCards()[static_cast<size_t>(CardIndex)]);

            LeftItems.append(QString::fromStdString(Pair.Left));
            LeftMarkups.append(Pair.LeftMarkup);
        }

        for (const int CardIndex : RightCardIndices)
        {
            const FMatchPair Pair = PairText(*Session->GetCards()[static_cast<size_t>(CardIndex)]);

            RightItems.append(QString::fromStdString(Pair.Right));
            RightMarkups.append(Pair.RightMarkup);
        }

        RememberRightTexts();

        return;
    }

    for (const int CardIndex : LeftCardIndices)
    {
        LeftItems.append(QString::fromStdString(
            GetDisplayWord(*Session->GetCards()[static_cast<size_t>(CardIndex)], TargetLanguage.toStdString())));
    }

    for (const int CardIndex : RightCardIndices)
    {
        RightItems.append(QString::fromStdString(
            GetDisplayTranslation(*Session->GetCards()[static_cast<size_t>(CardIndex)],
                NativeLanguage.toStdString())));
    }

    RememberRightTexts();
}

int SpeedMatchController::CountEntriesLeftInPass() const
{
    const auto* EachOnce = dynamic_cast<const FEachOnceSelector*>(Params.Selector.get());

    if (EachOnce == nullptr)
    {
        return std::numeric_limits<int>::max();
    }

    return static_cast<int>(EachOnce->GetEntries().size()) + 1;
}

void SpeedMatchController::RememberRightTexts()
{
    for (size_t Row = 0; Row < RightCardIndices.size() && Row < static_cast<size_t>(RightItems.size()); ++Row)
    {
        RightTextByCard[RightCardIndices[Row]] = RightItems[static_cast<qsizetype>(Row)];
    }
}

void SpeedMatchController::tapLeft(int Row)
{
    if (!Session.has_value() || Row < 0 || Row >= static_cast<int>(LeftCardIndices.size())
        || LeftCardIndices[static_cast<size_t>(Row)] < 0)
    {
        return;
    }

    if (IsVoiceLeft())
    {
        SelectedLeftRow = Row;
        Speak(LeftItems[static_cast<qsizetype>(Row)]);
    }
    else
    {
        SelectedLeftRow = (SelectedLeftRow == Row) ? -1 : Row;
    }

    FlashedLeftRow = -1;
    FlashedRightRow = -1;

    TryPendingPair();

    emit stateChanged();
}

void SpeedMatchController::tapRight(int Row)
{
    if (!Session.has_value() || Row < 0 || Row >= static_cast<int>(RightCardIndices.size())
        || RightCardIndices[static_cast<size_t>(Row)] < 0)
    {
        return;
    }

    SelectedRightRow = (SelectedRightRow == Row) ? -1 : Row;
    FlashedLeftRow = -1;
    FlashedRightRow = -1;

    TryPendingPair();

    emit stateChanged();
}

void SpeedMatchController::TryPendingPair()
{
    if (SelectedLeftRow < 0 || SelectedRightRow < 0)
    {
        return;
    }

    const int RightCard = RightCardIndices[static_cast<size_t>(SelectedRightRow)];
    const auto RightLeftRow = std::find(LeftCardIndices.begin(), LeftCardIndices.end(), RightCard);

    const bool bSameLeftText = PairText && RightLeftRow != LeftCardIndices.end()
        && LeftItems[static_cast<qsizetype>(RightLeftRow - LeftCardIndices.begin())]
            == LeftItems[static_cast<qsizetype>(SelectedLeftRow)];

    if (bSameLeftText)
    {
        std::swap(LeftCardIndices[static_cast<size_t>(SelectedLeftRow)], *RightLeftRow);
    }

    const int LeftCard = LeftCardIndices[static_cast<size_t>(SelectedLeftRow)];

    const auto LeftRightText = RightTextByCard.find(LeftCard);
    const bool bMatched = LeftCard == RightCard
        || (LeftRightText != RightTextByCard.end()
            && LeftRightText->second == RightItems[static_cast<qsizetype>(SelectedRightRow)]);

    ResultMarks.push_back(bMatched);

    emit answerGraded(bMatched);

    if (bMatched)
    {
        Session->SetResult(LeftCard, true);
        ++MatchedCount;
        ++TotalMatchedCount;

        if (!IsVoiceLeft())
        {
            SpeakAutomatically(QString::fromStdString(GetDisplayWord(
                *Session->GetCards()[static_cast<size_t>(LeftCard)], TargetLanguage.toStdString())));
        }

        LeftCardIndices[static_cast<size_t>(SelectedLeftRow)] = -1;
        RightCardIndices[static_cast<size_t>(SelectedRightRow)] = -1;
    }
    else
    {
        FlashedLeftRow = SelectedLeftRow;
        FlashedRightRow = SelectedRightRow;
    }

    SelectedLeftRow = -1;
    SelectedRightRow = -1;

    if (IsBatchComplete())
    {
        qCInfo(LogSession) << metaObject()->className() << "batch finished on" << Params.Title;
    }
}

bool SpeedMatchController::IsBatchComplete() const
{
    return !LeftCardIndices.empty() && MatchedCount >= static_cast<int>(LeftCardIndices.size());
}

QJsonObject SpeedMatchController::SaveExtraState() const
{
    QJsonArray LeftIndices;

    for (int Index : LeftCardIndices)
    {
        LeftIndices.append(Index);
    }

    QJsonArray RightIndices;

    for (int Index : RightCardIndices)
    {
        RightIndices.append(Index);
    }

    QJsonArray LeftItemsJson;

    for (const QString& Item : LeftItems)
    {
        LeftItemsJson.append(Item);
    }

    QJsonArray RightItemsJson;

    for (const QString& Item : RightItems)
    {
        RightItemsJson.append(Item);
    }

    QJsonObject Json;
    Json["leftCardIndices"] = LeftIndices;
    Json["rightCardIndices"] = RightIndices;
    Json["leftItems"] = LeftItemsJson;
    Json["rightItems"] = RightItemsJson;

    QJsonObject RightTexts;

    for (const auto& [CardIndex, Text] : RightTextByCard)
    {
        RightTexts[QString::number(CardIndex)] = Text;
    }

    Json["rightTextByCard"] = RightTexts;
    Json["rightMarkups"] = QJsonArray::fromStringList(RightMarkups);
    Json["leftMarkups"] = QJsonArray::fromStringList(LeftMarkups);
    Json["selectedLeftRow"] = SelectedLeftRow;
    Json["selectedRightRow"] = SelectedRightRow;
    Json["flashedLeftRow"] = FlashedLeftRow;
    Json["flashedRightRow"] = FlashedRightRow;
    Json["matchedCount"] = MatchedCount;
    Json["totalMatchedCount"] = TotalMatchedCount;

    return Json;
}

void SpeedMatchController::RestoreExtraState(const QJsonObject& Json)
{
    LeftCardIndices.clear();

    for (const QJsonValue& Index : Json["leftCardIndices"].toArray())
    {
        LeftCardIndices.push_back(Index.toInt());
    }

    RightCardIndices.clear();

    for (const QJsonValue& Index : Json["rightCardIndices"].toArray())
    {
        RightCardIndices.push_back(Index.toInt());
    }

    UpdateHeading();

    LeftItems.clear();

    for (const QJsonValue& Item : Json["leftItems"].toArray())
    {
        LeftItems.append(Item.toString());
    }

    RightItems.clear();

    for (const QJsonValue& Item : Json["rightItems"].toArray())
    {
        RightItems.append(Item.toString());
    }

    RightMarkups.clear();

    for (const QJsonValue& Markup : Json["rightMarkups"].toArray())
    {
        RightMarkups.append(Markup.toString());
    }

    LeftMarkups.clear();

    for (const QJsonValue& Markup : Json["leftMarkups"].toArray())
    {
        LeftMarkups.append(Markup.toString());
    }

    RightTextByCard.clear();

    const QJsonObject RightTexts = Json["rightTextByCard"].toObject();

    for (auto It = RightTexts.constBegin(); It != RightTexts.constEnd(); ++It)
    {
        RightTextByCard[It.key().toInt()] = It.value().toString();
    }

    SelectedLeftRow = Json["selectedLeftRow"].toInt(-1);
    SelectedRightRow = Json["selectedRightRow"].toInt(-1);
    FlashedLeftRow = Json["flashedLeftRow"].toInt(-1);
    FlashedRightRow = Json["flashedRightRow"].toInt(-1);
    MatchedCount = Json["matchedCount"].toInt(0);
    TotalMatchedCount = Json["totalMatchedCount"].toInt(0);
}

void SpeedMatchController::advance()
{
    if (!Session.has_value() || !IsBatchComplete())
    {
        return;
    }

    BuildScreen();

    emit stateChanged();
}

bool SpeedMatchController::IsFinished() const
{
    return ExerciseControllerBase::IsFinished() && LeftCardIndices.empty();
}

int SpeedMatchController::GetCurrentSessionCount() const
{
    return TotalMatchedCount;
}

QStringList SpeedMatchController::GetLeftVerbGroups() const
{
    QStringList Result;
    for (const int Index : LeftCardIndices)
    {
        Result << (!PairText && Session.has_value() && Index >= 0
            ? AppCards::GetVerbGroup(*Session->GetCards()[static_cast<size_t>(Index)]) : QString{});
    }
    return Result;
}

QStringList SpeedMatchController::GetLeftItems() const
{
    QStringList Result;
    Result.reserve(LeftItems.size());

    for (const QString& Item : LeftItems)
    {
        Result.append(GetShownText(Item));
    }

    return Result;
}

QStringList SpeedMatchController::GetRightItems() const
{
    return RightItems;
}

QStringList SpeedMatchController::GetRightMarkups() const
{
    return RightMarkups;
}

QStringList SpeedMatchController::GetLeftMarkups() const
{
    return LeftMarkups;
}

QList<bool> SpeedMatchController::GetLeftMatched() const
{
    QList<bool> Matched;

    for (const int CardIndex : LeftCardIndices)
    {
        Matched.append(CardIndex < 0);
    }

    return Matched;
}

QList<bool> SpeedMatchController::GetRightMatched() const
{
    QList<bool> Matched;

    for (const int CardIndex : RightCardIndices)
    {
        Matched.append(CardIndex < 0);
    }

    return Matched;
}

int SpeedMatchController::GetSelectedLeftRow() const
{
    return SelectedLeftRow;
}

int SpeedMatchController::GetSelectedRightRow() const
{
    return SelectedRightRow;
}

int SpeedMatchController::GetFlashedLeftRow() const
{
    return FlashedLeftRow;
}

int SpeedMatchController::GetFlashedRightRow() const
{
    return FlashedRightRow;
}

bool SpeedMatchController::IsVoiceLeft() const
{
    return SupportsVoicePrompt() && IsVoicePrompt();
}

bool SpeedMatchController::SupportsVoicePrompt() const
{
    return !PairText;
}

bool SpeedMatchController::IsAwaitingAnswer() const
{
    return !LeftCardIndices.empty() && !IsBatchComplete();
}

bool SpeedMatchController::IsShowingFeedback() const
{
    return Session.has_value() && IsBatchComplete();
}

int SpeedMatchController::GetWrongPairCount() const
{
    return static_cast<int>(std::count(ResultMarks.begin(), ResultMarks.end(), false));
}
