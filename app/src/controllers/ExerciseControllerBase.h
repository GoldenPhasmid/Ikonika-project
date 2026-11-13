#pragma once

#include "SessionSettings.h"
#include "core/sessions/FCardState.h"
#include "core/sessions/FTExerciseSession.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <optional>
#include <string>
#include <vector>

struct FExerciseCheckpoint
{
    FTExerciseSessionState SessionState;
    bool bAnswered = false;
    bool bRetypePending = false;
    QString LastGivenAnswer;
    QString LastRevealedAnswer;
    bool bLastAnswerCorrect = false;
    int LastCardIndex = -1;
    std::vector<bool> ResultMarks;
    QJsonObject Extra;
    qint64 ElapsedMilliseconds = 0;
};

using FWordMarkup = std::function<QString(const FTCard& Card)>;
using FFormsTable = std::function<QVariantList(const FTCard& Card)>;

class IExerciseProgressPolicy
{
public:
    virtual ~IExerciseProgressPolicy() = default;
    virtual int GetCurrent() = 0;
    virtual int GetMax() = 0;
    virtual float GetProgress() = 0;

    virtual int GetMasteryTarget() { return 0; }
    virtual int GetCardMasteryTarget(const std::string& CardId) { Q_UNUSED(CardId); return GetMasteryTarget(); }
    virtual int GetCardMasteryCount(const std::string& CardId) { Q_UNUSED(CardId); return 0; }
    virtual bool IsFinished() const { return false; }
    virtual void OnAnswerRecorded(const std::string& CardId, bool bCorrect) { Q_UNUSED(CardId); Q_UNUSED(bCorrect); }
};

class ExerciseControllerBase : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString title READ GetTitle NOTIFY stateChanged)
    Q_PROPERTY(QString partOfSpeech READ GetPartOfSpeech NOTIFY stateChanged)
    Q_PROPERTY(QString tense READ GetTense NOTIFY stateChanged)

    Q_PROPERTY(QString gender READ GetGender NOTIFY stateChanged)
    Q_PROPERTY(QString cardMeta READ GetCardMeta NOTIFY stateChanged)
    Q_PROPERTY(QString definition READ GetDefinition NOTIFY stateChanged)
    Q_PROPERTY(QString example READ GetExample NOTIFY stateChanged)
    Q_PROPERTY(QString exampleTranslation READ GetExampleTranslation NOTIFY stateChanged)
    Q_PROPERTY(QString phonetic READ GetPhonetic NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap currentCard READ GetCurrentCardEntry NOTIFY stateChanged)
    Q_PROPERTY(int elapsedSeconds READ GetElapsedSeconds NOTIFY stateChanged)

    Q_PROPERTY(bool finished READ IsFinished NOTIFY stateChanged)
    Q_PROPERTY(int maxSessionCount READ GetMaxSessionCount NOTIFY stateChanged)
    Q_PROPERTY(int currentSessionCount READ GetCurrentSessionCount NOTIFY stateChanged)
    Q_PROPERTY(float exerciseProgress READ GetExerciseProgress NOTIFY stateChanged)

    Q_PROPERTY(int masteryTarget READ GetMasteryTarget NOTIFY stateChanged)
    Q_PROPERTY(int cardMasteryCount READ GetCardMasteryCount NOTIFY stateChanged)

    Q_PROPERTY(QString prompt READ GetShownPrompt NOTIFY stateChanged)
    Q_PROPERTY(QString promptArticle READ GetPromptArticle NOTIFY stateChanged)
    Q_PROPERTY(int headwordGender READ GetHeadwordGender NOTIFY stateChanged)
    Q_PROPERTY(QString targetArticle READ GetTargetArticle NOTIFY stateChanged)
    Q_PROPERTY(QString targetVerbGroup READ GetTargetVerbGroup NOTIFY stateChanged)
    Q_PROPERTY(QString headwordVerbGroup READ GetHeadwordVerbGroup NOTIFY stateChanged)
    Q_PROPERTY(int targetGender READ GetTargetGender NOTIFY stateChanged)
    Q_PROPERTY(QString answer READ GetShownAnswer NOTIFY stateChanged)
    Q_PROPERTY(bool showsTargetLanguage READ ShowsTargetLanguage NOTIFY stateChanged)
    Q_PROPERTY(bool answered READ IsAnswered NOTIFY stateChanged)
    Q_PROPERTY(bool retypePending READ IsRetypePending NOTIFY stateChanged)

    Q_PROPERTY(QVariantList resultMarks READ GetResultMarks NOTIFY stateChanged)
    Q_PROPERTY(QVariantList forms READ GetForms NOTIFY stateChanged)

    Q_PROPERTY(QString lastRevealedAnswer READ GetLastRevealedAnswer NOTIFY stateChanged)
    Q_PROPERTY(QString lastGivenAnswer READ GetLastGivenAnswer NOTIFY stateChanged)
    Q_PROPERTY(QString wordMarkup READ GetWordMarkup NOTIFY stateChanged)
    Q_PROPERTY(QString lastWordMarkup READ GetLastWordMarkup NOTIFY stateChanged)
    Q_PROPERTY(bool lastAnswerCorrect READ WasLastAnswerCorrect NOTIFY stateChanged)

    Q_PROPERTY(bool speaking READ IsSpeaking NOTIFY speakingChanged)
    Q_PROPERTY(bool voicePrompt READ IsVoicePrompt NOTIFY stateChanged)
    Q_PROPERTY(bool voiceRequested READ IsVoicePromptRequested WRITE SetVoicePrompt NOTIFY stateChanged)
    Q_PROPERTY(bool voiceSupported READ SupportsVoicePrompt NOTIFY stateChanged)

    Q_PROPERTY(SessionSettings* sessionSettings READ GetSessionSettings NOTIFY sessionSettingsChanged)

