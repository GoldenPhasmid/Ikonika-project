#pragma once

#include "AppSpeech.h"

#include <QAtomicInteger>

#include <deque>
#include <functional>
#include <string>

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

    bool CanNarrate() const override;
    void QueueNarration(int Id, const QString& Text, const QString& LanguageCode) override;
    void StopNarration() override;

protected:
    void SpeakNow(const FSpeechRequest& Request) override;

private:
    struct FNarrationRequest
    {
        int Id = 0;
        std::string Text;
        std::string LanguageCode;
    };

    struct FNarrationPlayback
    {
        int Id = 0;
        FTNarrationChunk Chunk;
        bool bStarted = false;
    };

    struct FNarrationRun
    {
        quint64 Generation = 0;
        int VoiceIndex = -1;
        bool bContinues = false;
        std::deque<FNarrationPlayback> Playing;
    };

    void RunNarration();
    std::optional<FNarrationRequest> TakeNarrationRequest(FNarrationRun& Run);
    void PlayNarrationRequest(const FNarrationRequest& Request, FNarrationRun& Run);
    void ReportNarrationProgress(FNarrationRun& Run);

    void EnsureLoaded();
    bool LoadVoices();
    bool IsServicePlaying();
    int FindSavedVoice() const;
    void ChangeOnWorker(std::function<void()> Apply);

    FTSpeechService Service;
    QMutex WorkerMutex;
    QMutex StateMutex;
    QAtomicInteger<quint64> PendingGeneration{0};

    QMutex NarrationMutex;
    std::deque<FNarrationRequest> NarrationRequests;
    QAtomicInteger<quint64> NarrationGeneration{0};
    bool bNarrationRunning = false;
};
