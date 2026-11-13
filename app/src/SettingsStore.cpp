#include "SettingsStore.h"

#include "AppCatalog.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppPackageDownloader.h"
#include "AppSpeech.h"
#include "AppWebSearch.h"
#include "controllers/AppSyncController.h"

#include "core/FTLog.h"
#include "core/FTSchedule.h"
#include "core/FTSchema.h"
#include "core/FTCardStore.h"
#include "core/FTSettings.h"
#include "core/FTWordForms.h"

#include <QDate>
#include <QMutexLocker>
#include <QVariantMap>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <set>

#include "AppLocalization.h"

namespace
{
    /** A line for the voice sample button, chosen so the differences between voices are audible in
        one sentence rather than needing several words to emerge. A language without a line plays
        nothing, since another language's sentence read in its voice is worse than silence. */
    QString VoiceSampleLine(const std::string& TargetLanguage)
    {
        if (TargetLanguage == "en")
        {
            return SettingsStore::tr("The little dog eats a red apple and drinks cold water from the old well.");
        }

        if (TargetLanguage == "es")
        {
            // Rolled r, the ñ, and a diphthong.
            return SettingsStore::tr("El perro pequeño come una manzana roja y bebe agua fría.");
        }

        if (TargetLanguage == "pt")
        {
            // Nasal vowels, a reduced final vowel and the lh sound.
            return SettingsStore::tr("O cão pequeno come uma maçã vermelha e bebe água fria do velho poço.");
        }

        if (TargetLanguage == "fr")
        {
            // Nasal vowels, a liaison and two accents.
            return SettingsStore::tr("Le chien mange une pomme rouge et boit de l'eau fraîche.");
        }

        if (TargetLanguage == "it")
        {
            return SettingsStore::tr("Il cagnolino mangia una mela rossa e beve acqua fredda dal vecchio pozzo.");
        }

        return {};
    }
}

SettingsStore::SettingsStore(QObject* Parent)
    : QObject(Parent)
{
    // Long enough that stepping through voices or dragging the slider renders once, short enough
    // that a single deliberate change still sounds immediate.
    SampleDelay.setInterval(350);
    SampleDelay.setSingleShot(true);

    connect(&SampleDelay, &QTimer::timeout, this, &SettingsStore::PlaySampleNow);

    // Voices load on a worker after startup; audioAvailable, voiceNames and speakerCount read as
    // "no voice yet" until this fires, then as whatever actually loaded.
    connect(&AppSpeech::Get(), &AppSpeech::ready, this,
            &SettingsStore::audioSettingsChanged);
    connect(&AppSpeech::Get(), &AppSpeech::sentenceSpeedChanged, this,
            &SettingsStore::audioSettingsChanged);
    connect(&AppCatalog::Get(), &AppCatalog::catalogChanged, this,
            &SettingsStore::audioSettingsChanged);
    connect(&AppCatalog::Get(), &AppCatalog::catalogChanged, this,
            &SettingsStore::webSearchChanged);
    connect(this, &SettingsStore::languagesChanged, this, &SettingsStore::languageNamesChanged);
    connect(this, &SettingsStore::languagesChanged, this, &SettingsStore::webSearchChanged);
    connect(&AppLocalization::Get(), &AppLocalization::languageChanged, this,
            &SettingsStore::languageNamesChanged);

    if (AppSyncController* Sync{AppSyncController::GetInstance()})
    {
        connect(Sync, &AppSyncController::dataMerged, this, &SettingsStore::OnSettingsApplied);
    }

    if (!AppDatabase::Get().Initialise())
    {
        SetLastError(AppDatabase::Get().GetLastError());

        return;
    }

}

bool SettingsStore::IsReady() const
{
    return AppDatabase::Get().IsReady() && AppDictionary::Get().IsReady();
}

QString SettingsStore::GetLastError() const
{
    return LastError;
}

void SettingsStore::clearLastError()
{
    SetLastError(QString());
}

QString SettingsStore::GetDatabasePath() const
{
    return AppDatabase::Get().GetFilePath();
}

bool SettingsStore::IsAudioAvailable() const
{
    const QMutexLocker Guard(&AppSpeech::Get().Lock());

    return AppSpeech::Get().IsAvailable();
}

bool SettingsStore::IsAudioEnabled() const
{
    const QMutexLocker Guard(&AppSpeech::Get().Lock());

    return AppSpeech::Get().IsEnabled();
}

