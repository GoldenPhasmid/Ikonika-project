#pragma once

#include "core/FTAudioPlayer.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/** Speed multiplier bounds. Below one is slower, which is what a learner usually wants. */
inline constexpr float MinimumSpeechSpeed = 0.5f;
inline constexpr float MaximumSpeechSpeed = 1.5f;

/** Default cache ceiling per voice, in entries. FTSpeechService::SetMaximumCacheEntries changes
    it at runtime. */
inline constexpr int DefaultMaximumCacheEntries = 50;

inline constexpr const char* PiperEngine = "Piper";
inline constexpr const char* SupertonicEngine = "Supertonic";

/** What the interface needs to describe a voice without knowing anything about models. */
struct FTVoiceInfo
{
    /** The model family that produced this voice, as recognised when the directory was loaded.
        A label rather than an enumeration, so a new family needs no code change outside the loader. */
    std::string Engine;
    std::string Name;
    int SampleRate = 0;
    int SpeakerCount = 0;

    /**
     * ISO 639-1 codes this voice can pronounce. Piper is built for one language and cannot be told
     * otherwise; Supertonic takes a language per call.
     *
     * Written down rather than asked of the model, because neither engine reports it. Getting this
     * list wrong produces confident nonsense instead of an error, which is why it is not guessed.
     */
    std::vector<std::string> Languages;

    /** True for the voice that handles a language the chosen one cannot. Exactly one voice is the
        fallback, and it is whichever covers the most languages. */
    bool bFallback = false;
};

/** Whether this voice can pronounce the given ISO 639-1 code. */
inline bool IsLanguageSupported(const FTVoiceInfo& Voice, const std::string& LanguageCode)
{
    return std::find(Voice.Languages.begin(), Voice.Languages.end(), LanguageCode)
        != Voice.Languages.end();
}

inline constexpr float TargetSpeechRms = 0.075f;

float ComputeSpeechGain(std::span<const float> Samples);

inline constexpr float LeadingSilenceThreshold = 0.005f;
inline constexpr float LeadingSilenceMarginSeconds = 0.02f;

size_t FindSpeechStart(std::span<const float> Samples, int SampleRate);

std::vector<float> StretchSpeech(std::span<const float> Samples, int SampleRate, float Tempo);

/** Whether the directory holds the files of a voice model this build can load. */
bool IsVoiceModelDirectory(const std::string& ModelDirectory);

class FTSpeechSynthesizer
{
public:
    static std::optional<FTSpeechSynthesizer> Create(const std::string& ModelDirectory,
                                                   const std::string& LanguageCode = "fr",
                                                   int NumThreads = 0);

    ~FTSpeechSynthesizer();

    FTSpeechSynthesizer(FTSpeechSynthesizer&& Other) noexcept;
    FTSpeechSynthesizer& operator=(FTSpeechSynthesizer&& Other) noexcept;

    FTSpeechSynthesizer(const FTSpeechSynthesizer&) = delete;
    FTSpeechSynthesizer& operator=(const FTSpeechSynthesizer&) = delete;

    bool IsValid() const;

    /** The model family this voice belongs to, as recognised when the directory was loaded. */
    std::string GetEngine() const;

    /** Whatever the model produces. Read it rather than assuming; the player is opened at it. */
    int GetSampleRate() const;

    int GetSpeakerCount() const;

    /** A short label for the interface, derived from the engine and the model directory. */
    std::string GetName() const;

    /** Every language this model can pronounce. See FTVoiceInfo::Languages. */
    const std::vector<std::string>& GetLanguages() const;

    /**
     * Empty on failure or for empty input. Speed is clamped to the supported range.
     *
     * LanguageCode overrides the one the model was created with, for engines that take a language
     * per call. Empty keeps the created one, which is all Piper can do.
     */
    std::vector<int16_t> Synthesize(std::string_view Text,
                                    int SpeakerId = 0,
                                    float Speed = 1.0f,
                                    const std::string& LanguageCode = {}) const;

private:
    struct Impl;

    std::unique_ptr<Impl> Data;

    FTSpeechSynthesizer();
};

struct FTVoiceModel
{
    std::string Directory;
    FTSpeechSynthesizer Synthesizer;
    std::unique_ptr<FTAudioPlayer> Player;
};

class FTSpeechService
{
public:
    /** ModelDirectories are tried in order; ones that fail to load are skipped rather than fatal.
        Empty (the default) builds a service with no voices, for a caller that means to load them
        later, off the constructing thread, via LoadVoices. */
    explicit FTSpeechService(const std::vector<std::string>& ModelDirectories = {});

