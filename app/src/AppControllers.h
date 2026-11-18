#pragma once

#include "controllers/DailySessionController.h"
#include "controllers/DictionaryController.h"
#include "controllers/DiscoveryController.h"
#include "controllers/ExerciseLauncherController.h"
#include "controllers/IntensiveLearningController.h"
#include "controllers/PlacementTestController.h"
#include "controllers/ReaderController.h"
#include "controllers/RepetitionController.h"
#include "controllers/SentenceTrainingController.h"
#include "controllers/TriageController.h"
#include "controllers/VocabularyController.h"
#include "controllers/WordListController.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class AppControllers : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The screens are given their controllers")

    Q_PROPERTY(DailySessionController* daily READ GetDaily CONSTANT)
    Q_PROPERTY(IntensiveLearningController* training READ GetTraining CONSTANT)
    Q_PROPERTY(RepetitionController* review READ GetReview CONSTANT)
    Q_PROPERTY(TriageController* triage READ GetTriage CONSTANT)
    Q_PROPERTY(SentenceTrainingController* sentenceTraining READ GetSentenceTraining CONSTANT)
    Q_PROPERTY(DiscoveryController* discovery READ GetDiscovery CONSTANT)
    Q_PROPERTY(PlacementTestController* placementTest READ GetPlacementTest CONSTANT)
    Q_PROPERTY(ExerciseLauncherController* launcher READ GetLauncher CONSTANT)
    Q_PROPERTY(ReaderController* reader READ GetReader CONSTANT)
    Q_PROPERTY(VocabularyController* vocabulary READ GetVocabulary CONSTANT)
    Q_PROPERTY(VocabularyController* deckVocabulary READ GetDeckVocabulary CONSTANT)
    Q_PROPERTY(WordListController* wordList READ GetWordList CONSTANT)

public:
    explicit AppControllers(QObject* Parent = nullptr);

    DailySessionController* GetDaily() const;
    IntensiveLearningController* GetTraining() const;
    RepetitionController* GetReview() const;
    TriageController* GetTriage() const;
    SentenceTrainingController* GetSentenceTraining() const;
    DiscoveryController* GetDiscovery() const;
    PlacementTestController* GetPlacementTest() const;
    ExerciseLauncherController* GetLauncher() const;
    ReaderController* GetReader() const;
    VocabularyController* GetVocabulary() const;
    VocabularyController* GetDeckVocabulary() const;
    WordListController* GetWordList() const;

    Q_INVOKABLE DictionaryController* dictionary(const QString& Role);

private:
    DailySessionController* Daily;
    IntensiveLearningController* Training;
    RepetitionController* Review;
    TriageController* Triage;
    SentenceTrainingController* SentenceTraining;
    DiscoveryController* Discovery;
    PlacementTestController* PlacementTest;
    ExerciseLauncherController* Launcher;
    ReaderController* Reader;
    VocabularyController* Vocabulary;
    VocabularyController* DeckVocabulary;
    WordListController* WordList;
    QHash<QString, DictionaryController*> Dictionaries;
};
