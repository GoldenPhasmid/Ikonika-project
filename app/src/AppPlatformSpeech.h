#pragma once

#include "AppSpeech.h"

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

protected:
    void SpeakNow(const FSpeechRequest& Request) override;

private:
    void ApplyStoredSettings();

    float Speed = 1.0f;
    bool bEnabled = true;
};
