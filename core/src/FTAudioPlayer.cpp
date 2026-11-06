#include "core/FTAudioPlayer.h"

#include "core/FTLog.h"

#include "miniaudio.h"

#include <algorithm>
#include <iterator>
#include <mutex>

struct FTAudioPlayer::Impl
{
    ma_context Context{};
    bool bContextReady = false;

    ma_device Device{};
    bool bDeviceReady = false;
    int SourceSampleRate = 0;
    int DeviceSampleRate = 0;

    ma_resampler Resampler{};
    bool bResamplerReady = false;

    // Guards everything below. Play runs on the interface thread, the callback on the audio
    // thread, and a Play that arrives mid-playback swaps the buffer out from under it.
    std::mutex Guard;

    std::vector<int16_t> Samples;
    size_t Position = 0;
    uint64_t ConsumedFrames = 0;

    ~Impl()
    {
        CloseDevice();

        if (bContextReady)
        {
            ma_context_uninit(&Context);
        }

        if (bResamplerReady)
        {
            ma_resampler_uninit(&Resampler, nullptr);
        }
    }

    bool OpenContext()
    {
        if (bContextReady)
        {
            return true;
        }

        ma_context_config Config = ma_context_config_init();
        Config.coreaudio.sessionCategory = ma_ios_session_category_none;

        if (ma_context_init(nullptr, 0, &Config, &Context) != MA_SUCCESS)
        {
            qCWarning(LogSpeech) << "audio backend failed to initialise";

            return false;
        }

        bContextReady = true;

        return true;
    }

    bool OpenDevice()
    {
        if (!OpenContext())
        {
            return false;
        }

        ma_device_config Config = ma_device_config_init(ma_device_type_playback);
        Config.playback.format = ma_format_s16;
        Config.playback.channels = 1;
        Config.dataCallback = &Impl::OnDeviceData;
        Config.notificationCallback = &Impl::OnDeviceNotification;
        Config.pUserData = this;

        if (ma_device_init(&Context, &Config, &Device) != MA_SUCCESS)
        {
            qCWarning(LogSpeech) << "audio device failed to open";

            return false;
        }

        if (ma_device_start(&Device) != MA_SUCCESS)
        {
            qCWarning(LogSpeech) << "audio device failed to start";
            ma_device_uninit(&Device);

            return false;
        }

        bDeviceReady = true;
        DeviceSampleRate = static_cast<int>(Device.playback.internalSampleRate);

        qCInfo(LogSpeech) << "audio device opened at" << DeviceSampleRate
                          << "Hz for" << SourceSampleRate << "Hz source audio";

        return true;
    }

    void CloseDevice()
    {
        if (!bDeviceReady)
        {
            return;
        }

        ma_device_uninit(&Device);
        bDeviceReady = false;
    }

    bool IsRunning() const
    {
        return bDeviceReady && ma_device_is_started(&Device);
    }

    bool EnsureRunning()
    {
        if (IsRunning())
        {
            return true;
        }

        if (SourceSampleRate <= 0)
        {
            return false;
        }

        if (bDeviceReady)
        {
            qCWarning(LogSpeech) << "audio device stopped on its own, reopening it";
        }

        CloseDevice();

        return OpenDevice();
    }

    std::vector<int16_t> Resample(const std::vector<int16_t>& Source)
    {
        if (DeviceSampleRate == SourceSampleRate)
        {
            return Source;
        }

        if (bResamplerReady)
        {
            ma_resampler_uninit(&Resampler, nullptr);
            bResamplerReady = false;
        }

        ma_resampler_config Config = ma_resampler_config_init(
            ma_format_s16, 1, static_cast<ma_uint32>(SourceSampleRate),
            static_cast<ma_uint32>(DeviceSampleRate), ma_resample_algorithm_linear);

        if (ma_resampler_init(&Config, nullptr, &Resampler) != MA_SUCCESS)
        {
            return {};
        }

        bResamplerReady = true;

        ma_uint64 FrameCountIn = Source.size();
        ma_uint64 FrameCountOut = 0;

        if (ma_resampler_get_expected_output_frame_count(&Resampler, FrameCountIn, &FrameCountOut)
            != MA_SUCCESS)
        {
            return {};
        }

        std::vector<int16_t> Output(static_cast<size_t>(FrameCountOut));

        if (ma_resampler_process_pcm_frames(&Resampler, Source.data(), &FrameCountIn,
                                            Output.data(), &FrameCountOut) != MA_SUCCESS)
        {
            return {};
        }

        Output.resize(static_cast<size_t>(FrameCountOut));

        return Output;
    }