void SettingsStore::SetAudioEnabled(bool bValue)
{
    {
        const QMutexLocker Guard(&AppSpeech::Get().Lock());

        if (AppSpeech::Get().IsEnabled() == bValue)
        {
            return;
        }
    }

    AppSpeech::Get().SetEnabled(bValue);

    qCInfo(LogApp) << "Setting changed: audio enabled ->" << bValue;

    emit audioSettingsChanged();
}

void SettingsStore::OnSettingsApplied()
{
    const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

    if (!AppDictionary::Get().IsOpenFor(Rules.TargetLanguage, Rules.NativeLanguage))
    {
        qCInfo(LogApp) << "The merge changed the language pair to" << QString::fromStdString(Rules.TargetLanguage)
                       << "->" << QString::fromStdString(Rules.NativeLanguage) << ", dictionary opened"
                       << AppDictionary::Get().OpenSettingsPair();

        AppDatabase::Get().ApplyActiveLanguages();

        emit languagesChanged();
        emit audioSettingsChanged();
    }

    emit scheduleSettingsChanged();
    emit learningSettingsChanged();
    emit questionSideChanged();
    emit showArticleChanged();
    emit webSearchChanged();

    SessionSettings::NotifyDisplayChanged();
}

int SettingsStore::GetSpeakerCount() const
{
    const QMutexLocker Guard(&AppSpeech::Get().Lock());

    return AppSpeech::Get().GetVoiceInfo(AppSpeech::Get().GetActiveVoice()).SpeakerCount;
}

bool SettingsStore::IsSupertonicActive() const
{
    const QMutexLocker Guard(&AppSpeech::Get().Lock());

    return AppSpeech::Get().GetVoiceInfo(AppSpeech::Get().GetActiveVoice()).Engine == SupertonicEngine;
}

int SettingsStore::GetSpeakerId() const
{
    const QMutexLocker Guard(&AppSpeech::Get().Lock());

    return AppSpeech::Get().GetSpeakerId();
}

void SettingsStore::SetSpeakerId(int Value)
{
    {
        const QMutexLocker Guard(&AppSpeech::Get().Lock());

        if (AppSpeech::Get().GetSpeakerId() == Value)
        {
            return;
        }
    }

    AppSpeech::Get().SetSpeakerId(Value);

    qCInfo(LogApp) << "Setting changed: speaker ->" << Value;

    emit audioSettingsChanged();
}

qreal SettingsStore::GetSpeechSpeed() const
{
    const QMutexLocker Guard(&AppSpeech::Get().Lock());

    return AppSpeech::Get().GetSpeed();
}

void SettingsStore::SetSpeechSpeed(qreal Value)
{
    {
        const QMutexLocker Guard(&AppSpeech::Get().Lock());

        if (qFuzzyCompare(AppSpeech::Get().GetSpeed(), static_cast<float>(Value)))
        {
            return;
        }
    }

    AppSpeech::Get().SetSpeed(static_cast<float>(Value));

    qCInfo(LogApp) << "Setting changed: speech speed ->" << Value;

    emit audioSettingsChanged();
}

qreal SettingsStore::GetSentenceSpeechSpeed() const
{
    return AppSpeech::Get().GetSentenceSpeed();
}

void SettingsStore::SetSentenceSpeechSpeed(qreal Value)
{
    if (qFuzzyCompare(AppSpeech::Get().GetSentenceSpeed(), static_cast<float>(Value)))
    {
        return;
    }

    AppSpeech::Get().SetSentenceSpeed(static_cast<float>(Value));

    qCInfo(LogApp) << "Setting changed: sentence speech speed ->" << Value;

    emit audioSettingsChanged();
}

qreal SettingsStore::GetMinimumSpeechSpeed() const
{
    return MinimumSpeechSpeed;
}

qreal SettingsStore::GetMaximumSpeechSpeed() const
{
    return MaximumSpeechSpeed;
}

int SettingsStore::GetCachedLineCount() const
{
    // From the settings rather than the service: the service is loaded on a worker and reports
    // nothing useful until it is, while the setting is what the user actually chose.
    return FTSettings::Get().GetSpeechRules().CachedLineCount;
}

void SettingsStore::SetCachedLineCount(int Value)
{
    if (GetCachedLineCount() == Value)
    {
        return;
    }

    AppSpeech::Get().SetCachedLineCount(Value);

    qCInfo(LogApp) << "Setting changed: cached voice lines ->" << GetCachedLineCount();

    emit audioSettingsChanged();
}

