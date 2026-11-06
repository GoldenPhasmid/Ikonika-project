#include "core/FTSettings.h"

#include "core/FTWebSearch.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <algorithm>

namespace
{
    constexpr const char* IntervalsKey = "schedule/intervalDays";
    constexpr const char* FirstStageDaysKey = "schedule/maximumDaysInFirstStage";
    constexpr const char* FirstStageToPoolKey = "schedule/firstStageFailureReturnsToPool";
    constexpr const char* CardsPerExerciseKey = "intensiveLearning/cardsPerExercise";
    constexpr const char* SessionsPerDayKey = "intensiveLearning/sessionsPerDay";
    constexpr const char* TrainingWordCountKey = "intensiveLearning/trainingWordCount";
    constexpr const char* ReviewWordCountKey = "repetition/reviewWordCount";
    constexpr const char* TargetLanguageKey = "languages/target";
    constexpr const char* NativeLanguageKey = "languages/native";
    constexpr const char* AllowMissingArticleKey = "answers/allowMissingArticle";
    constexpr const char* MissedReviewDemotesKey = "schedule/missedReviewDemotes";
    constexpr const char* ReminderEnabledKey = "reminder/enabled";
    constexpr const char* ReminderTimeKey = "reminder/timeOfDayMinutes";
    constexpr const char* ReminderMessageKey = "reminder/message";
    constexpr const char* LaunchAtStartupKey = "startup/launchAtStartup";
    constexpr const char* LegacyRequireExactAccentsKey = "answers/requireExactAccents";
    constexpr const char* ExamplesPerSenseKey = "dictionary/examplesPerSense";
    constexpr const char* WebSearchSourceKey = "dictionary/webSearchSource";
    constexpr const char* CachedLineCountKey = "speech/cachedLineCount";
    constexpr const char* SpeakArticleKey = "speech/speakArticle";
    constexpr const char* AutoSpeakKey = "speech/autoSpeak";
    constexpr const char* QuestionSideKey = "exercises/askWith";
    constexpr const char* LegacyQuestionSideKey = "exercises/questionSide";
    constexpr const char* ShowArticleKey = "exercises/showArticle";
    constexpr const char* OptionalPronounKey = "forms/optionalPronoun";
    constexpr const char* LegacySpeedMatchVoiceKey = "exercises/speedMatchVoicePrompt";
    constexpr const char* LegacyChoiceVoiceKey = "exercises/choiceVoicePrompt";
    constexpr const char* LegacySentenceTypingKey = "sessions/sentenceTyping";
    constexpr const char* LegacyIncludeTypeWordKey = "intensiveLearning/includeTargetToNativeDirection";
    constexpr const char* LegacyIncludeAudioExerciseKey = "intensiveLearning/includeAudioExercise";

    constexpr const char* FlipCountName = "flipKnownCount";
    constexpr const char* ChoiceStageName = "choiceStage";
    constexpr const char* ChoiceCountName = "choiceCount";
    constexpr const char* ChoiceSideName = "choiceSide";
    constexpr const char* ChoiceVoiceName = "choiceVoicePrompt";
    constexpr const char* RepeatsToMasterName = "repeatsToMaster";
    constexpr const char* IncludeTypeWordName = "includeTargetToNativeDirection";
    constexpr const char* IncludeGapExerciseName = "includeGapExercise";
    constexpr const char* IncludeAudioExerciseName = "includeAudioExercise";
    constexpr const char* RequireExactAccentsName = "requireExactAccents";
    constexpr const char* SentenceStageName = "sentenceStage";
    constexpr const char* SentenceTypingName = "sentenceTyping";
    constexpr const char* MatchStageName = "matchStage";
    constexpr const char* MatchCountName = "matchCount";
    constexpr const char* MatchVoiceName = "matchVoicePrompt";
    constexpr const char* DebugContinueForcedKey = "intensiveLearning/debugContinueForced";

    bool IsSyncedKey(const QString& Key)
    {
        for (const char* DeviceGroup : {"appearance/", "motion/", "reader/", "sound/", "audio/", "sentences/", "triage/"})
        {
            if (Key.startsWith(QLatin1String(DeviceGroup)))
            {
                return false;
            }
        }

        for (const char* DeviceKey : {LaunchAtStartupKey, CachedLineCountKey, DebugContinueForcedKey})
        {
            if (Key == QLatin1String(DeviceKey))
            {
                return false;
            }
        }

        return true;
    }
    