    void FillFrames(void* Output, ma_uint32 FrameCount)
    {
        int16_t* Destination = static_cast<int16_t*>(Output);

        const std::lock_guard<std::mutex> Lock(Guard);

        const size_t Remaining = Samples.size() - std::min(Position, Samples.size());
        const size_t Copying = std::min(static_cast<size_t>(FrameCount), Remaining);

        if (Copying > 0)
        {
            std::copy_n(Samples.begin() + static_cast<ptrdiff_t>(Position), Copying, Destination);
            Position += Copying;
        }

        // Silence rather than stale data for whatever is left of the request.
        std::fill_n(Destination + Copying, static_cast<size_t>(FrameCount) - Copying, int16_t{0});
    }

    /** A static member rather than a free function, so it can see this private nested type. */
    static void OnDeviceData(ma_device* Device, void* Output, const void*, ma_uint32 FrameCount)
    {
        Impl* Data = static_cast<Impl*>(Device->pUserData);

        if (Data == nullptr)
        {
            return;
        }

        Data->FillFrames(Output, FrameCount);
    }

    static void OnDeviceNotification(const ma_device_notification* Notification)
    {
        static constexpr const char* TypeNames[] = {"started", "stopped", "rerouted", "interruption began",
                                                    "interruption ended", "unlocked"};

        const size_t Type = static_cast<size_t>(Notification->type);

        if (Type >= std::size(TypeNames))
        {
            return;
        }

        qCDebug(LogSpeech) << "audio device" << TypeNames[Type];
    }
};

FTAudioPlayer::FTAudioPlayer(int InSampleRate)
    : Data(std::make_unique<Impl>())
{
    if (InSampleRate <= 0)
    {
        return;
    }

    Data->SourceSampleRate = InSampleRate;
    Data->OpenDevice();
}

FTAudioPlayer::~FTAudioPlayer() = default;

bool FTAudioPlayer::IsAvailable() const
{
    return Data->bDeviceReady;
}

int FTAudioPlayer::GetSampleRate() const
{
    return Data->SourceSampleRate;
}

bool FTAudioPlayer::Play(std::vector<int16_t> Samples, int SampleRate)
{
    if (Samples.empty() || SampleRate != Data->SourceSampleRate)
    {
        return false;
    }

    if (!Data->EnsureRunning())
    {
        return false;
    }

    std::vector<int16_t> Converted = Data->Resample(Samples);

    if (Converted.empty())
    {
        return false;
    }

    {
        const std::lock_guard<std::mutex> Lock(Data->Guard);

        Data->Samples = std::move(Converted);
        Data->Position = 0;
        Data->ConsumedFrames = 0;
    }

    return true;
}

bool FTAudioPlayer::Append(std::vector<int16_t> Samples, int SampleRate)
{
    if (Samples.empty() || SampleRate != Data->SourceSampleRate)
    {
        return false;
    }

    if (!Data->EnsureRunning())
    {
        return false;
    }

    std::vector<int16_t> Converted{Data->Resample(Samples)};

    if (Converted.empty())
    {
        return false;
    }

    const std::lock_guard<std::mutex> Lock(Data->Guard);

    Data->Samples.erase(Data->Samples.begin(), Data->Samples.begin() + static_cast<ptrdiff_t>(Data->Position));
    Data->ConsumedFrames += Data->Position;
    Data->Position = 0;
    Data->Samples.insert(Data->Samples.end(), Converted.begin(), Converted.end());

    return true;
}

void FTAudioPlayer::Stop()
{
    const std::lock_guard<std::mutex> Lock(Data->Guard);

    Data->Samples.clear();
    Data->Position = 0;
    Data->ConsumedFrames = 0;
}

uint64_t FTAudioPlayer::GetQueuedFrames() const
{
    const std::lock_guard<std::mutex> Lock(Data->Guard);

    return Data->ConsumedFrames + Data->Samples.size();
}

uint64_t FTAudioPlayer::GetPlayedFrames() const
{
    const std::lock_guard<std::mutex> Lock(Data->Guard);

    return Data->ConsumedFrames + std::min(Data->Position, Data->Samples.size());
}

bool FTAudioPlayer::IsPlaying() const
{
    if (!Data->IsRunning())
    {
        return false;
    }

    const std::lock_guard<std::mutex> Lock(Data->Guard);

    return Data->Position < Data->Samples.size();
}
