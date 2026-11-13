#include "AppPlatformSpeech.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QSettings>
#include <QThreadPool>
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>

#include <algorithm>
#include <iterator>

namespace
{
    constexpr const char* PlatformSpeechClass = "com/goldenphasmid/ikonika/PlatformSpeech";
    constexpr const char* ReadAloudSessionClass = "com/goldenphasmid/ikonika/ReadAloudSession";

    void PlatformInitialize()
    {
        QJniObject Activity = QNativeInterface::QAndroidApplication::context();

        QJniObject::callStaticMethod<void>(PlatformSpeechClass, "initialize",
            "(Landroid/content/Context;)V", Activity.object<jobject>());
    }

    void PlatformSpeak(const QString& Text, const QString& Language, float RateScale)
    {
        QJniObject JniText = QJniObject::fromString(Text);
        QJniObject JniLanguage = QJniObject::fromString(Language);

        QJniObject::callStaticMethod<void>(PlatformSpeechClass, "speak",
            "(Ljava/lang/String;Ljava/lang/String;F)V",
            JniText.object<jstring>(), JniLanguage.object<jstring>(), RateScale);
    }

    void PlatformStop()
    {
        QJniObject::callStaticMethod<void>(PlatformSpeechClass, "stop", "()V");
    }

    void PlatformSetRate(float Rate)
    {
        QJniObject::callStaticMethod<void>(PlatformSpeechClass, "setRate", "(F)V", Rate);
    }

    void PlatformNarrate(int Id, const QString& Text, const QString& Language, float RateScale)
    {
        QJniObject JniText = QJniObject::fromString(Text);
        QJniObject JniLanguage = QJniObject::fromString(Language);

        QJniObject::callStaticMethod<void>(PlatformSpeechClass, "narrate",
            "(ILjava/lang/String;Ljava/lang/String;F)V",
            static_cast<jint>(Id), JniText.object<jstring>(), JniLanguage.object<jstring>(), RateScale);
    }

    void PlatformStopNarration()
    {
        QJniObject::callStaticMethod<void>(PlatformSpeechClass, "stopNarration", "()V");
    }

    void PlatformUpdateNarrationSession(const QString& Title, bool bStarted, bool bPlaying)
    {
        QJniObject Context = QNativeInterface::QAndroidApplication::context();
        QJniObject JniTitle = QJniObject::fromString(Title);

        QJniObject::callStaticMethod<void>(ReadAloudSessionClass, "update",
            "(Landroid/content/Context;Ljava/lang/String;ZZ)V",
            Context.object<jobject>(), JniTitle.object<jstring>(),
            static_cast<jboolean>(bStarted), static_cast<jboolean>(bPlaying));
    }

    void NativeOnPlayRequested(JNIEnv*, jclass)
    {
        AppSpeech::Get().ReportNarrationPlayRequested();
    }

    void NativeOnPauseRequested(JNIEnv*, jclass)
    {
        AppSpeech::Get().ReportNarrationPauseRequested();
    }

    void NativeOnStopRequested(JNIEnv*, jclass)
    {
        AppSpeech::Get().ReportNarrationStopRequested();
    }

    void NativeOnSpeakingChanged(JNIEnv*, jclass, jboolean Speaking)
    {
        AppSpeech::Get().ReportSpeaking(Speaking == JNI_TRUE);
    }

    void NativeOnNarrationStarted(JNIEnv*, jclass, jint Id)
    {
        AppSpeech::Get().ReportNarrationStarted(static_cast<int>(Id));
    }

    void NativeOnNarrationFinished(JNIEnv*, jclass, jint Id)
    {
        AppSpeech::Get().ReportNarrationFinished(static_cast<int>(Id));
    }

    void NativeOnNarrationFailed(JNIEnv*, jclass)
    {
        AppSpeech::Get().ReportNarrationFailed();
    }

    void NativeOnDiagnostic(JNIEnv*, jclass, jstring Message)
    {
        qCInfo(LogSpeech) << "engine:" << QJniObject(Message).toString();
    }