int SettingsStore::GetMinimumCachedLineCount() const
{
    return MinimumCachedLineCount;
}

int SettingsStore::GetMaximumCachedLineCount() const
{
    return MaximumCachedLineCount;
}

void SettingsStore::SetLastError(const QString& Message)
{
    if (LastError == Message)
    {
        return;
    }

    LastError = Message;

    emit lastErrorChanged();
}

bool SettingsStore::nukeDatabase()
{
    if (!IsReady())
    {
        return false;
    }

    if (!AppDatabase::Get().GetCardRepository().DeleteEverything())
    {
        SetLastError(tr("Could not clear the database."));

        qCWarning(LogApp) << "Failed to clear the database";

        return false;
    }

    qCWarning(LogApp) << "Database cleared: every word removed";

    SetLastError(QString());

    return true;
}

QVariantList SettingsStore::GetIntervalDays() const
{
    QVariantList Result;

    for (const int Days : FTSettings::Get().GetIntervalDays())
    {
        Result.append(Days);
    }

    return Result;
}

int SettingsStore::GetStageCount() const
{
    return GetLastStage();
}

int SettingsStore::GetMaximumStageCount() const
{
    return MaximumLadderStages;
}

int SettingsStore::GetMaximumIntervalDays() const
{
    return MaximumIntervalDays;
}

int SettingsStore::GetDaysInFirstStage() const
{
    return FTSettings::Get().GetScheduleRules().MaximumDaysInFirstStage;
}

void SettingsStore::SetDaysInFirstStage(int Value)
{
    if (GetDaysInFirstStage() == Value)
    {
        return;
    }

    FTSettings::Get().SetMaximumDaysInFirstStage(Value);

    qCInfo(LogApp) << "Setting changed: days in first stage ->" << Value;

    emit scheduleSettingsChanged();
}

int SettingsStore::GetDailyGoal() const
{
    return FTSettings::Get().GetIntensiveLearningRules().GetDailyGoal();
}

int SettingsStore::GetSessionsPerDay() const
{
    return FTSettings::Get().GetIntensiveLearningRules().SessionsPerDay;
}

void SettingsStore::SetSessionsPerDay(int Value)
{
    if (GetSessionsPerDay() == Value)
    {
        return;
    }

    FTSettings::Get().SetSessionsPerDay(Value);

    qCInfo(LogApp) << "Setting changed: sessions per day ->" << GetSessionsPerDay();

    emit learningSettingsChanged();
}

bool SettingsStore::IsArticleSpoken() const
{
    return FTSettings::Get().GetSpeechRules().bSpeakArticle;
}

void SettingsStore::SetSpeakArticle(bool bValue)
{
    if (IsArticleSpoken() == bValue)
    {
        return;
    }

    FTSettings::Get().SetSpeakArticle(bValue);

    qCInfo(LogApp) << "Setting changed: speak the article ->" << bValue;

    emit audioSettingsChanged();
}

bool SettingsStore::IsAutoSpeak() const
{
    return FTSettings::Get().GetSpeechRules().bAutoSpeak;
}

void SettingsStore::SetAutoSpeak(bool bValue)
{
    if (IsAutoSpeak() == bValue)
    {
        return;
    }

    FTSettings::Get().SetAutoSpeak(bValue);

    qCInfo(LogApp) << "Setting changed: auto speak ->" << bValue;

    emit audioSettingsChanged();
}

int SettingsStore::GetQuestionSide() const
{
    return FTSettings::Get().GetExerciseDisplayRules().QuestionSide;
}

bool SettingsStore::IsArticleShown() const
{
    return FTSettings::Get().GetExerciseDisplayRules().bShowArticle;
}

void SettingsStore::SetShowArticle(bool bValue)
{
    if (IsArticleShown() == bValue)
    {
        return;
    }

    FTSettings::Get().SetShowArticle(bValue);

    qCInfo(LogApp) << "Setting changed: show the article ->" << bValue;

    SessionSettings::NotifyDisplayChanged();

    emit showArticleChanged();
}

bool SettingsStore::IsMissingArticleAllowed() const
{
    return FTSettings::Get().GetLeniencyRules().bAllowMissingArticle;
}