    /** Loads and appends every model directory that succeeds, on top of whatever is already
        loaded. Not thread-safe against the rest of this class on its own — a caller running it
        on a worker thread has to keep every other call off the service until it returns, the same
        way AppSpeech serialises changes behind its worker lock. */
    void LoadVoices(const std::vector<std::string>& ModelDirectories);

    std::vector<FTVoiceModel> CreateVoiceModels(const std::vector<std::string>& ModelDirectories) const;
    void AddVoiceModels(std::vector<FTVoiceModel> Models);

    /** Drops every loaded voice and the audio it had cached, leaving the service in the state it
        was constructed in: IsAvailable goes false and LoadVoices brings it back.

        For an app sitting in the tray. The models are the largest thing it holds - Supertonic
        alone is around 140 MB of weights, on top of whatever the runtime allocated to run them -
        and none of it is doing anything while there is no window. The user's chosen voice, speed
        and mute setting are not touched, so a reload returns to the same place.

        Carries the same threading rule as LoadVoices: not safe against the rest of this class, so
        a caller has to keep every other call off the service until it returns. */
    void UnloadVoices();

    bool IsAvailable() const;

    int GetVoiceCount() const;
    FTVoiceInfo GetVoiceInfo(int Index) const;

    int GetActiveVoice() const;

    /** Out of range is ignored. Stops anything playing on the voice being left. */
    void SetActiveVoice(int Index);

    /**
     * Interrupts whatever is playing. False when unavailable, muted, or the text is empty.
     *
     * Uses the active voice when it supports LanguageCode, otherwise the fallback voice, otherwise
     * the active one anyway: mispronounced audio is more use than silence, and nothing here can
     * tell the user why it stayed quiet.
     */
    bool Speak(std::string_view Text, const std::string& LanguageCode, float RateScale = 1.0f);

    /**
     * Renders the text into the cache without playing it, so a caller can pay the synthesis cost
     * somewhere it does not matter and have the following Speak return immediately.
     *
     * Synthesis is the expensive half — a sentence is a few hundred milliseconds — and on the
     * thread drawing the interface that is a visible stall.
     */
    bool Prepare(std::string_view Text);

    /** Whether Speak would return without synthesising. Lets a caller skip a worker hop for text
        that is already rendered, which is the common case inside an exercise. */
    bool IsCached(std::string_view Text) const;

    /** Which voice would speak this language: the active one if it can, else the fallback, else
        the active one regardless. -1 when no voice is loaded at all. */
    int FindVoiceForLanguage(const std::string& LanguageCode) const;

    int FindFallbackVoice() const;

    void Stop();

    /** Whether any voice is still sounding the last Speak. */
    bool IsPlaying() const;

    bool IsEnabled() const;
    void SetEnabled(bool bValue);

    float GetSpeed() const;

    /** Clamped. Cached audio holds the old speed, so changing this drops the caches. */
    void SetSpeed(float Value);

    /** Per voice, since the models have different speaker counts. */
    int GetSpeakerId() const;
    void SetSpeakerId(int Value);

    /** Diagnostic for the active voice, and what the tests assert since audio output cannot be. */
    int GetCachedCount() const;

    /** Entries kept per voice before the least recently used one is dropped. Shrinking it evicts
        immediately rather than waiting for the next render. */
    void SetMaximumCacheEntries(int Value);

private:
    struct Voice
    {
        Voice(FTSpeechSynthesizer&& InSynthesizer, std::unique_ptr<FTAudioPlayer> InPlayer)
            : Synthesizer(std::move(InSynthesizer))
            , Player(std::move(InPlayer))
        {
        }

        FTSpeechSynthesizer Synthesizer;

        /** By pointer because FTAudioPlayer is deliberately not movable. */
        std::unique_ptr<FTAudioPlayer> Player;

        std::unordered_map<std::string, std::vector<int16_t>> Cache;

        /** Oldest used to newest used. A hit moves its key to the back, so the front is always the
            next one to evict regardless of when it was first synthesised. */
        std::deque<std::string> CacheOrder;

        int SpeakerId = 0;
    };

    Voice* GetActive();
    const Voice* GetActive() const;


    /** The cached samples for this key, synthesising and storing them if they are not there yet.
        Null when synthesis produced nothing. An empty language keeps the model's own. */
    const std::vector<int16_t>* Render(Voice& Active, const std::string& Key,
                                       float RenderSpeed, std::string_view Text,
                                       const std::string& LanguageCode);

    /** Drops entries beyond MaximumCacheEntries, oldest used first. */
    void EvictExcess(Voice& Active);

    void ClearCaches();

    std::vector<Voice> Voices;
    std::vector<std::string> LoadedDirectories;
    int ActiveVoice = 0;

    bool bEnabled = true;
    float Speed = 1.0f;

    int MaximumCacheEntries = DefaultMaximumCacheEntries;
};
