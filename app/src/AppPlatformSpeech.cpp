#include "AppPlatformSpeech.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QSettings>
#include <QThreadPool>
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>

#include <algorithm>

namespace
{
    constexpr const char* PlatformSpeechClass = "com/goldenphasmid/ikonika/PlatformSpeech";

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

    void NativeOnSpeakingChanged(JNIEnv*, jclass, jboolean Speaking)
    {
        AppSpeech::Get().ReportSpeaking(Speaking == JNI_TRUE);
    }

    void RegisterSpeakingBridge()
    {
        static const JNINativeMethod Methods[] = {
            { "nativeOnSpeakingChanged", "(Z)V",
              reinterpret_cast<void*>(&NativeOnSpeakingChanged) },
        };

        QJniEnvironment Env;

        if (!Env.registerNativeMethods(PlatformSpeechClass, Methods, 1))
        {
            qCWarning(LogSpeech) << "Could not register the speaking state bridge";
        }
    }
}

AppPlatformSpeech::AppPlatformSpeech()
{
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