void SettingsStore::SetAllowMissingArticle(bool bValue)
{
    if (IsMissingArticleAllowed() == bValue)
    {
        return;
    }

    FTSettings::Get().SetAllowMissingArticle(bValue);

    qCInfo(LogApp) << "Setting changed: allow missing article ->" << bValue;

    emit learningSettingsChanged();
}

QString SettingsStore::GetOptionalPronoun() const
{
    return QString::fromStdString(GetOptionalFormsPronoun(FTSettings::Get().GetLanguageRules().TargetLanguage));
}

bool SettingsStore::IsOptionalPronounLearned() const
{
    return FTSettings::Get().GetFormsRules().bOptionalPronoun;
}

void SettingsStore::SetLearnOptionalPronoun(bool bValue)
{
    if (IsOptionalPronounLearned() == bValue)
    {
        return;
    }

    FTSettings::Get().SetOptionalPronoun(bValue);

    qCInfo(LogApp) << "Setting changed: learn the optional pronoun ->" << bValue;

    emit learningSettingsChanged();
}

bool SettingsStore::DoesMissedReviewDemote() const
{
    return FTSettings::Get().GetScheduleRules().bMissedReviewDemotes;
}

void SettingsStore::SetMissedReviewDemotes(bool bValue)
{
    if (DoesMissedReviewDemote() == bValue)
    {
        return;
    }

    FTSettings::Get().SetMissedReviewDemotes(bValue);

    qCInfo(LogApp) << "Setting changed: missed review demotes ->" << bValue;

    emit scheduleSettingsChanged();
}

int SettingsStore::GetMaximumSessionsPerDay() const
{
    return MaximumSessionsPerDay;
}

SessionSettings* SettingsStore::GetTrainingSettings() const
{
    return &SessionSettings::Training();
}

SessionSettings* SettingsStore::GetReviewSettings() const
{
    return &SessionSettings::Review();
}

void SettingsStore::SetQuestionSide(int Value)
{
    if (Value < 0 || Value >= QuestionSideCount)
    {
        return;
    }

    if (GetQuestionSide() == Value)
    {
        return;
    }

    FTSettings::Get().SetQuestionSide(Value);

    qCInfo(LogApp) << "Setting changed: question side ->" << GetQuestionSide();

    emit questionSideChanged();
}

QStringList SettingsStore::GetQuestionSideNames() const
{
    // The word being learned is named by what it is rather than by its language, since the pair
    // being learned can change and these would then read backwards.
    return {tr("Word"), tr("Random"), tr("Translation")};
}

int SettingsStore::cycleQuestionSide()
{
    SetQuestionSide((GetQuestionSide() + 1) % QuestionSideCount);

    return GetQuestionSide();
}

int SettingsStore::GetCardsPerExercise() const
{
    return FTSettings::Get().GetIntensiveLearningRules().CardsPerExercise;
}

void SettingsStore::SetCardsPerExercise(int Value)
{
    if (GetCardsPerExercise() == Value)
    {
        return;
    }

    FTSettings::Get().SetCardsPerExercise(Value);

    qCInfo(LogApp) << "Setting changed: cards per exercise ->" << GetCardsPerExercise();

    emit learningSettingsChanged();
}

int SettingsStore::GetTrainingWordCount() const
{
    return FTSettings::Get().GetIntensiveLearningRules().TrainingWordCount;
}

void SettingsStore::SetTrainingWordCount(int Value)
{
    if (GetTrainingWordCount() == Value)
    {
        return;
    }

    FTSettings::Get().SetTrainingWordCount(Value);

    qCInfo(LogApp) << "Setting changed: training word count ->" << GetTrainingWordCount();

    emit learningSettingsChanged();
}

int SettingsStore::GetReviewWordCount() const
{
    return FTSettings::Get().GetRepetitionRules().ReviewWordCount;
}

void SettingsStore::SetReviewWordCount(int Value)
{
    if (GetReviewWordCount() == Value)
    {
        return;
    }

    FTSettings::Get().SetReviewWordCount(Value);

    qCInfo(LogApp) << "Setting changed: review word count ->" << GetReviewWordCount();

    emit learningSettingsChanged();
}

namespace
{
    /** The distinct languages that can be learned, in the order the dictionaries were scanned. */
    std::vector<std::string> AvailableTargetLanguages()
    {
        std::vector<std::string> Result;

        for (const FTLanguageRules& Pair : FTSettings::GetAvailableLanguagePairs())
        {
            if (std::find(Result.begin(), Result.end(), Pair.TargetLanguage) == Result.end())
            {
                Result.push_back(Pair.TargetLanguage);
            }
        }

        return Result;
    }

