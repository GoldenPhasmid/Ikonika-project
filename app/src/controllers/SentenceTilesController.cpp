#include "SentenceTilesController.h"

#include "core/FCardSelectorBase.h"
#include "core/FTLog.h"
#include "core/FTSentenceCards.h"
#include "core/FTSettings.h"

#include <QJsonArray>

#include <algorithm>
#include <numeric>
#include <random>

namespace
{
    QJsonArray IndicesToJson(const std::vector<int>& Indices)
    {
        QJsonArray Result;

        for (const int Index : Indices)
        {
            Result.append(Index);
        }

        return Result;
    }

    QJsonArray WordsToJson(const std::vector<std::string>& Words)
    {
        QJsonArray Result;

        for (const std::string& Word : Words)
        {
            Result.append(QString::fromStdString(Word));
        }

        return Result;
    }

    std::vector<std::string> WordsFromJson(const QJsonArray& Json)
    {
        std::vector<std::string> Result;
        Result.reserve(static_cast<size_t>(Json.size()));

        for (const QJsonValue& Value : Json)
        {
            Result.push_back(Value.toString().toStdString());
        }

        return Result;
    }

    QString FoldedWord(const std::string& Word)
    {
        return QString::fromStdString(Word).toCaseFolded();
    }

    std::vector<int> IndicesFromJson(const QJsonArray& Json)
    {
        std::vector<int> Result;
        Result.reserve(static_cast<size_t>(Json.size()));

        for (const QJsonValue& Value : Json)
        {
            Result.push_back(Value.toInt(-1));
        }

        return Result;
    }
}

SentenceTilesController::SentenceTilesController(QObject* Parent)
    : ExerciseControllerBase(Parent)
{
}

void SentenceTilesController::SetSentenceSource(FSentenceSource InSentenceSource)
{
    SentenceSource = std::move(InSentenceSource);
}

void SentenceTilesController::SetTypingAllowed(bool bAllowed)
{
    bTypingAllowed = bAllowed;
}

void SentenceTilesController::SetBuildsTranslation(bool bBuilds)
{
    bBuildsTranslation = bBuilds;
}

bool SentenceTilesController::IsBuildingTranslation() const
{
    return bBuildsTranslation;
}

bool SentenceTilesController::IsTypingAllowed() const
{
    return bTypingAllowed;
}

bool SentenceTilesController::IsTyping() const
{
    return bTypingAllowed && GetSessionRules().bSentenceTyping;
}

void SentenceTilesController::SetTyping(bool bTyping)
{
    if (!bTypingAllowed || IsTyping() == bTyping)
    {
        return;
    }

    qCInfo(LogSession) << "Sentence stage:" << (bTyping ? "typing whole sentences" : "building sentences from tiles");

    GetSessionSettings()->SetSentenceTyping(bTyping);
}

void SentenceTilesController::ApplySessionSettings()
{
    emit typingChanged();
}

void SentenceTilesController::submitTyped(const QString& Given)
{
    if (!Session.has_value() || bAnswered || Session->IsFinished() || Given.trimmed().isEmpty())
    {
        return;
    }

    const bool bCorrect = IsCorrectGrade(CompareSentence(Given.toStdString(), GetAnswer().toStdString(),
                                                         GetComparisonRules()));

    Reveal(bCorrect, Given.trimmed());
}

FTTreeSentence SentenceTilesController::GetCurrentSentence() const
{
    const FTCard* Current = GetCurrentCard();

    if (Current == nullptr || !SentenceSource)
    {
        return {};
    }

    return SentenceSource(*Current);
}

void SentenceTilesController::startSession()
{
    restart();
}

void SentenceTilesController::InitializeSession()
{
    ExerciseControllerBase::InitializeSession();

    LastTranslation.clear();

    BuildTiles(PickDistractors());
    replay();
}

QJsonObject SentenceTilesController::SaveExtraState() const
{
    QJsonObject Json;
    Json["tileOrder"] = IndicesToJson(TileOrder);
    Json["placed"] = IndicesToJson(Placed);
    Json["lastTranslation"] = LastTranslation;
    Json["distractors"] = WordsToJson(Distractors);

    return Json;
}

