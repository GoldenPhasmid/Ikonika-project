#pragma once

#include "core/FTAnswerComparison.h"
#include "core/FTTypes.h"

#include <QList>
#include <QSettings>
#include <QString>
#include <QVariant>

#include <functional>
#include <string>
#include <utility>
#include <vector>

struct FTScheduleRules
{
    std::vector<int> IntervalDays{1, 2, 4, 14, 40, 120};

    int MaximumDaysInFirstStage = 3;

    bool bFirstStageFailureReturnsToPool = false;

    bool bMissedReviewDemotes = true;
};

void Sanitise(FTScheduleRules& Rules);

struct FTIntensiveLearningRules
{
    int TrainingWordCount = 25;
    int SessionsPerDay = 1;
    
    int GetDailyGoal() const
    {
        return TrainingWordCount * SessionsPerDay;
    }
    
    int CardsPerExercise = 10;
};

void Sanitise(FTIntensiveLearningRules& Rules);

struct FTLanguageRules
{
    /** ISO 639-1, lowercase. The language being learned. */
    std::string TargetLanguage = "fr";

    /** ISO 639-1, lowercase. The language translations are given in. */
    std::string NativeLanguage = "en";
};

void Sanitise(FTLanguageRules& Rules);

/** What the dictionary shows, as opposed to what it holds. */
struct FTDictionaryDisplayRules
{
    /** Example sentences shown under each sense. Some senses carry dozens, which buries the
        senses below them; three is enough to see how the word is used. 0 hides them. */
    int ExamplesPerSense = 3;
};

void Sanitise(FTDictionaryDisplayRules& Rules);

inline constexpr int MinimumCachedLineCount = 10;
inline constexpr int MaximumCachedLineCount = 200;

struct FTSpeechRules
{
    int CachedLineCount = 50;

    /** Off leaves the article out of what the voice reads. What is shown and what a typed answer
        is checked against do not change. */
    bool bSpeakArticle = true;

    bool bAutoSpeak = true;
};

void Sanitise(FTSpeechRules& Rules);

struct FTExerciseDisplayRules
{
    int QuestionSide = SHOW_RANDOM;

    bool bShowArticle = true;
};

struct FTFormsRules
{
    bool bOptionalPronoun = true;
};

struct FTReminderRules
{
    bool bEnabled = false;

    /** Minutes from midnight, so it survives being stored as one number. */
    int TimeOfDayMinutes = 19 * 60;

    /** Shown in the notification. The user's own words if they set any. */
    std::string Message;
};

struct FTAnswerLeniencyRules
{
    /** Accepts "chien" for "le chien". Off by default: the article is part of the word being
        learned, which is why every exercise shows and speaks it. */
    bool bAllowMissingArticle = false;
};

inline constexpr int MaximumSessionsPerDay = 5;

inline constexpr int MaximumFlipCount = 5;
inline constexpr int MaximumStageRepeatCount = 3;

inline constexpr const char* TrainingSessionGroup = "intensiveLearning";
inline constexpr const char* ReviewSessionGroup = "repetition";

struct FTSessionExerciseRules
{
    int FlipCount = 2;

    bool bChoiceStage = true;
    int ChoiceCount = 1;
    int ChoiceSide = SHOW_RANDOM;
    bool bChoiceVoice = false;

    int RepeatsToMaster = 3;
    bool bTypeWordExercise = true;
    bool bGapExercise = true;
    bool bAudioExercise = true;
    bool bRequireExactAccents = false;

    bool bSentenceStage = true;
    bool bSentenceTyping = false;

    bool bMatchStage = true;
    int MatchCount = 1;
    bool bMatchVoice = true;

    bool operator==(const FTSessionExerciseRules&) const = default;
};

void Sanitise(FTSessionExerciseRules& Rules);

FTSessionExerciseRules MakeDefaultSessionExercises(const std::string& Group);

struct FTRepetitionRules
{
    int ReviewWordCount = 20;
};

void Sanitise(FTRepetitionRules& Rules);

std::string SerializeSettingValue(const QVariant& Value);
QVariant ParseSettingValue(const std::string& Text);

class FTSettings
{
public:
    explicit FTSettings(const QString& FilePath);

    static FTSettings& Get();
    static void SetActive(FTSettings* Settings);

    /** The shipped default for a key, or the compiled-in one when the file does not name it. */
    static QVariant GetShippedDefault(const char* Key, const QVariant& Compiled);