    /** Which languages one target can be translated into, which is whatever its dictionary was
        built with. */
    std::vector<std::string> AvailableNativeLanguages(const std::string& TargetLanguage)
    {
        std::vector<std::string> Result;

        for (const FTLanguageRules& Pair : FTSettings::GetAvailableLanguagePairs())
        {
            if (Pair.TargetLanguage != TargetLanguage)
            {
                continue;
            }

            if (std::find(Result.begin(), Result.end(), Pair.NativeLanguage) == Result.end())
            {
                Result.push_back(Pair.NativeLanguage);
            }
        }

        return Result;
    }

    QStringList ToNameList(const std::vector<std::string>& Codes)
    {
        QStringList Result;

        for (const std::string& Code : Codes)
        {
            Result.append(LanguageDisplayName(Code));
        }

        return Result;
    }

    int IndexOfCode(const std::vector<std::string>& Codes, const std::string& Wanted)
    {
        const auto Found = std::find(Codes.begin(), Codes.end(), Wanted);

        if (Found == Codes.end())
        {
            return 0;
        }

        return static_cast<int>(std::distance(Codes.begin(), Found));
    }
}

QStringList SettingsStore::GetTargetLanguageNames() const
{
    return ToNameList(AvailableTargetLanguages());
}

int SettingsStore::GetTargetLanguageIndex() const
{
    return IndexOfCode(AvailableTargetLanguages(),
                       FTSettings::Get().GetLanguageRules().TargetLanguage);
}

void SettingsStore::SetTargetLanguageIndex(int Index)
{
    const std::vector<std::string> Targets = AvailableTargetLanguages();

    if (Index < 0 || Index >= static_cast<int>(Targets.size()))
    {
        return;
    }

    // The native language is carried over where the new target offers it. SetLanguagePair falls back
    // to one the new target does have, so switching to a dictionary built without the current native
    // language moves rather than refusing.
    ApplyLanguagePair(Targets[static_cast<size_t>(Index)],
                      FTSettings::Get().GetLanguageRules().NativeLanguage);
}

void SettingsStore::selectTargetLanguage(const QString& Code)
{
    ApplyLanguagePair(Code.toStdString(), FTSettings::Get().GetLanguageRules().NativeLanguage);
}

void SettingsStore::selectLanguagePair(const QString& TargetCode, const QString& NativeCode)
{
    const std::string Native = NativeCode.isEmpty()
        ? FTSettings::Get().GetLanguageRules().NativeLanguage
        : NativeCode.toStdString();

    ApplyLanguagePair(TargetCode.toStdString(), Native);
}

QStringList SettingsStore::GetNativeLanguageNames() const
{
    return ToNameList(
        AvailableNativeLanguages(FTSettings::Get().GetLanguageRules().TargetLanguage));
}

QStringList SettingsStore::GetNativeLanguageCodes() const
{
    QStringList Result;

    for (const std::string& Code : AvailableNativeLanguages(FTSettings::Get().GetLanguageRules().TargetLanguage))
    {
        Result.append(QString::fromStdString(Code));
    }

    return Result;
}

int SettingsStore::GetNativeLanguageIndex() const
{
    const FTLanguageRules& Active = FTSettings::Get().GetLanguageRules();

    return IndexOfCode(AvailableNativeLanguages(Active.TargetLanguage), Active.NativeLanguage);
}

void SettingsStore::SetNativeLanguageIndex(int Index)
{
    const FTLanguageRules& Active = FTSettings::Get().GetLanguageRules();
    const std::vector<std::string> Natives = AvailableNativeLanguages(Active.TargetLanguage);

    if (Index < 0 || Index >= static_cast<int>(Natives.size()))
    {
        return;
    }

    ApplyLanguagePair(Active.TargetLanguage, Natives[static_cast<size_t>(Index)]);
}

