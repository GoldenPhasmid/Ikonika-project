#include "AppIosSpeech.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QHash>
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

    QString FindVoiceIdentifier(const QString& LanguageCode)
    {
        const QString Preferred = PreferredRegions.value(LanguageCode);
        AVSpeechSynthesisVoice* Best = nil;
        int BestScore = -1;

        for (AVSpeechSynthesisVoice* Voice in [AVSpeechSynthesisVoice speechVoices])
        {
            const QString Language = QString::fromNSString(Voice.language);

            if (Language != LanguageCode && !Language.startsWith(LanguageCode + QLatin1Char('-')))
            {
                continue;
            }

            const int Score = static_cast<int>(Voice.quality) * 10 + (Language == Preferred ? 5 : 0);

            if (Score > BestScore)
            {
                Best = Voice;
                BestScore = Score;
            }
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

    QString VoiceFor(const QString& LanguageCode)
    {
        const auto Found = VoiceIdentifiers.constFind(LanguageCode);

        if (Found != VoiceIdentifiers.constEnd())
        {
            return Found.value();
        }

        const QString Identifier = FindVoiceIdentifier(LanguageCode);
        VoiceIdentifiers.insert(LanguageCode, Identifier);

        return Identifier;
    }

    AVSpeechUtterance* MakeUtterance(const QString& Text, const QString& LanguageCode, float Rate)
    {
        const QString Identifier = VoiceFor(LanguageCode);

        if (Identifier.isEmpty())
        {
            qCWarning(LogSpeech) << "No installed voice speaks" << LanguageCode;

            return nil;
        }

        AVSpeechUtterance* Utterance = [AVSpeechUtterance speechUtteranceWithString:Text.toNSString()];
        Utterance.voice = [AVSpeechSynthesisVoice voiceWithIdentifier:Identifier.toNSString()];
        Utterance.rate = std::clamp(AVSpeechUtteranceDefaultSpeechRate * Rate, AVSpeechUtteranceMinimumSpeechRate,
                                    AVSpeechUtteranceMaximumSpeechRate);

        return Utterance;
    }

    void Speak(const QString& Text, const QString& LanguageCode, float Rate)
    {
        AVSpeechUtterance* Utterance = MakeUtterance(Text, LanguageCode, Rate);

        if (Utterance == nil)
        {
            return;
        }

        [Synthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
        [Synthesizer speakUtterance:Utterance];
    }

    bool Narrate(int Id, const QString& Text, const QString& LanguageCode, float Rate)
    {
        AVSpeechUtterance* Utterance = MakeUtterance(Text, LanguageCode, Rate);

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

    Native->Speak(Request.Text, Request.LanguageCode, Speed * Request.RateScale);
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

bool AppIosSpeech::CanNarrate() const
{
    return bEnabled;
}

void AppIosSpeech::QueueNarration(int Id, const QString& Text, const QString& LanguageCode)
{
    if (!Native->Narrate(Id, Text, LanguageCode, Speed))
    {
        ReportNarrationFailed();
    }
}

void AppIosSpeech::StopNarration()
{
    Native->StopNarration();
}
