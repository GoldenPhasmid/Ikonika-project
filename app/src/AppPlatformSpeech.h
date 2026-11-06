#pragma once

#include "AppSpeech.h"

#include <QThreadPool>

class AppPlatformSpeech : public AppSpeech
{
    Q_OBJECT

public:
    AppPlatformSpeech();

    void ReloadFromSettings() override;

    void Prewarm() override;

    void SetSpeed(float Value) override;
    void SetEnabled(bool bValue) override;

    bool IsAvailable() const override;
    bool IsEnabled() const override;
    float GetSpeed() const override;

    bool CanNarrate() const override;
    void QueueNarration(int Id, const QString& Text, const QString& LanguageCode) override;
    void StopNarration() override;
    void UpdateNarrationSession(const QString& Title, bool bStarted, bool bPlaying) override;

protected:
    void SpeakNow(const FSpeechRequest& Request) override;

private:
    void ApplyStoredSettings();

    QThreadPool NarrationPool;

    float Speed = 1.0f;
    bool bEnabled = true;
};
