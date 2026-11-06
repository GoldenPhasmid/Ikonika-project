#include "ExerciseControllerBase.h"
#include "AppCards.h"
#include "AppDictionary.h"

#include "AppSoundEffects.h"
#include "AppSpeech.h"
#include "ResultMarks.h"

#include "core/FTCard.h"
#include "core/FTCardMeaning.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"
#include "core/FTWordForms.h"

#include <QJsonArray>

#include <algorithm>
#include <optional>

ExerciseControllerBase::ExerciseControllerBase(QObject* Parent)
    : QObject(Parent)
{
    connect(&AppSpeech::Get(), &AppSpeech::speakingChanged, this,
        [this]() { emit speakingChanged(); });

    {
        const QMutexLocker Guard(&AppSpeech::Get().Lock());

        bSpeechEnabled = AppSpeech::Get().IsEnabled();
    }

    connect(&AppSpeech::Get(), &AppSpeech::enabledChanged, this, [this](bool bEnabled)
    {
        bSpeechEnabled = bEnabled;

        if (bVoicePromptRequested)
        {
            qCInfo(LogSession) << metaObject()->className() << "voice prompt" << (bEnabled ? "on" : "off")
                               << "after the speech setting changed";

            emit stateChanged();
        }
    });

    SessionOptionsConnection = connect(SessionOptions, &SessionSettings::changed, this,
                                       [this]() { OnSessionSettingsChanged(); });
}

void ExerciseControllerBase::SetSessionSettings(SessionSettings* InSessionSettings)
{
    if (InSessionSettings == nullptr || InSessionSettings == SessionOptions)
    {
        return;
    }

    disconnect(SessionOptionsConnection);

    SessionOptions = InSessionSettings;
    SessionOptionsConnection = connect(SessionOptions, &SessionSettings::changed, this,
                                       [this]() { OnSessionSettingsChanged(); });

    ApplySessionSettings();

    emit sessionSettingsChanged();
}

SessionSettings* ExerciseControllerBase::GetSessionSettings() const
{
    return SessionOptions;
}

const FTSessionExerciseRules& ExerciseControllerBase::GetSessionRules() const
{
    return SessionOptions->GetRules();
}

FTAnswerComparisonRules ExerciseControllerBase::GetComparisonRules() const
{
    return FTSettings::Get().GetComparisonRules(SessionOptions->GetGroup());
}

void ExerciseControllerBase::ApplySessionSettings()
{
}

void ExerciseControllerBase::OnSessionSettingsChanged()
{
    ApplySessionSettings();

    emit stateChanged();
}

void ExerciseControllerBase::SetVoicePrompt(bool bValue)
{
    if (bVoicePromptRequested == bValue)
    {
        return;
    }

    bVoicePromptRequested = bValue;

    emit stateChanged();
}

bool ExerciseControllerBase::IsVoicePrompt() const
{
    return bVoicePromptRequested && bSpeechEnabled;
}

bool ExerciseControllerBase::IsVoicePromptRequested() const
{
    return bVoicePromptRequested;
}

bool ExerciseControllerBase::SupportsVoicePrompt() const
{
    return false;
}

QString ExerciseControllerBase::GetTitle() const
{
    return Params.Title;
}

const FTCard* ExerciseControllerBase::GetCurrentCard() const
{
    return Session.has_value() ? Session->GetCurrentCard() : nullptr;
}

bool ExerciseControllerBase::updateCard(const QString& CardId, const QString& Word,
                                        const QString& Translation, int Gender,
                                        const QString& PartOfSpeech, const QString& MeaningGist, const QString& Hint)
{
    if (!AppCards::UpdateCardContent(CardId, Word, Translation, Gender, PartOfSpeech, MeaningGist, Hint))
    {
        return false;
    }

    emit stateChanged();

    return true;
}

int ExerciseControllerBase::findWordTranslationStage(const QString& Word, const QString& Translation) const
{
    return AppCards::FindWordTranslationStage(Word, Translation);
}

QVariantMap ExerciseControllerBase::GetCurrentCardEntry() const
{
    const FTCard* Current = GetCurrentCard();

    return Current != nullptr ? AppCards::DescribeCard(*Current) : QVariantMap{};
}

QString ExerciseControllerBase::GetPartOfSpeech() const
{
    const FTCard* Current = GetCurrentCard();

    if (Current == nullptr)
    {
        return {};
    }

    return IsFormsCard(*Current) ? GetTense() : QString::fromStdString(Current->PartOfSpeech);
}

