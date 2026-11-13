#pragma once

#include "SessionSettings.h"
#include "core/FTCardStore.h"
#include "core/FTTypes.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <string>
#include <vector>

class SettingsStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool ready READ IsReady CONSTANT)
    Q_PROPERTY(QString databasePath READ GetDatabasePath CONSTANT)

    Q_PROPERTY(QString lastError READ GetLastError NOTIFY lastErrorChanged)

    Q_PROPERTY(bool audioAvailable READ IsAudioAvailable NOTIFY audioSettingsChanged)
    Q_PROPERTY(bool audioEnabled READ IsAudioEnabled WRITE SetAudioEnabled NOTIFY audioSettingsChanged)

    Q_PROPERTY(int speakerCount READ GetSpeakerCount NOTIFY audioSettingsChanged)
    Q_PROPERTY(bool supertonicActive READ IsSupertonicActive NOTIFY audioSettingsChanged)
    Q_PROPERTY(int speakerId READ GetSpeakerId WRITE SetSpeakerId NOTIFY audioSettingsChanged)

    Q_PROPERTY(qreal speechSpeed READ GetSpeechSpeed WRITE SetSpeechSpeed NOTIFY audioSettingsChanged)
    Q_PROPERTY(qreal sentenceSpeechSpeed READ GetSentenceSpeechSpeed WRITE SetSentenceSpeechSpeed NOTIFY audioSettingsChanged)

    Q_PROPERTY(int cachedLineCount READ GetCachedLineCount WRITE SetCachedLineCount
               NOTIFY audioSettingsChanged)
    Q_PROPERTY(int minimumCachedLineCount READ GetMinimumCachedLineCount CONSTANT)
    Q_PROPERTY(int maximumCachedLineCount READ GetMaximumCachedLineCount CONSTANT)
    Q_PROPERTY(qreal minimumSpeechSpeed READ GetMinimumSpeechSpeed CONSTANT)
    Q_PROPERTY(qreal maximumSpeechSpeed READ GetMaximumSpeechSpeed CONSTANT)

    Q_PROPERTY(QVariantList intervalDays READ GetIntervalDays NOTIFY scheduleSettingsChanged)
    Q_PROPERTY(int stageCount READ GetStageCount NOTIFY scheduleSettingsChanged)
    Q_PROPERTY(int maximumStageCount READ GetMaximumStageCount CONSTANT)
    Q_PROPERTY(int maximumIntervalDays READ GetMaximumIntervalDays CONSTANT)

    Q_PROPERTY(int daysInFirstStage READ GetDaysInFirstStage WRITE SetDaysInFirstStage
               NOTIFY scheduleSettingsChanged)

    Q_PROPERTY(int dailyGoal READ GetDailyGoal NOTIFY learningSettingsChanged)
    Q_PROPERTY(int sessionsPerDay READ GetSessionsPerDay WRITE SetSessionsPerDay
               NOTIFY learningSettingsChanged)
    Q_PROPERTY(int cardsPerExercise READ GetCardsPerExercise WRITE SetCardsPerExercise
               NOTIFY learningSettingsChanged)
    Q_PROPERTY(int trainingWordCount READ GetTrainingWordCount WRITE SetTrainingWordCount
               NOTIFY learningSettingsChanged)
    Q_PROPERTY(int reviewWordCount READ GetReviewWordCount WRITE SetReviewWordCount
               NOTIFY learningSettingsChanged)

    Q_PROPERTY(SessionSettings* training READ GetTrainingSettings CONSTANT)
    Q_PROPERTY(SessionSettings* review READ GetReviewSettings CONSTANT)

    /** How many example sentences the dictionary shows under each sense. */
    Q_PROPERTY(int examplesPerSense READ GetExamplesPerSense WRITE SetExamplesPerSense
               NOTIFY learningSettingsChanged)

    Q_PROPERTY(QStringList webSearchSourceNames READ GetWebSearchSourceNames NOTIFY webSearchChanged)
    Q_PROPERTY(int webSearchSourceIndex READ GetWebSearchSourceIndex WRITE SetWebSearchSourceIndex
               NOTIFY webSearchChanged)

    Q_PROPERTY(bool speakArticle READ IsArticleSpoken WRITE SetSpeakArticle
               NOTIFY audioSettingsChanged)

    Q_PROPERTY(bool autoSpeak READ IsAutoSpeak WRITE SetAutoSpeak NOTIFY audioSettingsChanged)

    Q_PROPERTY(int questionSide READ GetQuestionSide WRITE SetQuestionSide
               NOTIFY questionSideChanged)

    Q_PROPERTY(QStringList questionSideNames READ GetQuestionSideNames CONSTANT)

    Q_PROPERTY(bool showArticle READ IsArticleShown WRITE SetShowArticle NOTIFY showArticleChanged)
    Q_PROPERTY(bool allowMissingArticle READ IsMissingArticleAllowed WRITE SetAllowMissingArticle
               NOTIFY learningSettingsChanged)
    Q_PROPERTY(QString optionalPronoun READ GetOptionalPronoun NOTIFY languagesChanged)
    Q_PROPERTY(bool learnOptionalPronoun READ IsOptionalPronounLearned WRITE SetLearnOptionalPronoun
               NOTIFY learningSettingsChanged)
    Q_PROPERTY(bool missedReviewDemotes READ DoesMissedReviewDemote WRITE SetMissedReviewDemotes
               NOTIFY scheduleSettingsChanged)
    Q_PROPERTY(int maximumSessionsPerDay READ GetMaximumSessionsPerDay CONSTANT)

    Q_PROPERTY(QStringList targetLanguageNames READ GetTargetLanguageNames
               NOTIFY languageNamesChanged)
    Q_PROPERTY(int targetLanguageIndex READ GetTargetLanguageIndex WRITE SetTargetLanguageIndex
               NOTIFY languagesChanged)
    Q_PROPERTY(QStringList nativeLanguageNames READ GetNativeLanguageNames
               NOTIFY languageNamesChanged)
    Q_PROPERTY(int nativeLanguageIndex READ GetNativeLanguageIndex WRITE SetNativeLanguageIndex
               NOTIFY languagesChanged)
    Q_PROPERTY(QStringList nativeLanguageCodes READ GetNativeLanguageCodes NOTIFY languagesChanged)

    Q_PROPERTY(QString targetLanguageName READ GetTargetLanguageName NOTIFY languageNamesChanged)
    Q_PROPERTY(QString nativeLanguageName READ GetNativeLanguageName NOTIFY languageNamesChanged)

    Q_PROPERTY(QString targetLanguageCode READ GetTargetLanguageCode NOTIFY languagesChanged)

    Q_PROPERTY(QString languagePairLabel READ GetLanguagePairLabel NOTIFY languagesChanged)

    Q_PROPERTY(int dictionaryWordCount READ GetDictionaryWordCount NOTIFY languagesChanged)

    Q_PROPERTY(int debugDayOffset READ GetDebugDayOffset WRITE SetDebugDayOffset
               NOTIFY debugTimeChanged)

    Q_PROPERTY(QString debugTodayText READ GetDebugTodayText NOTIFY debugTimeChanged)

    Q_PROPERTY(bool debugBuild READ IsDebugBuild CONSTANT)