void SettingsStore::ApplyLanguagePair(const std::string& TargetLanguage,
                                  const std::string& NativeLanguage)
{
    const FTLanguageRules Before = FTSettings::Get().GetLanguageRules();

    if (!FTSettings::Get().SetLanguagePair(TargetLanguage, NativeLanguage))
    {
        AppDictionary::Get().Discover();
    }

    if (!FTSettings::Get().SetLanguagePair(TargetLanguage, NativeLanguage))
    {
        SetLastError(tr("No dictionary is installed for that language."));

        return;
    }

    const FTLanguageRules& After = FTSettings::Get().GetLanguageRules();

    if (After.TargetLanguage == Before.TargetLanguage
        && After.NativeLanguage == Before.NativeLanguage
        && AppDictionary::Get().IsOpenFor(After.TargetLanguage, After.NativeLanguage))
    {
        return;
    }

    // The dictionary first. A failure here leaves the previous one open, so the setting is put back
    // rather than leaving the app reading one language and claiming another.
    if (!AppDictionary::Get().Open(After.TargetLanguage, After.NativeLanguage))
    {
        qCWarning(LogApp) << "Could not open the dictionary for"
                          << QString::fromStdString(After.TargetLanguage) << "->"
                          << QString::fromStdString(After.NativeLanguage);

        FTSettings::Get().SetLanguagePair(Before.TargetLanguage, Before.NativeLanguage);
        SetLastError(tr("That dictionary could not be opened."));

        return;
    }

    AppDatabase::Get().ApplyActiveLanguages();

    qCInfo(LogApp) << "Setting changed: language pair ->"
                   << QString::fromStdString(After.TargetLanguage) << "->"
                   << QString::fromStdString(After.NativeLanguage);

    SetLastError({});

    emit languagesChanged();
    emit audioSettingsChanged();
}

int SettingsStore::GetDictionaryWordCount() const
{
    const FTDictionaryPackage* Active = AppDictionary::Get().GetActivePackage();

    return Active != nullptr ? Active->WordCount : 0;
}

QString SettingsStore::GetTargetLanguageName() const
{
    return LanguageDisplayName(FTSettings::Get().GetLanguageRules().TargetLanguage);
}

QString SettingsStore::GetNativeLanguageName() const
{
    return LanguageDisplayName(FTSettings::Get().GetLanguageRules().NativeLanguage);
}

QString SettingsStore::GetLanguagePairLabel() const
{
    // The codes rather than the names: this sits in a narrow rail, and "FR / EN" fits where
    // "French / English" does not. Upper-cased for the same reason the codes are stored lower.
    const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

    return QStringLiteral("%1 / %2")
        .arg(QString::fromStdString(Rules.TargetLanguage).toUpper(),
             QString::fromStdString(Rules.NativeLanguage).toUpper());
}

QString SettingsStore::GetTargetLanguageCode() const
{
    return QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage);
}

void SettingsStore::playSentenceVoiceSample()
{
    if (!IsAudioEnabled() || !IsAudioAvailable())
    {
        return;
    }

    bSampleIsSentence = true;
    SampleDelay.start();
}

void SettingsStore::playVoiceSample()
{
    if (!IsAudioEnabled() || !IsAudioAvailable())
    {
        return;
    }

    bSampleIsSentence = false;

    /*
     * Held briefly rather than played at once. Clicking through voices or dragging the speed slider
     * fires this on every step, and each one is a few hundred milliseconds of synthesis that nobody
     * will hear because the next click supersedes it. Waiting for the choice to settle renders once.
     */
    SampleDelay.start();
}

void SettingsStore::playVoiceSampleNow()
{
    // No delay: the press is the whole intent.
    SampleDelay.stop();

    bSampleIsSentence = false;

    PlaySampleNow();
}

void SettingsStore::PlaySampleNow()
{
    if (!IsAudioEnabled() || !IsAudioAvailable())
    {
        return;
    }

    // In the language being learned, since that is what the voice will actually be reading.
    const std::string& Target = FTSettings::Get().GetLanguageRules().TargetLanguage;
    const QString Line = VoiceSampleLine(Target);

    if (Line.isEmpty())
    {
        qCWarning(LogApp) << "No voice sample line for" << QString::fromStdString(Target);

        return;
    }

    const float RateScale = bSampleIsSentence ? AppSpeech::Get().GetSentenceSpeed() : 1.0f;

    AppSpeech::Get().SpeakAtRate(Line, QString::fromStdString(Target), RateScale);
}

int SettingsStore::GetExamplesPerSense() const
{
    return FTSettings::Get().GetDictionaryDisplayRules().ExamplesPerSense;
}

void SettingsStore::SetExamplesPerSense(int Value)
{
    if (GetExamplesPerSense() == Value)
    {
        return;
    }

    FTSettings::Get().SetExamplesPerSense(Value);

    qCInfo(LogApp) << "Setting changed: examples per sense ->" << GetExamplesPerSense();

    emit learningSettingsChanged();
}

