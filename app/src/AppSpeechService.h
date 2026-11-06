#pragma once

#include "AppSpeech.h"

#include <QAtomicInteger>

#include <functional>

class AppSpeechService : public AppSpeech
{
    Q_OBJECT

public:
    AppSpeechService();

    void ReloadFromSettings() override;

    void Prewarm() override;
    void Reload() override;
    void Unload() override;

    void SetActiveVoice(int Index) override;
    void SetSpeed(float Value) override;
    void SetSpeakerId(int Value) override;
    void SetEnabled(bool bValue) override;
    void SetCachedLineCount(int Value) override;

    bool IsAvailable() const override;
    bool IsEnabled() const override;
    int GetVoiceCount() const override;
    FTVoiceInfo GetVoiceInfo(int Index) const override;
    int GetActiveVoice() const override;
    int GetSpeakerId() const override;
    float GetSpeed() const override;

    QMutex& Lock() override;

protected:
    void SpeakNow(const FSpeechRequest& Request) override;

private:
    void EnsureLoaded();
    bool LoadVoices();
    bool IsServicePlaying();
    int FindSavedVoice() const;
    void ChangeOnWorker(std::function<void()> Apply);

    FTSpeechService Service;
    QMutex WorkerMutex;
    QMutex StateMutex;
    QAtomicInteger<quint64> PendingGeneration{0};
};