QString ExerciseControllerBase::GetTense() const
{
    const FTCard* Shown = GetCurrentCard();

    if (Shown == nullptr && Session.has_value() && !Session->GetCards().empty())
    {
        Shown = Session->GetCards().front().get();
    }

    return Shown != nullptr ? QString::fromStdString(GetFormsCardTenseLabel(Shown->Id)) : QString();
}

QString ExerciseControllerBase::GetGender() const
{
    const FTCard* Current = GetCurrentCard();

    if (Current == nullptr || !Current->Gender.has_value())
    {
        return {};
    }

    if (*Current->Gender == GenderCommon)
    {
        return tr("any");
    }

    return *Current->Gender == GenderMasculine ? tr("masculine") : tr("feminine");
}

QString ExerciseControllerBase::GetCardMeta() const
{
    const QString Speech = GetPartOfSpeech();
    const QString Gender = GetGender();

    if (Speech.isEmpty() || Gender.isEmpty())
    {
        return Speech.isEmpty() ? Gender : Speech;
    }

    return QStringLiteral("%1 · %2").arg(Speech, Gender);
}

QString ExerciseControllerBase::GetDefinition() const
{
    const FTCard* Current = GetCurrentCard();

    return Current != nullptr ? QString::fromStdString(Current->Definition) : QString();
}

void ExerciseControllerBase::CacheCardMeaning() const
{
    const FTCard* Current = GetCurrentCard();
    const std::string CardId = Current != nullptr ? Current->Id : std::string();

    if (CardId == MeaningCardId)
    {
        return;
    }

    MeaningCardId = CardId;
    ExampleCache.clear();
    ExampleTranslationCache.clear();
    PhoneticCache.clear();

    if (Current == nullptr)
    {
        return;
    }

    PhoneticCache = QString::fromStdString(
        AppDictionary::Get().GetRepository().FindPhonetic(Current->TargetWord, Current->PartOfSpeech));

    if (Current->MeaningGist.empty())
    {
        return;
    }

    const std::vector<FTDictionarySense> Meanings = AppDictionary::Get().GetRepository().LoadMeanings(Current->TargetWord, Current->PartOfSpeech);
    const FTDictionarySense* Meaning = FindCardMeaning(*Current, Meanings);

    if (Meaning == nullptr)
    {
        return;
    }

    ExampleCache = QString::fromStdString(Meaning->Example);
    ExampleTranslationCache = QString::fromStdString(Meaning->ExampleTranslation);
}

QString ExerciseControllerBase::GetExample() const
{
    CacheCardMeaning();

    return ExampleCache;
}

QString ExerciseControllerBase::GetExampleTranslation() const
{
    CacheCardMeaning();

    return ExampleTranslationCache;
}

QString ExerciseControllerBase::GetPhonetic() const
{
    CacheCardMeaning();

    return PhoneticCache;
}

int ExerciseControllerBase::GetElapsedSeconds() const
{
    const qint64 RunningMs = RunTimer.isValid() ? RunTimer.elapsed() : 0;

    return static_cast<int>((AccumulatedElapsedMs + RunningMs) / 1000);
}

bool ExerciseControllerBase::IsFinished() const
{
    if (!Session.has_value())
    {
        return false;
    }

    if (Session->IsFinished())
    {
        return true;
    }
    
    return Params.FixedLength.has_value() && Session->GetDrawnCount() > *Params.FixedLength;
}

int ExerciseControllerBase::GetMaxSessionCount() const
{
    if (ProgressPolicy != nullptr)
    {
        return ProgressPolicy->GetMax();
    }

    if (!Session.has_value())
    {
        return 0;
    }

    return Params.FixedLength.has_value() ? *Params.FixedLength : Session->GetCards().size();
}

int ExerciseControllerBase::GetCurrentSessionCount() const
{
    if (ProgressPolicy != nullptr)
    {
        return ProgressPolicy->GetCurrent();
    }

    if (!Session.has_value())
    {
        return 0;
    }

    return Session->GetDrawnCount();
}

