#include "ExerciseLauncherController.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AudioTranscriptionController.h"
#include "CorrectIncorrectController.h"
#include "FlashcardController.h"
#include "FlipRecallController.h"
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

    ExerciseControllerBase* NewController = CreateController(Exercise);
    if (NewController == nullptr)
    {
        return false;
    }

    if (auto* Written = qobject_cast<WrittenProductionController*>(NewController))
    {
        Written->SetAskWord(!bReverse);
    }

    const int DesiredCardCount = CardCount >= 0
        ? std::clamp(CardCount, 1, (int)Cards.size())
        : (int)Cards.size();

    Cards.resize(DesiredCardCount);

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
