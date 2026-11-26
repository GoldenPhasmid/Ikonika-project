#include "AppIosSpeech.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QHash>
#include <QLocale>
#include <QSettings>

#include <algorithm>

#import <AVFoundation/AVFoundation.h>

@interface IkonikaSpeechDelegate : NSObject <AVSpeechSynthesizerDelegate>
@end

@implementation IkonikaSpeechDelegate

- (void)speechSynthesizer:(AVSpeechSynthesizer*)Synthesizer didStartSpeechUtterance:(AVSpeechUtterance*)Utterance
{
    AppSpeech::Get().ReportSpeaking(true);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)Synthesizer didFinishSpeechUtterance:(AVSpeechUtterance*)Utterance
{
    AppSpeech::Get().ReportSpeaking(false);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)Synthesizer didCancelSpeechUtterance:(AVSpeechUtterance*)Utterance
{
    AppSpeech::Get().ReportSpeaking(false);
}

@end

@interface IkonikaNarrationDelegate : NSObject <AVSpeechSynthesizerDelegate>
{
    NSMutableDictionary<NSValue*, NSNumber*>* Ids;
}
- (void)track:(AVSpeechUtterance*)Utterance as:(int)Id;
- (void)forgetAll;
@end

@implementation IkonikaNarrationDelegate

- (instancetype)init
{
    self = [super init];

    if (self != nil)
    {
        Ids = [[NSMutableDictionary alloc] init];
    }

    return self;
}

- (void)dealloc
{
    [Ids release];
    [super dealloc];
}

- (void)track:(AVSpeechUtterance*)Utterance as:(int)Id
{
    Ids[[NSValue valueWithNonretainedObject:Utterance]] = @(Id);
}

- (void)forgetAll
{
    [Ids removeAllObjects];
}

- (NSNumber*)take:(AVSpeechUtterance*)Utterance
{
    NSValue* Key = [NSValue valueWithNonretainedObject:Utterance];
    NSNumber* Id = [[Ids[Key] retain] autorelease];

    [Ids removeObjectForKey:Key];

    return Id;
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)Synthesizer didStartSpeechUtterance:(AVSpeechUtterance*)Utterance
{
    NSNumber* Id = Ids[[NSValue valueWithNonretainedObject:Utterance]];

    if (Id != nil)
    {
        AppSpeech::Get().ReportNarrationStarted(Id.intValue);
    }
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)Synthesizer didFinishSpeechUtterance:(AVSpeechUtterance*)Utterance
{
    NSNumber* Id = [self take:Utterance];

    if (Id != nil)
    {
        AppSpeech::Get().ReportNarrationFinished(Id.intValue);
    }
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)Synthesizer didCancelSpeechUtterance:(AVSpeechUtterance*)Utterance
{
    [self take:Utterance];
}

@end

namespace
{
    const QHash<QString, QString> PreferredRegions{
        {QStringLiteral("de"), QStringLiteral("de-DE")},
        {QStringLiteral("en"), QStringLiteral("en-GB")},
        {QStringLiteral("es"), QStringLiteral("es-ES")},
        {QStringLiteral("fr"), QStringLiteral("fr-FR")},
        {QStringLiteral("it"), QStringLiteral("it-IT")},
        {QStringLiteral("pt"), QStringLiteral("pt-PT")},
        {QStringLiteral("uk"), QStringLiteral("uk-UA")},
    };

    void ConfigureAudioSession()
    {
        AVAudioSession* Session = [AVAudioSession sharedInstance];
        NSError* Error = nil;

        if (![Session setCategory:AVAudioSessionCategoryPlayback
                             mode:AVAudioSessionModeSpokenAudio
                          options:AVAudioSessionCategoryOptionMixWithOthers
                            error:&Error])
        {
            qCWarning(LogSpeech) << "Could not set the audio session category:"
                                 << QString::fromNSString(Error.localizedDescription);

            return;
        }

        if (![Session setActive:YES error:&Error])
        {
            qCWarning(LogSpeech) << "Could not activate the audio session:"
                                 << QString::fromNSString(Error.localizedDescription);

            return;
        }

        qCInfo(LogSpeech) << "Audio session set to playback of spoken audio";
    }

    bool IsCharacterVoice(AVSpeechSynthesisVoice* Voice)
    {
        const QString Identifier = QString::fromNSString(Voice.identifier);

        if (Identifier.contains(QLatin1String(".eloquence."))
            || Identifier.startsWith(QLatin1String("com.apple.speech.synthesis.voice.")))
        {
            return true;
        }

        if (@available(iOS 17.0, *))
        {
            return (Voice.voiceTraits & (AVSpeechSynthesisVoiceTraitIsNoveltyVoice
                                         | AVSpeechSynthesisVoiceTraitIsPersonalVoice)) != 0;
        }

        return false;
    }