    void RegisterSpeakingBridge()
    {
        static const JNINativeMethod Methods[] = {
            { "nativeOnDiagnostic", "(Ljava/lang/String;)V",
              reinterpret_cast<void*>(&NativeOnDiagnostic) },
            { "nativeOnSpeakingChanged", "(Z)V",
              reinterpret_cast<void*>(&NativeOnSpeakingChanged) },
            { "nativeOnNarrationStarted", "(I)V",
              reinterpret_cast<void*>(&NativeOnNarrationStarted) },
            { "nativeOnNarrationFinished", "(I)V",
              reinterpret_cast<void*>(&NativeOnNarrationFinished) },
            { "nativeOnNarrationFailed", "()V",
              reinterpret_cast<void*>(&NativeOnNarrationFailed) },
        };

        QJniEnvironment Env;

        if (!Env.registerNativeMethods(PlatformSpeechClass, Methods, static_cast<int>(std::size(Methods))))
        {
            qCWarning(LogSpeech) << "Could not register the speaking state bridge";
        }

        static const JNINativeMethod SessionMethods[] = {
            { "nativeOnPlayRequested", "()V", reinterpret_cast<void*>(&NativeOnPlayRequested) },
            { "nativeOnPauseRequested", "()V", reinterpret_cast<void*>(&NativeOnPauseRequested) },
            { "nativeOnStopRequested", "()V", reinterpret_cast<void*>(&NativeOnStopRequested) },
        };

        if (!Env.registerNativeMethods(ReadAloudSessionClass, SessionMethods, static_cast<int>(std::size(SessionMethods))))
        {
            qCWarning(LogSpeech) << "Could not register the read aloud session bridge";
        }
    }
}

AppPlatformSpeech::AppPlatformSpeech()
{
    NarrationPool.setMaxThreadCount(1);

    RegisterSpeakingBridge();
    ApplyStoredSettings();
}

void AppPlatformSpeech::ApplyStoredSettings()
{
    const QSettings Settings(FTSettings::Get().OpenStore());

    Speed = std::clamp(Settings.value(SpeedKey, 1.0).toFloat(), MinimumSpeechSpeed, MaximumSpeechSpeed);
    bEnabled = Settings.value(EnabledKey, true).toBool();

    PlatformSetRate(Speed);
}

void AppPlatformSpeech::ReloadFromSettings()
{
    ApplyStoredSettings();

    qCInfo(LogSpeech) << "Speech settings reloaded for the open workspace, enabled =" << bEnabled;
}

void AppPlatformSpeech::Prewarm()
{
    PlatformInitialize();
}

void AppPlatformSpeech::SpeakNow(const FSpeechRequest& Request)
{
    if (!bEnabled)
    {
        return;
    }

    QThreadPool::globalInstance()->start([Request]()
    {
        PlatformSpeak(Request.Text, Request.LanguageCode, Request.RateScale);
    });
}

void AppPlatformSpeech::SetSpeed(float Value)
{
    QSettings Settings(FTSettings::Get().OpenStore());
    Settings.setValue(SpeedKey, Value);

    Speed = std::clamp(Value, MinimumSpeechSpeed, MaximumSpeechSpeed);

    PlatformSetRate(Speed);
}

void AppPlatformSpeech::SetEnabled(bool bValue)
{
    QSettings Stored(FTSettings::Get().OpenStore());
    Stored.setValue(EnabledKey, bValue);

    bEnabled = bValue;

    if (!bEnabled)
    {
        PlatformStop();
    }

    emit enabledChanged(bValue);
}

bool AppPlatformSpeech::IsAvailable() const
{
    return true;
}

bool AppPlatformSpeech::IsEnabled() const
{
    return bEnabled;
}

float AppPlatformSpeech::GetSpeed() const
{
    return Speed;
}

bool AppPlatformSpeech::CanNarrate() const
{
    return bEnabled;
}

void AppPlatformSpeech::QueueNarration(int Id, const QString& Text, const QString& LanguageCode)
{
    const float RateScale{GetSentenceSpeed()};

    NarrationPool.start([Id, Text, LanguageCode, RateScale]() { PlatformNarrate(Id, Text, LanguageCode, RateScale); });
}

void AppPlatformSpeech::StopNarration()
{
    NarrationPool.start([]() { PlatformStopNarration(); });
}

void AppPlatformSpeech::UpdateNarrationSession(const QString& Title, bool bStarted, bool bPlaying)
{
    NarrationPool.start([Title, bStarted, bPlaying]() { PlatformUpdateNarrationSession(Title, bStarted, bPlaying); });
}
