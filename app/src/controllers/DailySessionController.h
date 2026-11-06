#pragma once

#include <QObject>
#include <QDate>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <string>
#include <unordered_map>

class DailySessionController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QDate learningDate READ GetLearningDate NOTIFY countsChanged)
    Q_PROPERTY(int weekday READ GetWeekday NOTIFY countsChanged)

    /** Cards due for repetition today. */
    Q_PROPERTY(int dueCount READ GetDueCount NOTIFY countsChanged)

    /** New words a session would take, which is the training count or fewer when nothing is left
        to fill it with. */
    Q_PROPERTY(int newWordCount READ GetNewWordCount NOTIFY countsChanged)

    /** Words waiting unlearned in the Learning Pool. */
    Q_PROPERTY(int poolCount READ GetPoolCount NOTIFY countsChanged)

    /** How many words one session works through. */
    Q_PROPERTY(int trainingWordCount READ GetTrainingWordCount NOTIFY countsChanged)

    /** How many words triage has to collect before a session starting from the pool can be filled.
        Zero when the pool already holds enough. */
    Q_PROPERTY(int wantedFromTriage READ GetWantedFromTriage NOTIFY countsChanged)

    /** The day's target for new words, which several sessions can add up to. */
    Q_PROPERTY(int dailyGoal READ GetDailyGoal NOTIFY countsChanged)

    /** How many of the daily goal's words are still wanted today, floored at zero once a session
        (or several) already met it. */
    Q_PROPERTY(int wordsLeftForGoal READ GetWordsLeftForGoal NOTIFY countsChanged)

    Q_PROPERTY(int learnedToday READ GetLearnedToday NOTIFY countsChanged)
    Q_PROPERTY(int reviewedToday READ GetReviewedToday NOTIFY countsChanged)

    /** What the rail shows beside Today: new words and repetition together. */
    Q_PROPERTY(int totalCount READ GetTotalCount NOTIFY countsChanged)

    /**
     * How many words are due today and on each of the next several days: one entry per day,
     * carrying daysFromToday (0 for today) and count. Review only
     */
    Q_PROPERTY(QVariantList reviewTimeline READ GetReviewTimeline NOTIFY countsChanged)

    /**
     * The last seven days, oldest first: daysAgo (0 for today), recorded (false for a day with no
     * session), and how far review and learn got that day, each from 0 to 1.
     */
    Q_PROPERTY(QVariantList recentProgress READ GetRecentProgress NOTIFY countsChanged)

    /**
     * Every word of the language pair along the ladder: the pool, then one entry per box, then
     * completed. Each entry carries kind ("pool", "box" or "learned"), stage and count.
     */
    Q_PROPERTY(QVariantList ladder READ GetLadder NOTIFY countsChanged)

public:
    explicit DailySessionController(QObject* Parent = nullptr);

    /** Re-reads the counts. Called when the screen appears and after a session finishes. */
    Q_INVOKABLE void refresh();

    Q_INVOKABLE QStringList collectSessionCards() const;

    Q_INVOKABLE int shortfall() const;

    Q_INVOKABLE QString formsTrainingId(const QString& Training) const;

    Q_INVOKABLE int formsTrainingSize(const QString& TrainingId) const;

    /** Words a training could start on now: the ones waiting plus the ones triage could still offer. */
    Q_INVOKABLE int formsWordCount(const QString& TrainingId) const;

    Q_INVOKABLE bool canStartFormsTraining(const QString& TrainingId) const;

    /** A full session for the training: the words waiting, then studied words picked at random. */
    Q_INVOKABLE QStringList collectFormsCards(const QString& TrainingId) const;

    Q_INVOKABLE QVariantList formsTenses(const QString& TrainingId) const;

    Q_INVOKABLE QStringList collectTenseCards(const QString& TrainingId, const QString& Tense) const;

    int GetDueCount() const;
    QDate GetLearningDate() const;
    int GetWeekday() const;
    int GetNewWordCount() const;
    int GetDailyGoal() const;
    int GetWordsLeftForGoal() const;
    int GetLearnedToday() const;
    int GetReviewedToday() const;
    int GetTotalCount() const;
    int GetPoolCount() const;
    int GetTrainingWordCount() const;
    int GetWantedFromTriage() const;
    QVariantList GetReviewTimeline() const;
    QVariantList GetRecentProgress() const;
    QVariantList GetLadder() const;

signals:
    void countsChanged();

private:
    void DropStaleFormsCache() const;

    mutable std::string FormsCacheRevision;
    mutable std::unordered_map<std::string, int> FormsWordCounts;
    mutable std::unordered_map<std::string, QVariantList> FormsTenses;

    int DueCount = 0;
    QDate LearningDate;
    int PoolCount = 0;
    int LearnedToday = 0;
    int ReviewedToday = 0;

    QVariantList ReviewTimeline;
    QVariantList Ladder;
    QVariantList RecentProgress;
};