float ExerciseControllerBase::GetExerciseProgress() const
{
    if (ProgressPolicy != nullptr)
    {
        return ProgressPolicy->GetProgress();
    }

    const int CurrentSessionCount = GetCurrentSessionCount();
    const int MaxSessionCount = GetMaxSessionCount();
    return MaxSessionCount > 0 ? (float)CurrentSessionCount / (float)MaxSessionCount : 0;
}

int ExerciseControllerBase::GetMasteryTarget() const
{
    if (ProgressPolicy == nullptr)
    {
        return 0;
    }

    const FTCard* Current = GetCurrentCard();

    return Current != nullptr ? ProgressPolicy->GetCardMasteryTarget(Current->Id) : ProgressPolicy->GetMasteryTarget();
}

int ExerciseControllerBase::GetCardMasteryCount() const
{
    const FTCard* Current = GetCurrentCard();

    if (ProgressPolicy == nullptr || Current == nullptr)
    {
        return 0;
    }

    return ProgressPolicy->GetCardMasteryCount(Current->Id);
}

QString ExerciseControllerBase::GetPrompt() const
{
    return Session.has_value() ? QString::fromStdString(Session->GetPrompt()) : QString();
}

QString ExerciseControllerBase::GetAnswer() const
{
    return Session.has_value() ? QString::fromStdString(Session->GetAnswer()) : QString();
}

bool ExerciseControllerBase::ShowsTargetLanguage() const
{
    return true;
}

bool ExerciseControllerBase::IsAnswered() const
{
    return bAnswered;
}

bool ExerciseControllerBase::IsRetypePending() const
{
    return bRetypePending;
}

QVariantList ExerciseControllerBase::GetResultMarks() const
{
    return ToResultMarks(ResultMarks);
}

void ExerciseControllerBase::SetFormsTable(FFormsTable InFormsTable)
{
    FormsTable = std::move(InFormsTable);
}

const FTCard* ExerciseControllerBase::GetFormsCard() const
{
    return GetCurrentCard();
}

QVariantList ExerciseControllerBase::GetForms() const
{
    const FTCard* Current = GetFormsCard();

    if (!FormsTable || Current == nullptr)
    {
        return {};
    }

    if (FormsCardId != Current->Id)
    {
        FormsCardId = Current->Id;
        FormsCache = FormsTable(*Current);
    }

    return FormsCache;
}

QString ExerciseControllerBase::GetLastRevealedAnswer() const
{
    return LastRevealedAnswer;
}

void ExerciseControllerBase::SetWordMarkup(FWordMarkup InWordMarkup)
{
    WordMarkup = std::move(InWordMarkup);
}

QString ExerciseControllerBase::GetWordMarkup() const
{
    const FTCard* Current = GetCurrentCard();

    return WordMarkup && Current != nullptr ? WordMarkup(*Current) : QString();
}

QString ExerciseControllerBase::GetLastWordMarkup() const
{
    if (!WordMarkup || !Session.has_value() || LastCardIndex < 0)
    {
        return {};
    }

    const FTCard* Last = Session->GetCardState().GetCard(LastCardIndex);

    return Last != nullptr ? WordMarkup(*Last) : QString();
}

std::string ExerciseControllerBase::GetLastWordWithoutArticle() const
{
    if (!Session.has_value() || LastCardIndex < 0)
    {
        return {};
    }

    const FTCard* Last = Session->GetCardState().GetCard(LastCardIndex);

    if (Last == nullptr || GetDisplayWord(*Last, Params.TargetLanguage) == Last->TargetWord)
    {
        return {};
    }

    return Last->TargetWord;
}

const FCardState* ExerciseControllerBase::GetSessionCardState() const
{
    return Session.has_value() ? &Session->GetCardState() : nullptr;
}

QString ExerciseControllerBase::GetLastGivenAnswer() const
{
    return LastGivenAnswer;
}

bool ExerciseControllerBase::WasLastAnswerCorrect() const
{
    return bLastAnswerCorrect;
}

int ExerciseControllerBase::GetLastCardIndex() const
{
    return LastCardIndex;
}

void ExerciseControllerBase::RecordAnswer(bool bCorrect)
{
    RecordResult(bCorrect);

    if (!bCorrect)
    {
        AppSoundEffects::Get().PlayIncorrect();
    }

    emit answerGraded(bCorrect);
}

void ExerciseControllerBase::RecordSkip()
{
    RecordResult(false);
}