    bool IsVoiceForLanguage(AVSpeechSynthesisVoice* Voice, const QString& LanguageCode)
    {
        const QString Language{QString::fromNSString(Voice.language)};

        return !LanguageCode.isEmpty()
            && (Language == LanguageCode || Language.startsWith(LanguageCode + QLatin1Char('-')))
            && !IsCharacterVoice(Voice);
    }

    QString FindVoiceIdentifier(const QString& LanguageCode)
    {
        const QString Preferred = PreferredRegions.value(LanguageCode);
        AVSpeechSynthesisVoice* SystemDefault =
            [AVSpeechSynthesisVoice voiceWithLanguage:(Preferred.isEmpty() ? LanguageCode : Preferred).toNSString()];
        AVSpeechSynthesisVoice* Best = nil;
        int BestScore = -1;

        for (AVSpeechSynthesisVoice* Voice in [AVSpeechSynthesisVoice speechVoices])
        {
            if (!IsVoiceForLanguage(Voice, LanguageCode))
            {
                continue;
            }

            const QString Language{QString::fromNSString(Voice.language)};
            const bool bSystemDefault = SystemDefault != nil && [Voice.identifier isEqualToString:SystemDefault.identifier];
            const int Score = static_cast<int>(Voice.quality) * 10 + (Language == Preferred ? 5 : 0)
                + (bSystemDefault ? 2 : 0);

            if (Score > BestScore)
            {
                Best = Voice;
                BestScore = Score;
            }
        }

        if (Best == nil)
        {
            Best = SystemDefault;
        }

        if (Best == nil)
        {
            return QString();
        }

        qCInfo(LogSpeech) << "Voice for" << LanguageCode << "is" << QString::fromNSString(Best.name)
                          << QString::fromNSString(Best.language) << "quality" << static_cast<int>(Best.quality);

        return QString::fromNSString(Best.identifier);
    }
}

struct AppIosSpeech::Impl
{
    AVSpeechSynthesizer* Synthesizer = nil;
    IkonikaSpeechDelegate* Delegate = nil;
    AVSpeechSynthesizer* Narrator = nil;
    IkonikaNarrationDelegate* NarrationDelegate = nil;
    QHash<QString, QString> VoiceIdentifiers;

    Impl()
    {
        Synthesizer = [[AVSpeechSynthesizer alloc] init];
        Delegate = [[IkonikaSpeechDelegate alloc] init];
        Synthesizer.delegate = Delegate;

        Narrator = [[AVSpeechSynthesizer alloc] init];
        NarrationDelegate = [[IkonikaNarrationDelegate alloc] init];
        Narrator.delegate = NarrationDelegate;
    }

