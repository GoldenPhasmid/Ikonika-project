#pragma once

#include "core/FTDeckProgress.h"
#include "core/FTDiscoveryPlanner.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

class DiscoveryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QObject* activeController READ GetActiveController CONSTANT)
    Q_PROPERTY(bool finished READ IsSessionFinished CONSTANT)
    Q_PROPERTY(int stage READ GetStage CONSTANT)
    Q_PROPERTY(int stageCount READ GetStageCount CONSTANT)
    Q_PROPERTY(QString tense READ GetTense CONSTANT)
    Q_PROPERTY(float exerciseProgress READ GetExerciseProgress NOTIFY stateChanged)
    Q_PROPERTY(QVariantList forms READ GetForms NOTIFY stateChanged)

    Q_PROPERTY(bool roundOver READ IsRoundOver NOTIFY stateChanged)
    Q_PROPERTY(bool hasActiveSession READ HasActiveSession NOTIFY stateChanged)
    Q_PROPERTY(QString deckName READ GetDeckName NOTIFY stateChanged)
    Q_PROPERTY(QString promptKind READ GetPromptKind NOTIFY stateChanged)

    Q_PROPERTY(QString word READ GetWord NOTIFY stateChanged)
    Q_PROPERTY(QString displayWord READ GetDisplayWord NOTIFY stateChanged)
    Q_PROPERTY(QString article READ GetArticle NOTIFY stateChanged)
    Q_PROPERTY(QString partOfSpeech READ GetPartOfSpeech NOTIFY stateChanged)
    Q_PROPERTY(QString gender READ GetGender NOTIFY stateChanged)
    Q_PROPERTY(int headwordGender READ GetHeadwordGender NOTIFY stateChanged)
    Q_PROPERTY(QString phonetic READ GetPhonetic NOTIFY stateChanged)

    Q_PROPERTY(QString translations READ GetTranslations NOTIFY stateChanged)
    Q_PROPERTY(QString definition READ GetDefinition NOTIFY stateChanged)
    Q_PROPERTY(QString example READ GetExample NOTIFY stateChanged)
    Q_PROPERTY(QString exampleTranslation READ GetExampleTranslation NOTIFY stateChanged)
    Q_PROPERTY(int exampleSpanStart READ GetExampleSpanStart NOTIFY stateChanged)
    Q_PROPERTY(int exampleSpanLength READ GetExampleSpanLength NOTIFY stateChanged)
    Q_PROPERTY(QString beginnerSentence READ GetBeginnerSentence NOTIFY stateChanged)
    Q_PROPERTY(int beginnerSpanStart READ GetBeginnerSpanStart NOTIFY stateChanged)
    Q_PROPERTY(int beginnerSpanLength READ GetBeginnerSpanLength NOTIFY stateChanged)

    Q_PROPERTY(QStringList options READ GetOptions NOTIFY stateChanged)
    Q_PROPERTY(QVariantList struckOptions READ GetStruckOptions NOTIFY stateChanged)
    Q_PROPERTY(int correctOptionIndex READ GetCorrectOptionIndex NOTIFY stateChanged)
    Q_PROPERTY(int chosenOptionIndex READ GetChosenOptionIndex NOTIFY stateChanged)
    Q_PROPERTY(bool answered READ IsAnswered NOTIFY stateChanged)
    Q_PROPERTY(bool showsExample READ ShowsExample NOTIFY stateChanged)
    Q_PROPERTY(bool lastClue READ IsLastClue NOTIFY stateChanged)
    Q_PROPERTY(QString outcome READ GetOutcome NOTIFY stateChanged)
    Q_PROPERTY(bool learn READ IsLearn NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap meaning READ GetMeaning NOTIFY stateChanged)
    Q_PROPERTY(QVariantList translationOptions READ GetTranslationOptions NOTIFY stateChanged)
    Q_PROPERTY(QString translation READ GetTranslation NOTIFY stateChanged)

    Q_PROPERTY(int askedCount READ GetAskedCount NOTIFY stateChanged)
    Q_PROPERTY(int questionCount READ GetQuestionCount NOTIFY stateChanged)

    Q_PROPERTY(QVariantList results READ GetResults NOTIFY resultsChanged)

public:
    explicit DiscoveryController(QObject* Parent = nullptr);

    Q_INVOKABLE bool startDeck(qint64 DeckId);

    Q_INVOKABLE bool startDecks(const QVariantList& DeckIds);

    Q_INVOKABLE QVariantList savedDecks() const;

    Q_INVOKABLE void saveDecks(const QVariantList& DeckIds);

    Q_INVOKABLE bool restart();

    Q_INVOKABLE void choose(int Index);

    Q_INVOKABLE void askClue();

    Q_INVOKABLE void setTranslation(const QString& Translation);

    Q_INVOKABLE void commit(bool bLearn);

    Q_INVOKABLE void leave();

    Q_INVOKABLE bool updateCard(const QString& CardId, const QString& Word, const QString& Translation, int Gender,
                                const QString& PartOfSpeech, const QString& MeaningGist, const QString& Hint);

    Q_INVOKABLE int findWordTranslationStage(const QString& Word, const QString& Translation) const;

    Q_INVOKABLE void restartStage();

    Q_INVOKABLE void endSession();

    Q_INVOKABLE void speakWord();

    Q_INVOKABLE void speakSlowly();

    QObject* GetActiveController();
    bool IsSessionFinished() const;
    int GetStage() const;
    int GetStageCount() const;
    QString GetTense() const;
    float GetExerciseProgress() const;
    QVariantList GetForms() const;
    bool IsRoundOver() const;
    bool HasActiveSession() const;
    QString GetDeckName() const;
    QString GetPromptKind() const;
    QString GetWord() const;
    QString GetDisplayWord() const;
    QString GetArticle() const;
    QString GetPartOfSpeech() const;
    QString GetGender() const;
    int GetHeadwordGender() const;
    QString GetPhonetic() const;
    QString GetTranslations() const;
    QString GetDefinition() const;
    QString GetExample() const;
    QString GetExampleTranslation() const;
    int GetExampleSpanStart() const;
    int GetExampleSpanLength() const;
    QString GetBeginnerSentence() const;
    int GetBeginnerSpanStart() const;
    int GetBeginnerSpanLength() const;
    QStringList GetOptions() const;
    QVariantList GetStruckOptions() const;
    int GetCorrectOptionIndex() const;
    int GetChosenOptionIndex() const;
    bool IsAnswered() const;
    bool ShowsExample() const;
    bool IsLastClue() const;
    QString GetOutcome() const;
    bool IsLearn() const;
    QVariantMap GetMeaning() const;
    QVariantList GetTranslationOptions() const;
    QString GetTranslation() const;
    int GetAskedCount() const;
    int GetQuestionCount() const;
    QVariantList GetResults() const;
    int GetLearnCount() const;

signals:
    void stateChanged();
    void resultsChanged();

private:
    struct FWritten
    {
        std::optional<std::string> CardId;

        bool bMarked = false;
    };

    const FTDiscoveryQuestion* GetQuestion() const;

    const FTDiscoveryRow* GetQuestionRow() const;

    void SpeakClue();

    QString FormatHeadword(const FTDiscoveryRow& Row) const;

    void WriteResults();

    void WriteResult(std::size_t Index);

    void AnnounceQuestion();

    std::unique_ptr<FTDiscoveryPlanner> Planner;

    std::vector<FWritten> Written;

    std::vector<FTDeckWord> DeckWords;

    std::vector<qint64> DeckIds;

    QString DeckName;

};