bool ExerciseControllerBase::SubmitRetype(const QString& Given)
{
    if (!bAnswered || !bRetypePending)
    {
        return false;
    }

    const bool bAccepted = IsAcceptedAnswer(Given);

    if (bAccepted)
    {
        bRetypePending = false;
    }

    qCDebug(LogSession) << metaObject()->className() << "retype:" << (bAccepted ? "matched" : "wrong")
                        << "- given" << Given;

    emit retypeGraded(bAccepted);
    emit stateChanged();

    return true;
}

bool ExerciseControllerBase::IsAcceptedAnswer(const QString&) const
{
    return false;
}

void ExerciseControllerBase::RecordResult(bool bCorrect)
{
    bLastAnswerCorrect = bCorrect;
    bRetypePending = bRetypeOnMiss && !bCorrect;

    Session->SetResult(bCorrect);
    ResultMarks.push_back(bCorrect);

    bAnswered = true;

    const FTCard* Current = GetCurrentCard();

    if (ProgressPolicy != nullptr && Current != nullptr)
    {
        ProgressPolicy->OnAnswerRecorded(Current->Id, bCorrect);
    }
}

void ExerciseControllerBase::LogIfSessionFinished() const
{
    if (Session.has_value() && Session->IsFinished())
    {
        qCInfo(LogSession) << metaObject()->className() << "completed on" << Params.Title << "-"
                           << GetCurrentSessionCount() << "cards";
    }
}

bool ExerciseControllerBase::IsSpeaking() const
{
    return AppSpeech::Get().IsSpeaking();
}

void ExerciseControllerBase::Speak(const QString& Text) const
{
    if (!Session.has_value() || IsFinished())
    {
        qCInfo(LogSpeech) << metaObject()->className() << "speech skipped, no running exercise:" << Text;

        return;
    }

    if (ProgressPolicy != nullptr && ProgressPolicy->IsFinished())
    {
        qCInfo(LogSpeech) << metaObject()->className() << "speech skipped, stage finished:" << Text;

        return;
    }

    if (AppSpeech::Get().IsSpeechSilenced())
    {
        qCInfo(LogSpeech) << metaObject()->className() << "speech skipped, session restoring at launch:" << Text;

        return;
    }

    const QString Voiced = GetVoicedText(Text);

    qCInfo(LogSpeech) << metaObject()->className() << "speaks:" << Voiced;

    AppSpeech::Get().SpeakAtRate(Voiced, TargetLanguage, SpeechRateScale);
}

void ExerciseControllerBase::SpeakAutomatically(const QString& Text) const
{
    if (AppSpeech::Get().AllowsAutomaticSpeech())
    {
        Speak(Text);
    }
}

const FTCard* ExerciseControllerBase::FindCardByWord(const std::string& Word) const
{
    if (!Session.has_value())
    {
        return nullptr;
    }

    for (const FTCardHandle& Card : Session->GetCards())
    {
        if (Card->TargetWord == Word || GetDisplayWord(*Card, Params.TargetLanguage) == Word)
        {
            return Card.get();
        }
    }

    return nullptr;
}

QString ExerciseControllerBase::GetVoicedText(const QString& Text) const
{
    const FTCard* Card = FindCardByWord(Text.toStdString());

    if (Card == nullptr)
    {
        return Text;
    }

    const bool bSpeakArticle = FTSettings::Get().GetSpeechRules().bSpeakArticle;

    return QString::fromStdString(bSpeakArticle ? GetDisplayWord(*Card, Params.TargetLanguage) : Card->TargetWord);
}

QString ExerciseControllerBase::GetShownText(const QString& Text) const
{
    if (FTSettings::Get().GetExerciseDisplayRules().bShowArticle)
    {
        return Text;
    }

    const FTCard* Card = FindCardByWord(Text.toStdString());

    return Card != nullptr ? QString::fromStdString(GetDisplayHeadword(
        Card->TargetWord, Card->PartOfSpeech, Params.TargetLanguage)) : Text;
}

QString ExerciseControllerBase::GetShownPrompt() const
{
    return GetShownText(GetPrompt());
}

QString ExerciseControllerBase::GetPromptArticle() const
{
    return ShowsTargetLanguage() ? GetTargetArticle() : QString();
}

int ExerciseControllerBase::GetHeadwordGender() const
{
    return ShowsTargetLanguage() ? GetTargetGender() : -1;
}