    ~Impl()
    {
        Synthesizer.delegate = nil;
        [Synthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
        [Synthesizer release];
        [Delegate release];

        Narrator.delegate = nil;
        [Narrator stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
        [Narrator release];
        [NarrationDelegate release];
    }

    AVSpeechSynthesisVoice* DefaultVoiceFor(const QString& LanguageCode)
    {
        const auto Found = VoiceIdentifiers.constFind(LanguageCode);

        if (Found != VoiceIdentifiers.constEnd())
        {
            AVSpeechSynthesisVoice* Voice = [AVSpeechSynthesisVoice voiceWithIdentifier:Found.value().toNSString()];

            if (Voice != nil)
            {
                return Voice;
            }
        }

        const QString Identifier = FindVoiceIdentifier(LanguageCode);

        if (Identifier.isEmpty())
        {
            return nil;
        }

        VoiceIdentifiers.insert(LanguageCode, Identifier);

        return [AVSpeechSynthesisVoice voiceWithIdentifier:Identifier.toNSString()];
    }

    AVSpeechUtterance* MakeUtterance(const QString& Text, const QString& LanguageCode, float Rate, const QString& VoiceId)
    {
        AVSpeechSynthesisVoice* Voice = VoiceId.isEmpty() ? nil : [AVSpeechSynthesisVoice voiceWithIdentifier:VoiceId.toNSString()];

        if (Voice == nil || !IsVoiceForLanguage(Voice, LanguageCode))
        {
            Voice = DefaultVoiceFor(LanguageCode);
        }

        if (Voice == nil)
        {
            qCWarning(LogSpeech) << "No installed voice speaks" << LanguageCode;

            return nil;
        }

        AVSpeechUtterance* Utterance = [AVSpeechUtterance speechUtteranceWithString:Text.toNSString()];
        Utterance.voice = Voice;
        Utterance.rate = std::clamp(AVSpeechUtteranceDefaultSpeechRate * Rate, AVSpeechUtteranceMinimumSpeechRate,
                                    AVSpeechUtteranceMaximumSpeechRate);

        return Utterance;
    }

    void Speak(const QString& Text, const QString& LanguageCode, float Rate, const QString& VoiceId)
    {
        AVSpeechUtterance* Utterance = MakeUtterance(Text, LanguageCode, Rate, VoiceId);

        if (Utterance == nil)
        {
            return;
        }

        [Synthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
        [Synthesizer speakUtterance:Utterance];
    }

    bool Narrate(int Id, const QString& Text, const QString& LanguageCode, float Rate, const QString& VoiceId)
    {
        AVSpeechUtterance* Utterance = MakeUtterance(Text, LanguageCode, Rate, VoiceId);

        if (Utterance == nil)
        {
            return false;
        }

        [NarrationDelegate track:Utterance as:Id];
        [Narrator speakUtterance:Utterance];

        return true;
    }

    void Stop()
    {
        [Synthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    }

    void StopNarration()
    {
        [Narrator stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
        [NarrationDelegate forgetAll];
    }

    bool PauseNarration()
    {
        return [Narrator pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    }

    bool ResumeNarration()
    {
        return Narrator.paused && [Narrator continueSpeaking];
    }
};

AppIosSpeech::AppIosSpeech()
    : Native{std::make_unique<Impl>()}
{
    ConfigureAudioSession();
    ApplyStoredSettings();
}

AppIosSpeech::~AppIosSpeech() = default;

void AppIosSpeech::ApplyStoredSettings()
{
    const QSettings Settings(FTSettings::Get().OpenStore());

    Speed = std::clamp(Settings.value(SpeedKey, 1.0).toFloat(), MinimumSpeechSpeed, MaximumSpeechSpeed);
    bEnabled = Settings.value(EnabledKey, true).toBool();
}

void AppIosSpeech::ReloadFromSettings()
{
    ApplyStoredSettings();

    qCInfo(LogSpeech) << "Speech settings reloaded for the open workspace, enabled =" << bEnabled;
}

void AppIosSpeech::SpeakNow(const FSpeechRequest& Request)
{
    if (!bEnabled)
    {
        return;
    }

    Native->Speak(Request.Text, Request.LanguageCode, Speed * Request.RateScale, GetPlatformVoiceId(Request.LanguageCode));
}

void AppIosSpeech::SetSpeed(float Value)
{
    QSettings Settings(FTSettings::Get().OpenStore());
    Settings.setValue(SpeedKey, Value);

    Speed = std::clamp(Value, MinimumSpeechSpeed, MaximumSpeechSpeed);
}

void AppIosSpeech::SetEnabled(bool bValue)
{
    QSettings Stored(FTSettings::Get().OpenStore());
    Stored.setValue(EnabledKey, bValue);

    bEnabled = bValue;

    if (!bEnabled)
    {
        Native->Stop();
        Native->StopNarration();
    }

    emit enabledChanged(bValue);
}

bool AppIosSpeech::IsAvailable() const
{
    return true;
}

bool AppIosSpeech::IsEnabled() const
{
    return bEnabled;
}

float AppIosSpeech::GetSpeed() const
{
    return Speed;
}

QList<FTPlatformVoice> AppIosSpeech::GetPlatformVoices(const QString& LanguageCode) const
{
    QList<FTPlatformVoice> Voices;

    for (AVSpeechSynthesisVoice* Voice in [AVSpeechSynthesisVoice speechVoices])
    {
        if (!IsVoiceForLanguage(Voice, LanguageCode))
        {
            continue;
        }

        QString Name{QString::fromNSString(Voice.name)};

        if (Voice.quality == AVSpeechSynthesisVoiceQualityEnhanced)
        {
            Name = tr("%1 (Enhanced)").arg(Name);
        }
        else if (Voice.quality == AVSpeechSynthesisVoiceQualityPremium)
        {
            Name = tr("%1 (Premium)").arg(Name);
        }

        const QLocale Locale{QString::fromNSString(Voice.language)};

        if (!Locale.nativeTerritoryName().isEmpty())
        {
            Name = QStringLiteral("%1 · %2").arg(Name, Locale.nativeTerritoryName());
        }

        Voices.append(FTPlatformVoice{QString::fromNSString(Voice.identifier), Name});
    }

    std::sort(Voices.begin(), Voices.end(), [](const FTPlatformVoice& Left, const FTPlatformVoice& Right)
    {
        return Left.Name.localeAwareCompare(Right.Name) < 0;
    });

    return Voices;
}

bool AppIosSpeech::CanNarrate() const
{
    return bEnabled;
}

void AppIosSpeech::QueueNarration(int Id, const QString& Text, const QString& LanguageCode)
{
    if (!Native->Narrate(Id, Text, LanguageCode, Speed * GetSentenceSpeed(), GetPlatformVoiceId(LanguageCode)))
    {
        ReportNarrationFailed();
    }
}

void AppIosSpeech::StopNarration()
{
    Native->StopNarration();
}

bool AppIosSpeech::PauseNarration()
{
    return Native->PauseNarration();
}

bool AppIosSpeech::ResumeNarration()
{
    return Native->ResumeNarration();
}