    QString ShippedDefaultsPath()
    {
#if !defined(IKONIKA_DESKTOP)
        return QStringLiteral(":/defaults.ini");
#else
        return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("defaults.ini"));
#endif
    }

    QSettings& ShippedDefaults()
    {
        static QSettings Instance(ShippedDefaultsPath(), QSettings::IniFormat);

        return Instance;
    }

    QVariant ShippedDefault(const QString& Key, const QVariant& Compiled)
    {
        QSettings& Defaults = ShippedDefaults();
        Defaults.sync();

        return Defaults.value(Key, Compiled);
    }

    QVariant ShippedDefault(const char* Key, const QVariant& Compiled)
    {
        return ShippedDefault(QString::fromLatin1(Key), Compiled);
    }

    int FromLegacySide(int Stored)
    {
        if (Stored == 1)
        {
            return SHOW_TRANSLATION;
        }

        if (Stored == 2)
        {
            return SHOW_RANDOM;
        }

        return Stored;
    }

    QString SessionKey(const std::string& Group, const char* Name)
    {
        return QString::fromStdString(Group) + QLatin1Char('/') + QLatin1String(Name);
    }

    QVariant ReadSessionValue(const QSettings& Settings, const std::string& Group, const char* Name,
                              const char* LegacyKey, const QVariant& Compiled)
    {
        const QString Key = SessionKey(Group, Name);

        if (Settings.contains(Key))
        {
            return Settings.value(Key);
        }

        if (LegacyKey != nullptr && Settings.contains(QLatin1String(LegacyKey)))
        {
            return Settings.value(QLatin1String(LegacyKey));
        }

        return ShippedDefault(Key, Compiled);
    }

    FTSessionExerciseRules ReadSessionExercises(const QSettings& Settings, const std::string& Group)
    {
        const bool bReview = Group == ReviewSessionGroup;

        FTSessionExerciseRules Result = MakeDefaultSessionExercises(Group);

        const auto Read = [&Settings, &Group](const char* Name, const char* LegacyKey, const QVariant& Compiled)
        {
            return ReadSessionValue(Settings, Group, Name, LegacyKey, Compiled);
        };

        Result.FlipCount = Read(FlipCountName, nullptr, Result.FlipCount).toInt();
        Result.bChoiceStage = Read(ChoiceStageName, nullptr, Result.bChoiceStage).toBool();
        Result.ChoiceCount =Read(ChoiceCountName, nullptr, Result.ChoiceCount).toInt();
        Result.bChoiceVoice = Read(ChoiceVoiceName, LegacyChoiceVoiceKey, Result.bChoiceVoice).toBool();
        Result.RepeatsToMaster = Read(RepeatsToMasterName, nullptr, Result.RepeatsToMaster).toInt();

        Result.bTypeWordExercise = Read(IncludeTypeWordName,
            bReview ? LegacyIncludeTypeWordKey : nullptr, Result.bTypeWordExercise).toBool();
        Result.bGapExercise = Read(IncludeGapExerciseName, nullptr, Result.bGapExercise).toBool();
        Result.bAudioExercise = Read(IncludeAudioExerciseName,
            bReview ? LegacyIncludeAudioExerciseKey : nullptr, Result.bAudioExercise).toBool();

        Result.bRequireExactAccents = Read(RequireExactAccentsName, LegacyRequireExactAccentsKey,
            ShippedDefault(LegacyRequireExactAccentsKey, Result.bRequireExactAccents)).toBool();

        Result.bSentenceStage = Read(SentenceStageName, nullptr, Result.bSentenceStage).toBool();
        Result.bSentenceTyping = Read(SentenceTypingName, LegacySentenceTypingKey, Result.bSentenceTyping).toBool();
        Result.bMatchStage = Read(MatchStageName, nullptr, Result.bMatchStage).toBool();
        Result.MatchCount = Read(MatchCountName, nullptr, Result.MatchCount).toInt();
        Result.bMatchVoice = Read(MatchVoiceName, LegacySpeedMatchVoiceKey, Result.bMatchVoice).toBool();

        const QString SideKey = SessionKey(Group, ChoiceSideName);

        if (Settings.contains(SideKey))
        {
            Result.ChoiceSide = Settings.value(SideKey).toInt();
        }
        else if (Settings.contains(QLatin1String(LegacyQuestionSideKey)))
        {
            Result.ChoiceSide = FromLegacySide(Settings.value(QLatin1String(LegacyQuestionSideKey)).toInt());
        }
        else
        {
            Result.ChoiceSide = ShippedDefault(SideKey, Result.ChoiceSide).toInt();
        }

        Sanitise(Result);

        return Result;
    }

    void WriteSessionExercises(const std::function<void(const QString&, const QVariant&)>& Store,
                               const std::string& Group, const FTSessionExerciseRules& Before,
                               const FTSessionExerciseRules& After)
    {
        const auto Write = [&Store, &Group](const char* Name, const QVariant& Old, const QVariant& New)
        {
            if (Old != New)
            {
                Store(SessionKey(Group, Name), New);
            }
        };

        Write(FlipCountName, Before.FlipCount, After.FlipCount);
        Write(ChoiceStageName, Before.bChoiceStage, After.bChoiceStage);
        Write(ChoiceCountName,Before.ChoiceCount, After.ChoiceCount);
        Write(ChoiceSideName, Before.ChoiceSide, After.ChoiceSide);
        Write(ChoiceVoiceName, Before.bChoiceVoice, After.bChoiceVoice);
        Write(RepeatsToMasterName, Before.RepeatsToMaster, After.RepeatsToMaster);
        Write(IncludeTypeWordName, Before.bTypeWordExercise, After.bTypeWordExercise);
        Write(IncludeGapExerciseName, Before.bGapExercise, After.bGapExercise);
        Write(IncludeAudioExerciseName, Before.bAudioExercise, After.bAudioExercise);
        Write(RequireExactAccentsName, Before.bRequireExactAccents, After.bRequireExactAccents);
        Write(SentenceStageName, Before.bSentenceStage, After.bSentenceStage);
        Write(SentenceTypingName, Before.bSentenceTyping, After.bSentenceTyping);
        Write(MatchStageName, Before.bMatchStage, After.bMatchStage);
        Write(MatchCountName, Before.MatchCount, After.MatchCount);
        Write(MatchVoiceName, Before.bMatchVoice, After.bMatchVoice);
    }

    QString ToStored(const std::vector<int>& Days)
    {
        QStringList Parts;

        for (const int Value : Days)
        {
            Parts.append(QString::number(Value));
        }

        return Parts.join(QLatin1Char(','));
    }

    std::vector<int> FromStored(const QString& Stored)
    {
        std::vector<int> Result;

        for (const QString& Part : Stored.split(QLatin1Char(','), Qt::SkipEmptyParts))
        {
            bool bParsed = false;
            const int Value = Part.trimmed().toInt(&bParsed);

            if (bParsed)
            {
                Result.push_back(Value);
            }
        }

        return Result;
    }

    FTScheduleRules ReadRules(const QSettings& Settings)
    {
        FTScheduleRules Result;

        const QString Stored = Settings.value(
            IntervalsKey,
            ShippedDefault(IntervalsKey, QString())).toString();

        if (!Stored.isEmpty())
        {
            Result.IntervalDays = FromStored(Stored);
        }

        Result.MaximumDaysInFirstStage =
            Settings.value(
                FirstStageDaysKey,
                ShippedDefault(FirstStageDaysKey, Result.MaximumDaysInFirstStage)).toInt();

        Result.bFirstStageFailureReturnsToPool =
            Settings.value(
                FirstStageToPoolKey,
                ShippedDefault(FirstStageToPoolKey,
                               Result.bFirstStageFailureReturnsToPool)).toBool();

        Result.bMissedReviewDemotes = Settings.value(
            MissedReviewDemotesKey, ShippedDefault(MissedReviewDemotesKey, Result.bMissedReviewDemotes)).toBool();

        Sanitise(Result);

        return Result;
    }

    FTIntensiveLearningRules ReadIntensiveLearningRules(const QSettings& Settings)
    {
        FTIntensiveLearningRules Result;

        Result.CardsPerExercise =
            Settings.value(
                CardsPerExerciseKey,
                ShippedDefault(CardsPerExerciseKey, Result.CardsPerExercise)).toInt();

        Result.SessionsPerDay = Settings.value(
            SessionsPerDayKey,
            ShippedDefault(SessionsPerDayKey, Result.SessionsPerDay)).toInt();

        Result.TrainingWordCount =
            Settings.value(
                TrainingWordCountKey,
                ShippedDefault(TrainingWordCountKey, Result.TrainingWordCount)).toInt();

        Sanitise(Result);

        return Result;
    }

    FTReminderRules ReadReminderRules(const QSettings& Settings)
    {
        FTReminderRules Result;

        Result.bEnabled = Settings.value(
            ReminderEnabledKey,
            ShippedDefault(ReminderEnabledKey, Result.bEnabled)).toBool();

        Result.TimeOfDayMinutes =
            Settings.value(
                ReminderTimeKey,
                ShippedDefault(ReminderTimeKey, Result.TimeOfDayMinutes)).toInt();

        Result.Message =
            Settings.value(
                ReminderMessageKey,
                ShippedDefault(ReminderMessageKey, QString())).toString().toStdString();

        // A time outside the day would never arrive, so a stored oddity falls back rather than
        // silently disabling the reminder.
        if (Result.TimeOfDayMinutes < 0 || Result.TimeOfDayMinutes >= 24 * 60)
        {
            Result.TimeOfDayMinutes = FTReminderRules{}.TimeOfDayMinutes;
        }

        return Result;
    }

    bool IsTargetInstalled(const std::string& TargetLanguage)
    {
        const std::vector<FTLanguageRules> Available = FTSettings::GetAvailableLanguagePairs();

        return std::any_of(Available.begin(), Available.end(),
                           [&TargetLanguage](const FTLanguageRules& Pair) { return Pair.TargetLanguage == TargetLanguage; });
    }

    FTLanguageRules ReadLanguageRules(const QSettings& Settings)
    {
        FTLanguageRules Result;

        Result.TargetLanguage =
            Settings.value(
                TargetLanguageKey,
                ShippedDefault(TargetLanguageKey, QString::fromStdString(Result.TargetLanguage)))
                .toString().toStdString();

        Result.NativeLanguage =
            Settings.value(
                NativeLanguageKey,
                ShippedDefault(NativeLanguageKey, QString::fromStdString(Result.NativeLanguage)))
                .toString().toStdString();

        if (Settings.contains(TargetLanguageKey) && !IsTargetInstalled(Result.TargetLanguage))
        {
            return Result;
        }

        Sanitise(Result);

        return Result;
    }

    /** Filled by whatever scanned for dictionary files. Not a setting: it describes what is on
        disk, so it is never read from or written to QSettings. */
    std::vector<FTLanguageRules>& AvailableLanguagePairs()
    {
        static std::vector<FTLanguageRules> Pairs;

        return Pairs;
    }

    FTDictionaryDisplayRules ReadDictionaryDisplayRules(const QSettings& Settings)
    {
        FTDictionaryDisplayRules Result;

        Result.ExamplesPerSense =
            Settings.value(
                ExamplesPerSenseKey,
                ShippedDefault(ExamplesPerSenseKey, Result.ExamplesPerSense)).toInt();

        Sanitise(Result);

        return Result;
    }

    FTSpeechRules ReadSpeechRules(const QSettings& Settings)
    {
        FTSpeechRules Result;

        Result.CachedLineCount =
            Settings.value(
                CachedLineCountKey,
                ShippedDefault(CachedLineCountKey, Result.CachedLineCount)).toInt();

        Result.bSpeakArticle =
            Settings.value(
                SpeakArticleKey,
                ShippedDefault(SpeakArticleKey, Result.bSpeakArticle)).toBool();

        Result.bAutoSpeak =
            Settings.value(
                AutoSpeakKey,
                ShippedDefault(AutoSpeakKey, Result.bAutoSpeak)).toBool();

        Sanitise(Result);

        return Result;
    }

    FTExerciseDisplayRules ReadExerciseDisplayRules(const QSettings& Settings)
    {
        FTExerciseDisplayRules Result;

        int Stored = ShippedDefault(QuestionSideKey, Result.QuestionSide).toInt();

        if (Settings.contains(QLatin1String(QuestionSideKey)))
        {
            Stored = Settings.value(QLatin1String(QuestionSideKey)).toInt();
        }
        else if (Settings.contains(QLatin1String(LegacyQuestionSideKey)))
        {
            Stored = FromLegacySide(Settings.value(QLatin1String(LegacyQuestionSideKey)).toInt());
        }

        if (Stored >= 0 && Stored < QuestionSideCount)
        {
            Result.QuestionSide = Stored;
        }

        Result.bShowArticle = Settings.value(
            ShowArticleKey, ShippedDefault(ShowArticleKey, Result.bShowArticle)).toBool();

        return Result;
    }
}