QString ExerciseControllerBase::GetTargetArticle() const
{
    const FTCard* Current = GetCurrentCard();

    if (Current == nullptr)
    {
        return {};
    }

    const QString Shown = ShowsTargetLanguage() ? GetShownPrompt() : GetShownAnswer();
    const QString Word = QString::fromStdString(Current->TargetWord);

    if (Word.isEmpty() || Shown.size() <= Word.size() || !Shown.endsWith(Word))
    {
        return {};
    }

    return Shown.left(Shown.size() - Word.size());
}

QString ExerciseControllerBase::GetTargetVerbGroup() const
{
    const FTCard* Current = GetCurrentCard();
    return Current != nullptr && !ReadWordForm(*Current).has_value() ? AppCards::GetVerbGroup(*Current) : QString{};
}

QString ExerciseControllerBase::GetHeadwordVerbGroup() const
{
    return ShowsTargetLanguage() ? GetTargetVerbGroup() : QString{};
}

int ExerciseControllerBase::GetTargetGender() const
{
    const FTCard* Current = GetCurrentCard();

    if (Current == nullptr || GetTargetArticle().isEmpty())
    {
        return -1;
    }

    return Current->Gender.value_or(-1);
}

QString ExerciseControllerBase::GetShownAnswer() const
{
    return GetShownText(GetAnswer());
}

void ExerciseControllerBase::speakPrompt()
{
    qCInfo(LogSpeech) << metaObject()->className() << "prompt clicked to speak";

    SpeakPromptIfTargetLanguage();
}

void ExerciseControllerBase::SpeakPromptIfTargetLanguage() const
{
    if (ShowsTargetLanguage())
    {
        Speak(GetPrompt());
    }
}

void ExerciseControllerBase::MarkRunStarted()
{
    RunTimer.start();
}

bool ExerciseControllerBase::RollShowsTargetWord()
{
    return RollShowsTargetWord(FTSettings::Get().GetExerciseDisplayRules().QuestionSide);
}

bool ExerciseControllerBase::RollShowsTargetWord(int Side)
{
    return Side == SHOW_WORD || (Side == SHOW_RANDOM && MakeRandomSeed() % 2);
}

bool ExerciseControllerBase::Start(const FTSessionStartParams& InParams)
{
    Params = InParams;
    restart();
    return true;
}

void ExerciseControllerBase::restart()
{
    AccumulatedElapsedMs = 0;
    MarkRunStarted();

    if (Params.CardState->GetCount() == 0)
    {
        return;
    }

    bAnswered = false;
    bRetypePending = false;
    LastRevealedAnswer.clear();
    ResultMarks.clear();

    InitializeSession();

    qCInfo(LogSession) << metaObject()->className() << "started on" << Params.Title << "-" << Params.CardState->GetCount() << "cards";

    emit stateChanged();
}

void ExerciseControllerBase::InitializeSession()
{
    TargetLanguage = QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage);
    NativeLanguage = QString::fromStdString(FTSettings::Get().GetLanguageRules().NativeLanguage);
    Session = FTExerciseSession{Params};
}

std::optional<FExerciseCheckpoint> ExerciseControllerBase::TakeSession() const
{
    if (!Session.has_value())
    {
        return {};
    }

    FExerciseCheckpoint Checkpoint;
    Checkpoint.SessionState = Session->SaveState();
    Checkpoint.bAnswered = bAnswered;
    Checkpoint.bRetypePending = bRetypePending;
    Checkpoint.LastGivenAnswer = LastGivenAnswer;
    Checkpoint.LastRevealedAnswer = LastRevealedAnswer;
    Checkpoint.bLastAnswerCorrect = bLastAnswerCorrect;
    Checkpoint.LastCardIndex = LastCardIndex;
    Checkpoint.ResultMarks = ResultMarks;
    Checkpoint.Extra = SaveExtraState();
    Checkpoint.ElapsedMilliseconds = AccumulatedElapsedMs + (RunTimer.isValid() ? RunTimer.elapsed() : 0);

    return Checkpoint;
}

