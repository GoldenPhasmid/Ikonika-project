#pragma once

#include "core/FTSpeech.h"

#include <QAtomicInteger>
#include <QJSEngine>
#include <QMutex>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <optional>

struct FSpeechRequest
{
    QString Text;
    QString LanguageCode;
    float RateScale = 1.0f;
};

class AppSpeech : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool speaking READ IsSpeaking NOTIFY speakingChanged)

public:
    static AppSpeech& Get();
    static void SetInstance(AppSpeech* NewInstance);

    ~AppSpeech() override = default;

    virtual void Prewarm() {}
    virtual void Reload() {}
    virtual void Unload() {}

    void Speak(const QString& Text, const QString& LanguageCode)
    {
        SpeakAtRate(Text, LanguageCode, 1.0f);
    }

    virtual void SpeakAtRate(const QString& Text, const QString& LanguageCode, float RateScale);

    virtual void SetActiveVoice(int Index) { Q_UNUSED(Index); }
    virtual void SetSpeed(float Value) { Q_UNUSED(Value); }
    virtual void SetSpeakerId(int Value) { Q_UNUSED(Value); }
    virtual void SetEnabled(bool bValue) { Q_UNUSED(bValue); }
    virtual void SetCachedLineCount(int Value) { Q_UNUSED(Value); }

    virtual bool IsAvailable() const { return false; }
    virtual bool IsEnabled() const { return true; }
    virtual int GetVoiceCount() const { return 0; }
    virtual FTVoiceInfo GetVoiceInfo(int Index) const { Q_UNUSED(Index); return {}; }
    virtual int GetActiveVoice() const { return 0; }
    virtual int GetSpeakerId() const { return 0; }
    virtual float GetSpeed() const { return 1.0f; }

    virtual QMutex& Lock() { return DefaultLock; }

    bool IsSpeaking() const { return bSpeaking.loadRelaxed() != 0; }

    /** Safe from any thread: the change reaches listeners on this object's thread. */
    void ReportSpeaking(bool bValue);

    Q_INVOKABLE void holdSpeech();
    Q_INVOKABLE void releaseSpeech();
    bool IsSpeechHeld() const;

    void SilenceSpeech();
    void UnsilenceSpeech();
    bool IsSpeechSilenced() const;

    bool AllowsAutomaticSpeech() const;

signals:
    void ready();
    void speakingChanged(bool bSpeaking);
    void enabledChanged(bool bEnabled);
    void speechReleased();

protected:
    AppSpeech();

    static constexpr const char* SpeedKey = "audio/speed";
    static constexpr const char* EnabledKey = "audio/enabled";

    virtual void SpeakNow(const FSpeechRequest& Request) { Q_UNUSED(Request); }

private:
    void SpeakPending();

    std::optional<FSpeechRequest> PendingRequest;
    QMutex DefaultLock;
    QAtomicInteger<int> bSpeaking{0};
    int HoldCount = 0;
    int SilenceCount = 0;

    static AppSpeech* CurrentInstance;
};

class FScopedSpeechSilence
{
public:
    FScopedSpeechSilence()
    {
        AppSpeech::Get().SilenceSpeech();
    }

    ~FScopedSpeechSilence()
    {
        AppSpeech::Get().UnsilenceSpeech();
    }

    FScopedSpeechSilence(const FScopedSpeechSilence&) = delete;
    FScopedSpeechSilence& operator=(const FScopedSpeechSilence&) = delete;
};

struct AppSpeechSingleton
{
    Q_GADGET
    QML_FOREIGN(AppSpeech)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppSpeech)

public:
    static AppSpeech* create(QQmlEngine* Engine, QJSEngine*)
    {
        Q_UNUSED(Engine);

        AppSpeech& Instance = AppSpeech::Get();

        QJSEngine::setObjectOwnership(&Instance, QJSEngine::CppOwnership);

        return &Instance;
    }
};