void Sanitise(FTScheduleRules& Rules)
{
    if (Rules.IntervalDays.empty())
    {
        Rules.IntervalDays = FTScheduleRules{}.IntervalDays;
    }

    if (Rules.IntervalDays.size() > static_cast<size_t>(10))
    {
        Rules.IntervalDays.resize(10);
    }

    for (int& Days : Rules.IntervalDays)
    {
        Days = std::clamp(Days, 1, 1000);
    }

    Rules.MaximumDaysInFirstStage = std::max(Rules.MaximumDaysInFirstStage, 1);
}

void Sanitise(FTIntensiveLearningRules& Rules)
{
    Rules.CardsPerExercise = std::clamp(Rules.CardsPerExercise, 1, 100);

    // Steps of 5, so the stepper in the settings screen always lands on a round number.
    Rules.TrainingWordCount = std::clamp(Rules.TrainingWordCount, 10, 25);
    Rules.TrainingWordCount -= Rules.TrainingWordCount % 5;

    Rules.SessionsPerDay = std::clamp(Rules.SessionsPerDay, 0, MaximumSessionsPerDay);
}

void Sanitise(FTSessionExerciseRules& Rules)
{
    Rules.FlipCount = std::clamp(Rules.FlipCount, 1, MaximumFlipCount);
    Rules.ChoiceCount = std::clamp(Rules.ChoiceCount, 1, MaximumStageRepeatCount);
    Rules.MatchCount = std::clamp(Rules.MatchCount, 1, MaximumStageRepeatCount);
    Rules.RepeatsToMaster = std::clamp(Rules.RepeatsToMaster, 1, 20);

    if (Rules.ChoiceSide < 0 || Rules.ChoiceSide >= QuestionSideCount)
    {
        Rules.ChoiceSide = SHOW_RANDOM;
    }

    if (!Rules.bTypeWordExercise && !Rules.bGapExercise && !Rules.bAudioExercise)
    {
        Rules.bAudioExercise = true;
    }
}

