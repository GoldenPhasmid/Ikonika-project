#pragma once

#include "AppSpeech.h"

#include <memory>

class AppIosSpeech : public AppSpeech
{
    Q_OBJECT

public:
    AppIosSpeech();
    ~AppIosSpeech() override;

    void ReloadFromSettings() override;

    void SetSpeed(float Value) override;
    void SetEnabled(bool bValue) override;

    bool IsAvailable() const override;
    bool IsEnabled() const override;
    float GetSpeed() const override;

    bool CanNarrate() const override;
    void QueueNarration(int Id, const QString& Text, const QString& LanguageCode) override;
    void StopNarration() override;

protected:
    void SpeakNow(const FSpeechRequest& Request) override;

private:
    struct Impl;

    void ApplyStoredSettings();

    std::unique_ptr<Impl> Native;
    float Speed = 1.0f;
    bool bEnabled = true;
};