bool ExerciseControllerBase::RestoreSession(const FTSessionStartParams& InParams, std::optional<FExerciseCheckpoint> InCheckpoint)
{
    if (!InCheckpoint.has_value())
    {
        return false;
    }

    Params = InParams;
    TargetLanguage = QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage);
    NativeLanguage = QString::fromStdString(FTSettings::Get().GetLanguageRules().NativeLanguage);
    Session = FTExerciseSession(InParams, InCheckpoint->SessionState);

    bAnswered = InCheckpoint->bAnswered;
    bRetypePending = InCheckpoint->bRetypePending;
    LastGivenAnswer = InCheckpoint->LastGivenAnswer;
    LastRevealedAnswer = InCheckpoint->LastRevealedAnswer;
    bLastAnswerCorrect = InCheckpoint->bLastAnswerCorrect;
    LastCardIndex = InCheckpoint->LastCardIndex;
    ResultMarks = InCheckpoint->ResultMarks;

    RestoreExtraState(InCheckpoint->Extra);

    AccumulatedElapsedMs = InCheckpoint->ElapsedMilliseconds;
    MarkRunStarted();

    emit stateChanged();
    return true;
}

QJsonObject ExerciseControllerBase::SaveExtraState() const
{
    return {};
}

void ExerciseControllerBase::RestoreExtraState(const QJsonObject&)
{
}

QJsonObject ExerciseControllerBase::CheckpointToJson(const FExerciseCheckpoint& Checkpoint)
{
    QJsonArray Entries;

    for (int32 Entry : Checkpoint.SessionState.RemainingEntries)
    {
        Entries.append(Entry);
    }

    QJsonArray Marks;

    for (bool Mark : Checkpoint.ResultMarks)
    {
        Marks.append(Mark);
    }

    QJsonObject Json;
    Json["remainingEntries"] = Entries;
    Json["currentIndex"] = Checkpoint.SessionState.CurrentIndex;
    Json["count"] = Checkpoint.SessionState.Count;
    Json["finished"] = Checkpoint.SessionState.bFinished;
    Json["answered"] = Checkpoint.bAnswered;
    Json["retypePending"] = Checkpoint.bRetypePending;
    Json["lastGivenAnswer"] = Checkpoint.LastGivenAnswer;
    Json["lastRevealedAnswer"] = Checkpoint.LastRevealedAnswer;
    Json["lastAnswerCorrect"] = Checkpoint.bLastAnswerCorrect;
    Json["lastCardIndex"] = Checkpoint.LastCardIndex;
    Json["resultMarks"] = Marks;
    Json["extra"] = Checkpoint.Extra;
    Json["elapsedMs"] = Checkpoint.ElapsedMilliseconds;

    return Json;
}

std::optional<FExerciseCheckpoint> ExerciseControllerBase::CheckpointFromJson(const QJsonObject& Json)
{
    if (Json.isEmpty())
    {
        return {};
    }

    FExerciseCheckpoint Checkpoint;

    for (const QJsonValue& Entry : Json["remainingEntries"].toArray())
    {
        Checkpoint.SessionState.RemainingEntries.push_back(Entry.toInt());
    }

    Checkpoint.SessionState.CurrentIndex = Json["currentIndex"].toInt(-1);
    Checkpoint.SessionState.Count = Json["count"].toInt(0);
    Checkpoint.SessionState.bFinished = Json["finished"].toBool(false);
    Checkpoint.bAnswered = Json["answered"].toBool(false);
    Checkpoint.bRetypePending = Json["retypePending"].toBool(false);
    Checkpoint.LastGivenAnswer = Json["lastGivenAnswer"].toString();
    Checkpoint.LastRevealedAnswer = Json["lastRevealedAnswer"].toString();
    Checkpoint.bLastAnswerCorrect = Json["lastAnswerCorrect"].toBool(false);
    Checkpoint.LastCardIndex = Json["lastCardIndex"].toInt(-1);

    for (const QJsonValue& Mark : Json["resultMarks"].toArray())
    {
        Checkpoint.ResultMarks.push_back(Mark.toBool());
    }

    Checkpoint.Extra = Json["extra"].toObject();
    Checkpoint.ElapsedMilliseconds = static_cast<qint64>(Json["elapsedMs"].toDouble(0));

    return Checkpoint;
}

void ExerciseControllerBase::advance()
{
    if (Session.has_value() && !Session->IsFinished())
    {
        Session->Advance();
        emit stateChanged();
    }
}

bool ExerciseControllerBase::removeCard(const QString& CardId)
{
    if (!Session.has_value())
    {
        return false;
    }

    const bool bRemoved = Session->NotifyCardRemoved(CardId.toStdString());

    if (bRemoved)
    {
        emit stateChanged();
    }

    return bRemoved;
}