FTSessionExerciseRules MakeDefaultSessionExercises(const std::string& Group)
{
    FTSessionExerciseRules Result;

    if (Group == ReviewSessionGroup)
    {
        Result.bChoiceStage = false;
        Result.bSentenceStage = false;
    }

    return Result;
}

void Sanitise(FTRepetitionRules& Rules)
{
    // Steps of 5, so the stepper in the settings screen always lands on a round number.
    Rules.ReviewWordCount = std::clamp(Rules.ReviewWordCount, 10, 25);
    Rules.ReviewWordCount -= Rules.ReviewWordCount % 5;
}

void Sanitise(FTDictionaryDisplayRules& Rules)
{
    // The ceiling is a display limit, not a data one: a sense with sixty examples would push every
    // later sense off the screen.
    Rules.ExamplesPerSense = std::clamp(Rules.ExamplesPerSense, 0, 10);
}

void Sanitise(FTSpeechRules& Rules)
{
    Rules.CachedLineCount =
        std::clamp(Rules.CachedLineCount, MinimumCachedLineCount, MaximumCachedLineCount);
}

void Sanitise(FTLanguageRules& Rules)
{
    const std::vector<FTLanguageRules> Available = FTSettings::GetAvailableLanguagePairs();

    // Nothing has scanned for dictionaries yet, or none are installed. Either way there is no
    // correct pair to fall back to, and overwriting the user's own choice would lose it for good.
    if (Available.empty())
    {
        return;
    }

    for (const FTLanguageRules& Pair : Available)
    {
        if (Pair.TargetLanguage == Rules.TargetLanguage
            && Pair.NativeLanguage == Rules.NativeLanguage)
        {
            return;
        }
    }

    // The stored pair has no dictionary behind it, which happens when a package is removed.
    Rules = Available.front();
}

