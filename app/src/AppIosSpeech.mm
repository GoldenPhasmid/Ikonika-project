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
    QHash<QString, QString> VoiceIdentifiers;

    Impl()
    {
        Synthesizer = [[AVSpeechSynthesizer alloc] init];
        Delegate = [[IkonikaSpeechDelegate alloc] init];
        Synthesizer.delegate = Delegate;
    }

    ~Impl()
    {
        Synthesizer.delegate = nil;
        [Synthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
        [Synthesizer release];
        [Delegate release];
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

    void Speak(const QString& Text, const QString& LanguageCode, float Rate)
    {
        const QString Identifier = VoiceFor(LanguageCode);

        if (Identifier.isEmpty())
        {
            qCWarning(LogSpeech) << "No installed voice speaks" << LanguageCode;

            return;
        }

        AVSpeechUtterance* Utterance = [AVSpeechUtterance speechUtteranceWithString:Text.toNSString()];
        Utterance.voice = [AVSpeechSynthesisVoice voiceWithIdentifier:Identifier.toNSString()];
        Utterance.rate = std::clamp(AVSpeechUtteranceDefaultSpeechRate * Rate, AVSpeechUtteranceMinimumSpeechRate,
                                    AVSpeechUtteranceMaximumSpeechRate);

        [Synthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
        [Synthesizer speakUtterance:Utterance];
    }

    void Stop()
    {
        [Synthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
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