QStringList SettingsStore::GetWebSearchSourceNames() const
{
    QStringList Names;

    for (const FTWebSearchSource& Source : AppWebSearch::GetSources())
    {
        Names.append(QString::fromStdString(Source.Name));
    }

    return Names;
}

int SettingsStore::GetWebSearchSourceIndex() const
{
    return AppWebSearch::GetActiveSourceIndex();
}

void SettingsStore::SetWebSearchSourceIndex(int Index)
{
    const std::vector<FTWebSearchSource> Sources{AppWebSearch::GetSources()};

    if (Index < 0 || Index >= static_cast<int>(Sources.size()) || Index == GetWebSearchSourceIndex())
    {
        return;
    }

    const FTWebSearchSource& Source{Sources[static_cast<std::size_t>(Index)]};
    FTSettings::Get().SetWebSearchSource(Source.Id);

    qCInfo(LogApp) << "Setting changed: web search source ->" << QString::fromStdString(Source.Id);

    emit webSearchChanged();
}

int SettingsStore::GetDebugDayOffset() const
{
#ifdef QT_DEBUG
    return ::GetDebugDayOffset();
#else
    return 0;
#endif
}

void SettingsStore::SetDebugDayOffset(int Days)
{
#ifdef QT_DEBUG
    if (::GetDebugDayOffset() == Days)
    {
        return;
    }

    ::SetDebugDayOffset(Days);

    qCInfo(LogApp) << "Debug day offset ->" << Days << "days, today is now"
                   << GetDebugTodayText();

    emit debugTimeChanged();
#else
    Q_UNUSED(Days);
#endif
}

QString SettingsStore::GetDebugTodayText() const
{
    // Built from the day number rather than the system clock, so it shows the shifted date the
    // schedule is actually using rather than the real one.
    const QDate Epoch(1970, 1, 1);

    return Epoch.addDays(GetTodayDayNumber()).toString(Qt::ISODate);
}

void SettingsStore::setIntervalDays(int Stage, int Days)
{
    QList<int> Ladder = FTSettings::Get().GetIntervalDays();

    if (Stage < FirstLadderStage || Stage > Ladder.size())
    {
        return;
    }

    if (Ladder[Stage - 1] == Days)
    {
        return;
    }

    Ladder[Stage - 1] = Days;
    FTSettings::Get().SetIntervalDays(Ladder);

    qCInfo(LogApp) << "Setting changed: interval for stage" << Stage << "->" << Days << "days";

    emit scheduleSettingsChanged();
}

void SettingsStore::addStage()
{
    QList<int> Ladder = FTSettings::Get().GetIntervalDays();

    if (Ladder.size() >= MaximumLadderStages)
    {
        return;
    }

    // Twice the last interval, which is what the default ladder does and what Leitner assumes.
    const int Last = Ladder.isEmpty() ? 1 : Ladder.last();
    Ladder.append(std::min(Last * 2, MaximumIntervalDays));

    FTSettings::Get().SetIntervalDays(Ladder);

    qCInfo(LogApp) << "Setting changed: added ladder stage" << Ladder.size() << "at"
                   << Ladder.last() << "days";

    emit scheduleSettingsChanged();
}

void SettingsStore::removeStage()
{
    QList<int> Ladder = FTSettings::Get().GetIntervalDays();

    if (Ladder.size() <= 1)
    {
        return;
    }

    const int RemovedStage = Ladder.size();

    Ladder.removeLast();
    FTSettings::Get().SetIntervalDays(Ladder);

    qCInfo(LogApp) << "Setting changed: removed ladder stage" << RemovedStage;

    // Cards above the new end are left alone. They complete on their next promotion and drop into
    // the new ladder on a demotion, rather than being rewritten the moment a setting changes.
    emit scheduleSettingsChanged();
}

void SettingsStore::resetScheduleToDefaults()
{
    const FTScheduleRules Defaults;

    FTSettings::Get().SetIntervalDays(QList<int>(Defaults.IntervalDays.begin(), Defaults.IntervalDays.end()));
    FTSettings::Get().SetMaximumDaysInFirstStage(Defaults.MaximumDaysInFirstStage);

    qCInfo(LogApp) << "Setting changed: schedule reset to defaults";

    emit scheduleSettingsChanged();
}