FTSettings::FTSettings(const QString& FilePath)
    : FilePath{FilePath}
{
    ReloadFromDisk();
}

FTSettings::FTSettings()
{
    // The same call the reload uses, so construction and reload cannot read different sets of
    // settings. They had drifted: the reload knew about two groups and construction about eight.
    ReloadFromDisk();
}

QVariant FTSettings::GetShippedDefault(const char* Key, const QVariant& Compiled)
{
    return ShippedDefault(Key, Compiled);
}

namespace
{
    FTSettings* ActiveSettings = nullptr;
}

FTSettings& FTSettings::Get()
{
    static FTSettings Instance;

    return ActiveSettings != nullptr ? *ActiveSettings : Instance;
}

void FTSettings::SetActive(FTSettings* Settings)
{
    ActiveSettings = Settings;
}

const FTScheduleRules& FTSettings::GetScheduleRules() const
{
    return Rules;
}

void FTSettings::SetIntervalDays(const QList<int>& Days)
{
    Rules.IntervalDays.assign(Days.begin(), Days.end());
    Sanitise(Rules);

    Write(QLatin1String(IntervalsKey), ToStored(Rules.IntervalDays));
}

void FTSettings::SetMaximumDaysInFirstStage(int Days)
{
    Rules.MaximumDaysInFirstStage = Days;
    Sanitise(Rules);

    Write(QLatin1String(FirstStageDaysKey), Rules.MaximumDaysInFirstStage);
}

void FTSettings::SetMissedReviewDemotes(bool bValue)
{
    Rules.bMissedReviewDemotes = bValue;

    Write(QLatin1String(MissedReviewDemotesKey), bValue);
}

void FTSettings::SetFirstStageFailureReturnsToPool(bool bValue)
{
    Rules.bFirstStageFailureReturnsToPool = bValue;

    Write(QLatin1String(FirstStageToPoolKey), bValue);
}

QList<int> FTSettings::GetIntervalDays() const
{
    return QList<int>(Rules.IntervalDays.begin(), Rules.IntervalDays.end());
}

const FTIntensiveLearningRules& FTSettings::GetIntensiveLearningRules() const
{
    return IntensiveRules;
}

void FTSettings::SetCardsPerExercise(int Value)
{
    IntensiveRules.CardsPerExercise = Value;
    Sanitise(IntensiveRules);

    Write(QLatin1String(CardsPerExerciseKey), IntensiveRules.CardsPerExercise);
}

void FTSettings::SetSessionsPerDay(int Value)
{
    IntensiveRules.SessionsPerDay = Value;
    Sanitise(IntensiveRules);

    Write(QLatin1String(SessionsPerDayKey), IntensiveRules.SessionsPerDay);
}

void FTSettings::SetTrainingWordCount(int Value)
{
    IntensiveRules.TrainingWordCount = Value;
    Sanitise(IntensiveRules);

    Write(QLatin1String(TrainingWordCountKey), IntensiveRules.TrainingWordCount);
}