void SentenceTilesController::RestoreExtraState(const QJsonObject& Json)
{
    LastTranslation = Json["lastTranslation"].toString();

    BuildTiles(bBuildsTranslation ? WordsFromJson(Json["distractors"].toArray()) : std::vector<std::string>{});

    const std::vector<int> SavedOrder = IndicesFromJson(Json["tileOrder"].toArray());

    if (!std::is_permutation(SavedOrder.begin(), SavedOrder.end(), TileOrder.begin(), TileOrder.end()))
    {
        return;
    }

    TileOrder = SavedOrder;

    for (const int TileIndex : IndicesFromJson(Json["placed"].toArray()))
    {
        const bool bValid = TileIndex >= 0 && TileIndex < static_cast<int>(TileOrder.size())
            && std::find(Placed.begin(), Placed.end(), TileIndex) == Placed.end();

        if (bValid)
        {
            Placed.push_back(TileIndex);
        }
    }
}

void SentenceTilesController::replay()
{
    Speak(GetAnswer());
}

QString SentenceTilesController::GetLastTranslation() const
{
    return LastTranslation;
}

std::vector<std::string> SentenceTilesController::PickDistractors() const
{
    if (!bBuildsTranslation || !Session.has_value() || IsFinished())
    {
        return {};
    }

    std::vector<QString> Taken;

    for (const std::string& Word : SplitSentenceWords(GetPrompt().toStdString()))
    {
        Taken.push_back(FoldedWord(Word));
    }

    std::vector<std::string> Pool;
    const int CurrentIndex = Session->GetCurrentCardIndex();
    const int CardCount = static_cast<int>(Session->GetCards().size());

    for (int Index = 0; Index < CardCount; ++Index)
    {
        if (Index == CurrentIndex)
        {
            continue;
        }

        for (const std::string& Word : SplitSentenceWords(Session->GetAnswerFor(Index)))
        {
            const QString Folded = FoldedWord(Word);

            if (std::find(Taken.begin(), Taken.end(), Folded) == Taken.end())
            {
                Taken.push_back(Folded);
                Pool.push_back(Word);
            }
        }
    }

    std::mt19937 Generator{MakeRandomSeed()};
    std::shuffle(Pool.begin(), Pool.end(), Generator);
    Pool.resize(std::min(Pool.size(), static_cast<size_t>(DistractorCount)));

    return Pool;
}

bool SentenceTilesController::IsPlacedAnswerCorrect() const
{
    std::vector<QString> PlacedWords;

    for (const int TileIndex : Placed)
    {
        PlacedWords.push_back(FoldedWord(Words[static_cast<size_t>(TileOrder[static_cast<size_t>(TileIndex)])]));
    }

    std::vector<QString> AnswerWords;

    for (size_t Index = 0; Index < AnswerWordCount; ++Index)
    {
        AnswerWords.push_back(FoldedWord(Words[Index]));
    }

    if (bBuildsTranslation)
    {
        return std::is_permutation(PlacedWords.begin(), PlacedWords.end(), AnswerWords.begin(), AnswerWords.end());
    }

    return PlacedWords == AnswerWords;
}

void SentenceTilesController::BuildTiles(const std::vector<std::string>& InDistractors)
{
    Words.clear();
    Distractors.clear();
    TileOrder.clear();
    Placed.clear();
    AnswerWordCount = 0;

    const FTCard* Current = GetCurrentCard();

    if (Current == nullptr || IsFinished())
    {
        return;
    }

    if (bBuildsTranslation)
    {
        Words = SplitSentenceWords(GetPrompt().toStdString());
    }
    else
    {
        Words = SplitSentenceWords(SentenceSource ? GetCurrentSentence().Text : Current->TargetWord);
    }

    AnswerWordCount = Words.size();
    Distractors = InDistractors;
    Words.insert(Words.end(), Distractors.begin(), Distractors.end());

    TileOrder.resize(Words.size());
    std::iota(TileOrder.begin(), TileOrder.end(), 0);

    const bool bCanDiffer = std::adjacent_find(Words.begin(), Words.end(),
                                               std::not_equal_to<>()) != Words.end();

    std::mt19937 Generator{MakeRandomSeed()};

    do
    {
        std::shuffle(TileOrder.begin(), TileOrder.end(), Generator);
    }
    while (bCanDiffer && std::is_sorted(TileOrder.begin(), TileOrder.end()));
}