public:
    explicit SettingsStore(QObject* Parent = nullptr);

    Q_INVOKABLE void clearLastError();

    Q_INVOKABLE bool nukeDatabase();

    Q_INVOKABLE void playVoiceSample();

    Q_INVOKABLE void playSentenceVoiceSample();

    Q_INVOKABLE void playVoiceSampleNow();

    /** Sets one stage's interval. Out of range does nothing. */
    Q_INVOKABLE void setIntervalDays(int Stage, int Days);

    /** Adds a stage at the end, taking twice the last interval. Capped. */
    Q_INVOKABLE void addStage();

    Q_INVOKABLE void removeStage();

    Q_INVOKABLE void resetScheduleToDefaults();

    bool IsReady() const;
    QString GetLastError() const;
    QString GetDatabasePath() const;

    bool IsAudioAvailable() const;
    bool IsAudioEnabled() const;
    void SetAudioEnabled(bool bValue);

    int GetSpeakerCount() const;
    bool IsSupertonicActive() const;
    int GetSpeakerId() const;
    void SetSpeakerId(int Value);

    qreal GetSpeechSpeed() const;
    void SetSpeechSpeed(qreal Value);
    qreal GetSentenceSpeechSpeed() const;
    void SetSentenceSpeechSpeed(qreal Value);
    qreal GetMinimumSpeechSpeed() const;
    qreal GetMaximumSpeechSpeed() const;

    int GetCachedLineCount() const;
    void SetCachedLineCount(int Value);
    int GetMinimumCachedLineCount() const;
    int GetMaximumCachedLineCount() const;

    QVariantList GetIntervalDays() const;
    int GetStageCount() const;
    int GetMaximumStageCount() const;
    int GetMaximumIntervalDays() const;

    int GetDaysInFirstStage() const;
    void SetDaysInFirstStage(int Value);

    int GetDailyGoal() const;

    int GetSessionsPerDay() const;
    void SetSessionsPerDay(int Value);

    int GetCardsPerExercise() const;
    void SetCardsPerExercise(int Value);

    int GetTrainingWordCount() const;
    void SetTrainingWordCount(int Value);

    int GetReviewWordCount() const;
    void SetReviewWordCount(int Value);

    SessionSettings* GetTrainingSettings() const;
    SessionSettings* GetReviewSettings() const;

    int GetExamplesPerSense() const;
    void SetExamplesPerSense(int Value);

    QStringList GetWebSearchSourceNames() const;
    int GetWebSearchSourceIndex() const;
    void SetWebSearchSourceIndex(int Index);

    bool IsArticleSpoken() const;
    void SetSpeakArticle(bool bValue);

    bool IsAutoSpeak() const;
    void SetAutoSpeak(bool bValue);

    int GetQuestionSide() const;
    void SetQuestionSide(int Value);
    QStringList GetQuestionSideNames() const;

    bool IsArticleShown() const;
    void SetShowArticle(bool bValue);

    bool IsMissingArticleAllowed() const;
    void SetAllowMissingArticle(bool bValue);

    QString GetOptionalPronoun() const;
    bool IsOptionalPronounLearned() const;
    void SetLearnOptionalPronoun(bool bValue);

    bool DoesMissedReviewDemote() const;
    void SetMissedReviewDemotes(bool bValue);

    int GetMaximumSessionsPerDay() const;

    Q_INVOKABLE int cycleQuestionSide();

    QStringList GetTargetLanguageNames() const;
    int GetTargetLanguageIndex() const;
    void SetTargetLanguageIndex(int Index);

    Q_INVOKABLE void selectTargetLanguage(const QString& Code);
    Q_INVOKABLE void selectLanguagePair(const QString& TargetCode, const QString& NativeCode);

    QStringList GetNativeLanguageNames() const;
    QStringList GetNativeLanguageCodes() const;
    int GetNativeLanguageIndex() const;
    void SetNativeLanguageIndex(int Index);

    QString GetTargetLanguageName() const;
    QString GetNativeLanguageName() const;
    QString GetLanguagePairLabel() const;
    QString GetTargetLanguageCode() const;

    int GetDictionaryWordCount() const;

    int GetDebugDayOffset() const;
    void SetDebugDayOffset(int Days);
    QString GetDebugTodayText() const;

    static constexpr bool IsDebugBuild()
    {
#ifdef QT_DEBUG
        return true;
#else
        return false;
#endif
    }

signals:
    void lastErrorChanged();

    void audioSettingsChanged();

    void scheduleSettingsChanged();

    void learningSettingsChanged();

    void webSearchChanged();

    void questionSideChanged();
    void showArticleChanged();

    void languagesChanged();
    void languageNamesChanged();

    void debugTimeChanged();

private:
    void SetLastError(const QString& Message);

    void ApplyLanguagePair(const std::string& TargetLanguage, const std::string& NativeLanguage);

    void PlaySampleNow();

    void OnSettingsApplied();

    QString LastError;

    QTimer SampleDelay;
    bool bSampleIsSentence = false;
};