const FTRepetitionRules& FTSettings::GetRepetitionRules() const
{
    return RepetitionRules;
}

void FTSettings::SetReviewWordCount(int Value)
{
    RepetitionRules.ReviewWordCount = Value;
    Sanitise(RepetitionRules);

    Write(QLatin1String(ReviewWordCountKey), RepetitionRules.ReviewWordCount);
}

const FTAnswerLeniencyRules& FTSettings::GetLeniencyRules() const
{
    return LeniencyRules;
}

void FTSettings::SetAllowMissingArticle(bool bValue)
{
    LeniencyRules.bAllowMissingArticle = bValue;

    Write(QLatin1String(AllowMissingArticleKey), bValue);
}

FTAnswerComparisonRules FTSettings::GetComparisonRules(const std::string& Group) const
{
    FTAnswerComparisonRules Result;

    // Case, whitespace and hyphens are always forgiven: none of them is part of knowing a word, and
    // a keyboard makes all three easy to get wrong. Only the two the user controls are settings.
    Result.bAllowMissingArticle = LeniencyRules.bAllowMissingArticle;
    Result.bRequireExactAccents = GetSessionExercises(Group).bRequireExactAccents;

    return Result;
}

const FTSessionExerciseRules& FTSettings::GetSessionExercises(const std::string& Group) const
{
    return Group == ReviewSessionGroup ? ReviewExercises : TrainingExercises;
}

void FTSettings::SetSessionExercises(const std::string& Group, const FTSessionExerciseRules& Value)
{
    FTSessionExerciseRules Wanted = Value;
    Sanitise(Wanted);

    FTSessionExerciseRules& Current = Group == ReviewSessionGroup ? ReviewExercises : TrainingExercises;

    WriteSessionExercises([this](const QString& Key, const QVariant& Value) { Write(Key, Value); }, Group,
                          Current, Wanted);

    Current = Wanted;
}

const FTReminderRules& FTSettings::GetReminderRules() const
{
    return ReminderRules;
}

void FTSettings::SetReminderEnabled(bool bValue)
{
    ReminderRules.bEnabled = bValue;

    Write(QLatin1String(ReminderEnabledKey), bValue);
}

bool FTSettings::GetLaunchAtStartup() const
{
    return bLaunchAtStartup;
}

void FTSettings::SetLaunchAtStartup(bool bValue)
{
    bLaunchAtStartup = bValue;

    Write(QLatin1String(LaunchAtStartupKey), bValue);
}

void FTSettings::SetReminderTimeMinutes(int Minutes)
{
    // Clamped rather than rejected: a time outside the day would simply never arrive.
    ReminderRules.TimeOfDayMinutes = std::clamp(Minutes, 0, (24 * 60) - 1);

    Write(QLatin1String(ReminderTimeKey), ReminderRules.TimeOfDayMinutes);
}

void FTSettings::SetReminderMessage(const std::string& Text)
{
    ReminderRules.Message = Text;

    Write(QLatin1String(ReminderMessageKey), QString::fromStdString(Text));
}

std::vector<FTLanguageRules> FTSettings::GetAvailableLanguagePairs()
{
    return AvailableLanguagePairs();
}

void FTSettings::SetAvailableLanguagePairs(std::vector<FTLanguageRules> Pairs)
{
    AvailableLanguagePairs() = std::move(Pairs);
}

void FTSettings::RevalidateLanguageRules()
{
    if (HasChosenLanguage() && !IsTargetInstalled(LanguageRules.TargetLanguage))
    {
        return;
    }

    FTLanguageRules Wanted = LanguageRules;
    Sanitise(Wanted);

    if (Wanted.TargetLanguage == LanguageRules.TargetLanguage
        && Wanted.NativeLanguage == LanguageRules.NativeLanguage)
    {
        return;
    }

    LanguageRules = Wanted;

    Write(QLatin1String(TargetLanguageKey), QString::fromStdString(LanguageRules.TargetLanguage));
    Write(QLatin1String(NativeLanguageKey), QString::fromStdString(LanguageRules.NativeLanguage));
}

bool FTSettings::HasChosenLanguage() const
{
    return OpenStore().contains(QLatin1String(TargetLanguageKey));
}

void FTSettings::AdoptLanguagePair(const FTLanguageRules& Pair)
{
    LanguageRules = Pair;

    Write(QLatin1String(TargetLanguageKey), QString::fromStdString(LanguageRules.TargetLanguage));
    Write(QLatin1String(NativeLanguageKey), QString::fromStdString(LanguageRules.NativeLanguage));
}

const FTDictionaryDisplayRules& FTSettings::GetDictionaryDisplayRules() const
{
    return DictionaryDisplayRules;
}

