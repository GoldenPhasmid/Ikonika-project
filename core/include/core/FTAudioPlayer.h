#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class FTAudioPlayer
{
public:
    /** A rate of zero or less opens no device, which is the harmless unavailable state. */
    explicit FTAudioPlayer(int InSampleRate);

    ~FTAudioPlayer();

    FTAudioPlayer(const FTAudioPlayer&) = delete;
    FTAudioPlayer& operator=(const FTAudioPlayer&) = delete;
    FTAudioPlayer(FTAudioPlayer&&) = delete;
    FTAudioPlayer& operator=(FTAudioPlayer&&) = delete;

    bool IsAvailable() const;

    int GetSampleRate() const;

    bool Play(std::vector<int16_t> Samples, int SampleRate);

    bool Append(std::vector<int16_t> Samples, int SampleRate);

    void Stop();

    uint64_t GetQueuedFrames() const;
    uint64_t GetPlayedFrames() const;

    bool IsPlaying() const;

private:
    struct Impl;

    // Pimpl so miniaudio.h, which is four megabytes, stays out of every file that plays a sound.
    std::unique_ptr<Impl> Data;
};