public:
    explicit ExerciseControllerBase(QObject* Parent = nullptr);

    QString GetTitle() const;

    virtual QString GetPartOfSpeech() const;
    QString GetTense() const;
    virtual QString GetGender() const;
    virtual bool Start(const FTSessionStartParams& InParams);
    virtual const FTCard* GetCurrentCard() const;
    Q_INVOKABLE virtual void restart();

    virtual bool IsFinished() const;
    bool IsSpeaking() const;
    virtual int GetMaxSessionCount() const;
    virtual int GetCurrentSessionCount() const;
    virtual float GetExerciseProgress() const;
    int GetMasteryTarget() const;
    int GetCardMasteryCount() const;

    virtual QString GetPrompt() const;
    virtual QString GetAnswer() const;
    QString GetShownPrompt() const;
    QString GetPromptArticle() const;
    int GetHeadwordGender() const;
    QString GetTargetArticle() const;
    int GetTargetGender() const;
    QString GetTargetVerbGroup() const;
    QString GetHeadwordVerbGroup() const;
    QString GetShownAnswer() const;
    QString GetShownText(const QString& Text) const;
    virtual bool ShowsTargetLanguage() const;
    virtual bool IsAnswered() const;
    bool IsRetypePending() const;

    Q_INVOKABLE virtual void advance();
    Q_INVOKABLE void speakPrompt();
    Q_INVOKABLE virtual void speakSlowly();

    Q_INVOKABLE bool updateCard(const QString& CardId, const QString& Word,
                                const QString& Translation, int Gender,
                                const QString& PartOfSpeech, const QString& MeaningGist, const QString& Hint = QString());

    Q_INVOKABLE int findWordTranslationStage(const QString& Word, const QString& Translation) const;

    QVariantMap GetCurrentCardEntry() const;
    virtual bool removeCard(const QString& CardId);

    QVariantList GetResultMarks() const;

    void SetFormsTable(FFormsTable InFormsTable);
    QVariantList GetForms() const;

    QString GetLastRevealedAnswer() const;

    void SetWordMarkup(FWordMarkup InWordMarkup);
    QString GetWordMarkup() const;
    QString GetLastWordMarkup() const;

    QString GetLastGivenAnswer() const;
    bool WasLastAnswerCorrect() const;

    int GetLastCardIndex() const;

    QString GetCardMeta() const;
    QString GetDefinition() const;
    QString GetExample() const;
    QString GetExampleTranslation() const;
    QString GetPhonetic() const;
    int GetElapsedSeconds() const;

    std::optional<FExerciseCheckpoint> TakeSession() const;
    bool RestoreSession(const FTSessionStartParams& InParams, std::optional<FExerciseCheckpoint> InCheckpoint);

    const FCardState* GetSessionCardState() const;

    static QJsonObject CheckpointToJson(const FExerciseCheckpoint& Checkpoint);
    static std::optional<FExerciseCheckpoint> CheckpointFromJson(const QJsonObject& Json);

    static bool RollShowsTargetWord();
    static bool RollShowsTargetWord(int Side);

    void SetSessionSettings(SessionSettings* InSessionSettings);
    SessionSettings* GetSessionSettings() const;
    const FTSessionExerciseRules& GetSessionRules() const;
    FTAnswerComparisonRules GetComparisonRules() const;

    void SetSpeaksSentences(bool bValue)
    {
        bSpeaksSentences = bValue;
    }

    void SetVoicePrompt(bool bValue);
    virtual bool IsVoicePrompt() const;
    bool IsVoicePromptRequested() const;
    virtual bool SupportsVoicePrompt() const;

    void SetProgressPolicy(IExerciseProgressPolicy* Policy)
    {
        ProgressPolicy = Policy;
    }