void FTSettings::SetExamplesPerSense(int Value)
{
    FTDictionaryDisplayRules Wanted = DictionaryDisplayRules;
    Wanted.ExamplesPerSense = Value;
    Sanitise(Wanted);

    if (Wanted.ExamplesPerSense == DictionaryDisplayRules.ExamplesPerSense)
    {
        return;
    }

    DictionaryDisplayRules = Wanted;

    Write(QLatin1String(ExamplesPerSenseKey), DictionaryDisplayRules.ExamplesPerSense);
}

const std::string& FTSettings::GetWebSearchSource() const
{
    return WebSearchSource;
}

void FTSettings::SetWebSearchSource(const std::string& Id)
{
    if (Id.empty() || Id == WebSearchSource)
    {
        return;
    }

    WebSearchSource = Id;

    Write(QLatin1String(WebSearchSourceKey), QString::fromStdString(WebSearchSource));
}

const FTSpeechRules& FTSettings::GetSpeechRules() const
{
    return SpeechRules;
}

void FTSettings::SetCachedLineCount(int Value)
{
    FTSpeechRules Wanted = SpeechRules;
    Wanted.CachedLineCount = Value;
    Sanitise(Wanted);

    if (Wanted.CachedLineCount == SpeechRules.CachedLineCount)
    {
        return;
    }

    SpeechRules = Wanted;

    Write(QLatin1String(CachedLineCountKey), SpeechRules.CachedLineCount);
}

void FTSettings::SetSpeakArticle(bool bValue)
{
    SpeechRules.bSpeakArticle = bValue;

    Write(QLatin1String(SpeakArticleKey), bValue);
}

void FTSettings::SetAutoSpeak(bool bValue)
{
    SpeechRules.bAutoSpeak = bValue;

    Write(QLatin1String(AutoSpeakKey), bValue);
}

const FTExerciseDisplayRules& FTSettings::GetExerciseDisplayRules() const
{
    return ExerciseDisplayRules;
}

void FTSettings::SetQuestionSide(int Side)
{
    if (ExerciseDisplayRules.QuestionSide == Side)
    {
        return;
    }

    ExerciseDisplayRules.QuestionSide = Side;

    Write(QLatin1String(QuestionSideKey), Side);
}

void FTSettings::SetShowArticle(bool bValue)
{
    ExerciseDisplayRules.bShowArticle = bValue;

    Write(QLatin1String(ShowArticleKey), bValue);
}

const FTFormsRules& FTSettings::GetFormsRules() const
{
    return FormsRules;
}

void FTSettings::SetOptionalPronoun(bool bValue)
{
    FormsRules.bOptionalPronoun = bValue;

    Write(QLatin1String(OptionalPronounKey), bValue);
}

const FTLanguageRules& FTSettings::GetLanguageRules() const
{
    return LanguageRules;
}

void FTSettings::SetTargetLanguage(const std::string& Code)
{
    FTLanguageRules Wanted = LanguageRules;
    Wanted.TargetLanguage = Code;
    Sanitise(Wanted);

    // Sanitise falls back to the shipped pair, so a code with no dictionary behind it never lands.
    if (Wanted.TargetLanguage != Code)
    {
        return;
    }

    LanguageRules = Wanted;

    Write(QLatin1String(TargetLanguageKey), QString::fromStdString(LanguageRules.TargetLanguage));
    Write(QLatin1String(NativeLanguageKey), QString::fromStdString(LanguageRules.NativeLanguage));
}

bool FTSettings::SetLanguagePair(const std::string& TargetCode, const std::string& NativeCode)
{
    const std::vector<FTLanguageRules> Available = GetAvailableLanguagePairs();
    
    if (Available.empty())
    {
        return false;
    }

    const FTLanguageRules* Fallback = nullptr;

    for (const FTLanguageRules& Pair : Available)
    {
        if (Pair.TargetLanguage != TargetCode)
        {
            continue;
        }

        if (Pair.NativeLanguage == NativeCode)
        {
            Fallback = &Pair;

            break;
        }
        
        if (Fallback == nullptr)
        {
            Fallback = &Pair;
        }
    }

    if (Fallback == nullptr)
    {
        return false;
    }

    if (Fallback->TargetLanguage == LanguageRules.TargetLanguage
        && Fallback->NativeLanguage == LanguageRules.NativeLanguage)
    {
        return true;
    }

    LanguageRules = *Fallback;

    Write(QLatin1String(TargetLanguageKey), QString::fromStdString(LanguageRules.TargetLanguage));
    Write(QLatin1String(NativeLanguageKey), QString::fromStdString(LanguageRules.NativeLanguage));

    return true;
}

void FTSettings::SetNativeLanguage(const std::string& Code)
{
    FTLanguageRules Wanted = LanguageRules;
    Wanted.NativeLanguage = Code;
    Sanitise(Wanted);

    if (Wanted.NativeLanguage != Code)
    {
        return;
    }

    LanguageRules = Wanted;

    Write(QLatin1String(TargetLanguageKey), QString::fromStdString(LanguageRules.TargetLanguage));
    Write(QLatin1String(NativeLanguageKey), QString::fromStdString(LanguageRules.NativeLanguage));
}