    using ChangeListener = std::function<void(const QString& Key, const QVariant& Value)>;

    void SetChangeListener(ChangeListener Listener);
    std::vector<std::pair<QString, QVariant>> ReadSyncedValues() const;
    void ApplySyncedValues(const std::vector<std::pair<QString, QVariant>>& Values);
    void RestoreSyncedValues(const std::vector<std::pair<QString, QVariant>>& Values);

    const FTScheduleRules& GetScheduleRules() const;

    void SetIntervalDays(const QList<int>& Days);
    void SetMaximumDaysInFirstStage(int Days);
    void SetFirstStageFailureReturnsToPool(bool bValue);
    void SetMissedReviewDemotes(bool bValue);

    QList<int> GetIntervalDays() const;

    const FTIntensiveLearningRules& GetIntensiveLearningRules() const;

    void SetCardsPerExercise(int Value);
    void SetSessionsPerDay(int Value);
    void SetTrainingWordCount(int Value);

    const FTRepetitionRules& GetRepetitionRules() const;

    void SetReviewWordCount(int Value);

    const FTSessionExerciseRules& GetSessionExercises(const std::string& Group) const;
    void SetSessionExercises(const std::string& Group, const FTSessionExerciseRules& Value);

    const FTLanguageRules& GetLanguageRules() const;

    const FTAnswerLeniencyRules& GetLeniencyRules() const;
    void SetAllowMissingArticle(bool bValue);

    FTAnswerComparisonRules GetComparisonRules(const std::string& Group) const;

    const FTReminderRules& GetReminderRules() const;

    const FTDictionaryDisplayRules& GetDictionaryDisplayRules() const;

    void SetExamplesPerSense(int Value);

    const std::string& GetWebSearchSource() const;

    std::vector<std::string> GetEssentialVerbs(const std::string& Language) const;
    void SetWebSearchSource(const std::string& Id);

    const FTSpeechRules& GetSpeechRules() const;

    void SetCachedLineCount(int Value);
    void SetSpeakArticle(bool bValue);
    void SetAutoSpeak(bool bValue);

    const FTExerciseDisplayRules& GetExerciseDisplayRules() const;

    void SetQuestionSide(int Side);
    void SetShowArticle(bool bValue);

    const FTFormsRules& GetFormsRules() const;
    void SetOptionalPronoun(bool bValue);

    void SetReminderEnabled(bool bValue);

    /** Whether the desktop app starts with the user's sign-in. */
    bool GetLaunchAtStartup() const;
    void SetLaunchAtStartup(bool bValue);
    void SetReminderTimeMinutes(int Minutes);
    void SetReminderMessage(const std::string& Text);

    void SetTargetLanguage(const std::string& Code);
    void SetNativeLanguage(const std::string& Code);
    
    bool SetLanguagePair(const std::string& TargetCode, const std::string& NativeCode);
    
    static std::vector<FTLanguageRules> GetAvailableLanguagePairs();
    
    static void SetAvailableLanguagePairs(std::vector<FTLanguageRules> Pairs);
    
    void RevalidateLanguageRules();

    bool HasChosenLanguage() const;

    void AdoptLanguagePair(const FTLanguageRules& Pair);

#ifdef QT_DEBUG
    bool IsDebugContinueForced() const;
    void SetDebugContinueForced(bool bValue);
#endif
    
    void ReloadFromDisk();

    QSettings OpenStore() const;

private:
    FTSettings();
    void Write(const QString& Key, const QVariant& Value);

    QString FilePath;
    ChangeListener OnChange;

    FTScheduleRules Rules;
    FTIntensiveLearningRules IntensiveRules;
    FTRepetitionRules RepetitionRules;
    FTSessionExerciseRules TrainingExercises;
    FTSessionExerciseRules ReviewExercises;
    FTAnswerLeniencyRules LeniencyRules;
    FTLanguageRules LanguageRules;
    FTReminderRules ReminderRules;
    bool bLaunchAtStartup = false;
    FTDictionaryDisplayRules DictionaryDisplayRules;
    std::string WebSearchSource;
    FTSpeechRules SpeechRules;

    FTExerciseDisplayRules ExerciseDisplayRules;
    FTFormsRules FormsRules;

#ifdef QT_DEBUG
    bool bDebugContinueForced = false;
#endif
};