protected:
    virtual void InitializeSession();

    virtual const FTCard* GetFormsCard() const;

    virtual void ApplySessionSettings();

    virtual QJsonObject SaveExtraState() const;
    virtual void RestoreExtraState(const QJsonObject& Json);

    void MarkRunStarted();
    void RecordAnswer(bool bCorrect);
    void RecordSkip();
    bool SubmitRetype(const QString& Given);
    virtual bool IsAcceptedAnswer(const QString& Given) const;
    std::string GetLastWordWithoutArticle() const;
    void Speak(const QString& Text) const;
    void SpeakAutomatically(const QString& Text) const;
    void SpeakAtScale(const QString& Text, float RateScale) const;
    QString GetVoicedText(const QString& Text) const;
    const FTCard* FindCardByWord(const std::string& Word) const;
    void SpeakPromptIfTargetLanguage() const;

    void LogIfSessionFinished() const;

    FTSessionStartParams Params;
    std::optional<FTExerciseSession> Session;
    
    QString LastRevealedAnswer;
    QString LastGivenAnswer;
    bool bLastAnswerCorrect = false;
    bool bAnswered = false;
    bool bRetypePending = false;
    bool bRetypeOnMiss = false;
    int LastCardIndex = -1;
    
    std::vector<bool> ResultMarks;

    QString TargetLanguage;
    QString NativeLanguage;

signals:
    void speakingChanged();
    void stateChanged();
    void answerGraded(bool bCorrect);
    void retypeGraded(bool bCorrect);
    void sessionSettingsChanged();

private:
    void RecordResult(bool bCorrect);
    void OnSessionSettingsChanged();

    QElapsedTimer RunTimer;
    qint64 AccumulatedElapsedMs = 0;
    IExerciseProgressPolicy* ProgressPolicy = nullptr;
    FWordMarkup WordMarkup;
    FFormsTable FormsTable;
    mutable std::string FormsCardId;
    mutable QVariantList FormsCache;
    void CacheCardMeaning() const;

    mutable std::string MeaningCardId;
    mutable QString ExampleCache;
    mutable QString ExampleTranslationCache;
    mutable QString PhoneticCache;
    bool bSpeaksSentences = false;
    float GetSpeechRateScale() const;
    mutable QString LastSpokenText;
    mutable std::string LastSpokenCardId;
    SessionSettings* SessionOptions = &SessionSettings::Training();
    QMetaObject::Connection SessionOptionsConnection;
    bool bVoicePromptRequested = false;
    bool bSpeechEnabled = true;
};