#ifdef QT_DEBUG
bool FTSettings::IsDebugContinueForced() const
{
    return bDebugContinueForced;
}

void FTSettings::SetDebugContinueForced(bool bValue)
{
    bDebugContinueForced = bValue;

    Write(QLatin1String(DebugContinueForcedKey), bValue);
}
#endif

void FTSettings::ReloadFromDisk()
{
    const QSettings Settings(OpenStore());

    Rules = ReadRules(Settings);
    IntensiveRules = ReadIntensiveLearningRules(Settings);
    LanguageRules = ReadLanguageRules(Settings);
    TrainingExercises = ReadSessionExercises(Settings, TrainingSessionGroup);
    LeniencyRules.bAllowMissingArticle = Settings.value(
        AllowMissingArticleKey, ShippedDefault(AllowMissingArticleKey, LeniencyRules.bAllowMissingArticle)).toBool();
    ReviewExercises = ReadSessionExercises(Settings, ReviewSessionGroup);
    ReminderRules = ReadReminderRules(Settings);

    bLaunchAtStartup = Settings.value(
        LaunchAtStartupKey,
        ShippedDefault(LaunchAtStartupKey, bLaunchAtStartup)).toBool();
    DictionaryDisplayRules = ReadDictionaryDisplayRules(Settings);
    WebSearchSource = Settings.value(
        WebSearchSourceKey,
        ShippedDefault(WebSearchSourceKey, QString::fromLatin1(DefaultWebSearchSourceId)))
        .toString().toStdString();
    SpeechRules = ReadSpeechRules(Settings);
    ExerciseDisplayRules = ReadExerciseDisplayRules(Settings);
    FormsRules.bOptionalPronoun = Settings.value(
        OptionalPronounKey, ShippedDefault(OptionalPronounKey, FormsRules.bOptionalPronoun)).toBool();

    RepetitionRules.ReviewWordCount =
        Settings.value(
            ReviewWordCountKey,
            ShippedDefault(ReviewWordCountKey, RepetitionRules.ReviewWordCount)).toInt();

    Sanitise(RepetitionRules);

#ifdef QT_DEBUG
    bDebugContinueForced = Settings.value(
        DebugContinueForcedKey,
        ShippedDefault(DebugContinueForcedKey, false)).toBool();
#endif
}

QSettings FTSettings::OpenStore() const
{
    if (FilePath.isEmpty())
    {
        return QSettings();
    }

    return QSettings(FilePath, QSettings::IniFormat);
}

std::string SerializeSettingValue(const QVariant& Value)
{
    const QJsonArray Wrapped{QJsonValue::fromVariant(Value)};

    return QJsonDocument(Wrapped).toJson(QJsonDocument::Compact).toStdString();
}

QVariant ParseSettingValue(const std::string& Text)
{
    const QJsonDocument Parsed{QJsonDocument::fromJson(QByteArray::fromStdString(Text))};

    if (!Parsed.isArray() || Parsed.array().isEmpty())
    {
        return {};
    }

    return Parsed.array().at(0).toVariant();
}

void FTSettings::SetChangeListener(ChangeListener Listener)
{
    OnChange = std::move(Listener);
}

std::vector<std::pair<QString, QVariant>> FTSettings::ReadSyncedValues() const
{
    const QSettings Settings(OpenStore());

    std::vector<std::pair<QString, QVariant>> Values;

    for (const QString& Key : Settings.allKeys())
    {
        if (IsSyncedKey(Key))
        {
            Values.emplace_back(Key, Settings.value(Key));
        }
    }

    return Values;
}

void FTSettings::ApplySyncedValues(const std::vector<std::pair<QString, QVariant>>& Values)
{
    {
        QSettings Settings(OpenStore());

        for (const auto& [Key, Value] : Values)
        {
            if (!IsSyncedKey(Key) || !Value.isValid())
            {
                continue;
            }

            if (Settings.value(Key).toString() != Value.toString())
            {
                Settings.setValue(Key, Value);
            }
        }
    }

    ReloadFromDisk();
}

void FTSettings::RestoreSyncedValues(const std::vector<std::pair<QString, QVariant>>& Values)
{
    {
        QSettings Settings(OpenStore());

        for (const QString& Key : Settings.allKeys())
        {
            if (IsSyncedKey(Key))
            {
                Settings.remove(Key);
            }
        }

        for (const auto& [Key, Value] : Values)
        {
            if (IsSyncedKey(Key) && Value.isValid())
            {
                Settings.setValue(Key, Value);
            }
        }
    }

    ReloadFromDisk();
}

void FTSettings::Write(const QString& Key, const QVariant& Value)
{
    {
        QSettings Settings(OpenStore());
        Settings.setValue(Key, Value);
    }

    if (OnChange && IsSyncedKey(Key))
    {
        OnChange(Key, Value);
    }
}