QString SentenceTilesController::GetPrompt() const
{
    if (SentenceSource)
    {
        return QString::fromStdString(GetCurrentSentence().Translation);
    }

    return Session.has_value() ? QString::fromStdString(Session->GetAnswer()) : QString();
}

QString SentenceTilesController::GetAnswer() const
{
    if (SentenceSource)
    {
        return QString::fromStdString(GetCurrentSentence().Text);
    }

    return Session.has_value() ? QString::fromStdString(Session->GetPrompt()) : QString();
}

bool SentenceTilesController::ShowsTargetLanguage() const
{
    return false;
}

QStringList SentenceTilesController::GetTiles() const
{
    QStringList Result;

    for (const int WordIndex : TileOrder)
    {
        Result.push_back(QString::fromStdString(Words[static_cast<size_t>(WordIndex)]));
    }

    return Result;
}

QVariantList SentenceTilesController::GetPlaced() const
{
    QVariantList Result;

    for (const int TileIndex : Placed)
    {
        Result.push_back(TileIndex);
    }

    return Result;
}

void SentenceTilesController::placeTile(int TileIndex)
{
    if (!Session.has_value() || bAnswered || Session->IsFinished())
    {
        return;
    }

    const bool bValid = TileIndex >= 0 && TileIndex < static_cast<int>(TileOrder.size());

    if (!bValid || Placed.size() >= AnswerWordCount || std::find(Placed.begin(), Placed.end(), TileIndex) != Placed.end())
    {
        return;
    }

    Placed.push_back(TileIndex);

    emit stateChanged();
}

bool SentenceTilesController::CanSubmit() const
{
    return Session.has_value() && !bAnswered && !Session->IsFinished() && AnswerWordCount > 0
        && Placed.size() == AnswerWordCount;
}

void SentenceTilesController::submit()
{
    if (!CanSubmit())
    {
        return;
    }

    QStringList Given;

    for (const int PlacedIndex : Placed)
    {
        Given.push_back(QString::fromStdString(Words[static_cast<size_t>(TileOrder[static_cast<size_t>(PlacedIndex)])]));
    }

    Reveal(IsPlacedAnswerCorrect(), Given.join(QLatin1Char(' ')));
}

void SentenceTilesController::removeTile(int TileIndex)
{
    if (!Session.has_value() || bAnswered)
    {
        return;
    }

    const auto Found = std::find(Placed.begin(), Placed.end(), TileIndex);

    if (Found == Placed.end())
    {
        return;
    }

    Placed.erase(Found);

    emit stateChanged();
}

void SentenceTilesController::skip()
{
    if (!Session.has_value() || bAnswered || Session->IsFinished())
    {
        return;
    }

    qCDebug(LogSession) << metaObject()->className() << "answer: skipped (don't remember)";

    Reveal(false, QString());
}

void SentenceTilesController::Reveal(bool bCorrect, const QString& Given)
{
    LastCardIndex = Session->GetCurrentCardIndex();
    LastGivenAnswer = Given;
    LastRevealedAnswer = GetAnswer();
    LastTranslation = GetPrompt();

    RecordAnswer(bCorrect);

    qCDebug(LogSession) << metaObject()->className() << "answer:" << (bCorrect ? "correct" : "wrong")
                        << "- given" << LastGivenAnswer << ", expected" << LastRevealedAnswer;

    emit stateChanged();
}

void SentenceTilesController::advance()
{
    if (!Session.has_value() || !bAnswered)
    {
        return;
    }

    Session->Advance();
    bAnswered = false;

    LogIfSessionFinished();

    BuildTiles(PickDistractors());
    replay();

    emit stateChanged();
}
