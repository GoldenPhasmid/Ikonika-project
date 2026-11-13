#include "ExerciseLauncherController.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AudioTranscriptionController.h"
#include "CorrectIncorrectController.h"
#include "ExerciseFlowStages.h"
#include "FlashcardController.h"
#include "FlipRecallController.h"
#include "GapProductionController.h"
#include "MultipleChoiceController.h"
#include "SpeedMatchController.h"
#include "VocabularyController.h"
#include "WrittenProductionController.h"

#include "core/FCardSelectorBase.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <algorithm>
#include <random>

ExerciseLauncherController::ExerciseLauncherController(QObject* Parent)
    : QObject(Parent)
{
}

ExerciseControllerBase* ExerciseLauncherController::CreateController(const QString& Exercise)
{
    if (Exercise == QStringLiteral("flashcards"))
    {
        return new FlashcardController(this);
    }

    if (Exercise == QStringLiteral("flip"))
    {
        return new FlipRecallController(this);
    }

    if (Exercise == QStringLiteral("correctincorrect"))
    {
        return new CorrectIncorrectController(this);
    }

    if (Exercise == QStringLiteral("choice"))
    {
        return new MultipleChoiceController(this);
    }

    if (Exercise == QStringLiteral("audio"))
    {
        return new AudioTranscriptionController(this);
    }

    if (Exercise == QStringLiteral("speedmatch"))
    {
        return new SpeedMatchController(this);
    }

    if (Exercise == QStringLiteral("written"))
    {
        return new WrittenProductionController(this);
    }

    if (Exercise == QStringLiteral("gap"))
    {
        return new WordGapController(this);
    }

    return nullptr;
}

bool ExerciseLauncherController::startExercise(const QString& Exercise, bool bReverse, int CardCount,
    int RepetitionCount, const QStringList& FilterKeys)
{
    std::vector<FTCardHandle> Cards = AppCards::LoadCards();

    Cards.erase(std::remove_if(Cards.begin(), Cards.end(), [&FilterKeys](const FTCardHandle& Card)
    {
        return !Card || !VocabularyController::MatchesAnyFilter(*Card, FilterKeys);
    }), Cards.end());

    if (Cards.empty())
    {
        qCInfo(LogApp) << "Practice" << Exercise << "has no cards matching" << FilterKeys;

        return false;
    }

    std::mt19937 Generator{MakeRandomSeed()};
    std::shuffle(Cards.begin(), Cards.end(), Generator);

    const int DesiredCardCount = CardCount >= 0
        ? std::clamp(CardCount, 1, (int)Cards.size())
        : (int)Cards.size();

    Cards.resize(DesiredCardCount);

    std::shared_ptr<const FGapSentences> GapSentences;

    if (Exercise == QStringLiteral("gap"))
    {
        GapSentences = std::make_shared<const FGapSentences>(PickGapSentences(FCardState{Cards}));

        Cards.erase(std::remove_if(Cards.begin(), Cards.end(), [&GapSentences](const FTCardHandle& Card)
        {
            return !GapSentences->contains(Card->Id);
        }), Cards.end());

        if (Cards.empty())
        {
            qCInfo(LogApp) << "Practice gap has no sentences for the chosen cards";

            return false;
        }
    }

    ExerciseControllerBase* NewController = CreateController(Exercise);
    if (NewController == nullptr)
    {
        return false;
    }

    if (auto* Gap = qobject_cast<WordGapController*>(NewController))
    {
        Gap->SetSentenceLookup(MakeGapSentenceLookup(GapSentences));
    }
    else if (auto* Written = qobject_cast<WrittenProductionController*>(NewController))
    {
        Written->SetAskWord(!bReverse);
    }

    const int DesiredRepetitionCount = RepetitionCount >= 0 ? RepetitionCount
        : FTSettings::Get().GetIntensiveLearningRules().CardsPerExercise;

    FTSessionStartParams Params{};
    Params.Title = tr("My Vocabulary");
    Params.TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;
    Params.NativeLanguage = FTSettings::Get().GetLanguageRules().NativeLanguage;
    Params.CardState = std::make_shared<FCardState>(Cards);
    Params.Selector = std::make_shared<FEachOnceSelector>(Params.CardState->GetCount(),
        DesiredRepetitionCount > Params.CardState->GetCount());
    Params.FixedLength = DesiredRepetitionCount;

    if (!NewController->Start(Params))
    {
        NewController->deleteLater();
        return false;
    }

    if (ActiveController != nullptr)
    {
        ActiveController->deleteLater();
    }

    ActiveController = NewController;

    emit activeControllerChanged();

    return true;
}

ExerciseControllerBase* ExerciseLauncherController::GetActiveController() const
{
    return ActiveController;
}
