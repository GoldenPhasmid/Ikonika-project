#include "AppControllers.h"

#include "core/FTLog.h"

#include <QQmlEngine>

AppControllers::AppControllers(QObject* Parent)
    : QObject(Parent)
    , Daily{new DailySessionController(this)}
    , Training{new IntensiveLearningController(this)}
    , Review{new RepetitionController(this)}
    , Triage{new TriageController(this)}
    , SentenceTraining{new SentenceTrainingController(this)}
    , Discovery{new DiscoveryController(this)}
    , PlacementTest{new PlacementTestController(this)}
    , Launcher{new ExerciseLauncherController(this)}
    , Reader{new ReaderController(this)}
    , Vocabulary{new VocabularyController(this)}
    , DeckVocabulary{new VocabularyController(this)}
    , WordList{new WordListController(this)}
{
    qCInfo(LogApp) << "Created the controllers the screens share";
}

DailySessionController* AppControllers::GetDaily() const
{
    return Daily;
}

IntensiveLearningController* AppControllers::GetTraining() const
{
    return Training;
}

RepetitionController* AppControllers::GetReview() const
{
    return Review;
}

TriageController* AppControllers::GetTriage() const
{
    return Triage;
}

SentenceTrainingController* AppControllers::GetSentenceTraining() const
{
    return SentenceTraining;
}

DiscoveryController* AppControllers::GetDiscovery() const
{
    return Discovery;
}

PlacementTestController* AppControllers::GetPlacementTest() const
{
    return PlacementTest;
}

ExerciseLauncherController* AppControllers::GetLauncher() const
{
    return Launcher;
}

ReaderController* AppControllers::GetReader() const
{
    return Reader;
}

VocabularyController* AppControllers::GetVocabulary() const
{
    return Vocabulary;
}

VocabularyController* AppControllers::GetDeckVocabulary() const
{
    return DeckVocabulary;
}

WordListController* AppControllers::GetWordList() const
{
    return WordList;
}

DictionaryController* AppControllers::dictionary(const QString& Role)
{
    DictionaryController*& Found = Dictionaries[Role];

    if (Found == nullptr)
    {
        Found = new DictionaryController(this);
        QQmlEngine::setObjectOwnership(Found, QQmlEngine::CppOwnership);

        qCDebug(LogApp) << "Created the dictionary controller for" << Role;
    }

    return Found;
}
