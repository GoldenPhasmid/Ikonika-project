#include "core/FTSpeech.h"

#include "core/FTLog.h"

#include <sherpa-onnx/c-api/c-api.h>

#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <numbers>
#include <system_error>
#include <utility>

namespace
{
    /** Sentences are synthesised whole, so one chunk rather than many. */
    constexpr int MaxSentencesPerChunk = 1;

    constexpr int SynthesisThreads = 2;

    constexpr float SpeechPeakCeiling = 0.9f;
    constexpr float MaximumSpeechGain = 16.0f;
    constexpr float SilenceRmsThreshold = 1.0e-4f;

    constexpr float SupertonicPace = 1.05f;
    constexpr int SupertonicSteps = 8;
    constexpr int SupertonicSeed = 42;

    constexpr float StretchFrameSeconds = 0.040f;
    constexpr float StretchSearchSeconds = 0.012f;
    constexpr float UnstretchedTempoTolerance = 0.001f;
    constexpr float MinimumWindowSum = 1.0e-3f;
    constexpr size_t CoarseOffsetStep = 2;
    constexpr size_t CoarseSampleStep = 4;

    std::string Join(const std::string& Directory, const std::string& Name)
    {
        return (std::filesystem::path(Directory) / Name).string();
    }

    bool Exists(const std::string& Path)
    {
        std::error_code Ignored;

        return std::filesystem::exists(Path, Ignored);
    }

    bool AllExist(const std::string& Directory, std::initializer_list<const char*> Names)
    {
        for (const char* Name : Names)
        {
            if (!Exists(Join(Directory, Name)))
            {
                return false;
            }
        }

        return true;
    }

    /** The single .onnx at the top level of a Piper model directory, or empty. */
    std::string FindSingleOnnx(const std::string& Directory)
    {
        std::error_code Ignored;

        if (!std::filesystem::is_directory(Directory, Ignored))
        {
            return {};
        }

        std::vector<std::string> Found;

        for (const auto& Entry : std::filesystem::directory_iterator(Directory, Ignored))
        {
            if (Entry.is_regular_file(Ignored) && Entry.path().extension() == ".onnx")
            {
                Found.push_back(Entry.path().string());
            }
        }

        // Sorted so the choice does not depend on directory order.
        std::sort(Found.begin(), Found.end());

        return Found.size() == 1 ? Found.front() : std::string();
    }

    std::string LastPathComponent(const std::string& Directory)
    {
        return std::filesystem::path(Directory).filename().string();
    }

    std::string DerivePiperLanguage(const std::string& OnnxPath)
    {
        QFile File(QString::fromStdString(OnnxPath + ".json"));

        if (!File.open(QIODevice::ReadOnly))
        {
            return {};
        }

        const QJsonDocument Doc = QJsonDocument::fromJson(File.readAll());
        const QString Code = Doc.object().value("language").toObject().value("code").toString();

        if (Code.isEmpty())
        {
            return {};
        }

        const int Underscore = Code.indexOf(QChar('_'));
        const QString Primary = Underscore >= 0 ? Code.left(Underscore) : Code;

        return Primary.toLower().toStdString();
    }
}

// ------------------------------------------------------------------ FTSpeechSynthesizer

struct FTSpeechSynthesizer::Impl
{
    const SherpaOnnxOfflineTts* Tts = nullptr;

    std::string Engine;

    /** Whether a call can name a language other than the one the model was created for. Piper's
        model fixes its language; Supertonic takes one per call. Set at load from the detection. */
    bool bTakesLanguagePerCall = false;

    std::string Name;

    std::vector<std::string> Languages;

    /** The JSON blob sherpa-onnx wants for the model's own language, used when a call names none. */
    std::string LanguageExtra;

    ~Impl()
    {
        if (Tts != nullptr)
        {
            SherpaOnnxDestroyOfflineTts(Tts);
        }
    }
};

FTSpeechSynthesizer::FTSpeechSynthesizer()
    : Data(std::make_unique<Impl>())
{
}

FTSpeechSynthesizer::~FTSpeechSynthesizer() = default;

FTSpeechSynthesizer::FTSpeechSynthesizer(FTSpeechSynthesizer&& Other) noexcept = default;

FTSpeechSynthesizer& FTSpeechSynthesizer::operator=(FTSpeechSynthesizer&& Other) noexcept = default;

namespace
{
    /**
     * The 31 languages Supertonic ships with, from its own documentation. Written down because
     * neither engine reports what it supports, and a wrong entry produces confident nonsense rather
     * than an error.
     *
     * Piper is not listed here: each of its models is built for one language, given at creation.
     */
    const std::vector<std::string>& SupertonicLanguages()
    {
        static const std::vector<std::string> Languages = {
            "ar", "bg", "hr", "cs", "da", "nl", "en", "et", "fi", "fr", "de", "el", "hi", "hu",
            "id", "it", "ja", "ko", "lv", "lt", "pl", "pt", "ro", "ru", "sk", "sl", "es", "sv",
            "tr", "uk", "vi"
        };

        return Languages;
    }

    std::string MakeLanguageExtra(const std::string& LanguageCode)
    {
        return "{\"lang\": \"" + LanguageCode + "\", \"seed\": " + std::to_string(SupertonicSeed) + "}";
    }

    std::vector<float> MakeHannWindow(size_t Length)
    {
        std::vector<float> Window(Length);
        const double Denominator = static_cast<double>(Length - 1);

        for (size_t Index = 0; Index < Length; ++Index)
        {
            Window[Index] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(Index) / Denominator));
        }

        return Window;
    }

    float Correlate(const float* First, const float* Second, size_t Length, size_t Step)
    {
        float Sum = 0.0f;

        for (size_t Index = 0; Index < Length; Index += Step)
        {
            Sum += First[Index] * Second[Index];
        }

        return Sum;
    }

    size_t FindBestOverlap(const std::vector<float>& Padded, size_t Natural, size_t Target,
                           size_t SearchRadius, size_t OverlapLength)
    {
        const float* Reference = Padded.data() + Natural;

        size_t Best = Target;
        float BestScore = std::numeric_limits<float>::lowest();

        for (size_t Candidate = Target - SearchRadius; Candidate <= Target + SearchRadius; Candidate += CoarseOffsetStep)
        {
            const float Score = Correlate(Padded.data() + Candidate, Reference, OverlapLength, CoarseSampleStep);

            if (Score > BestScore)
            {
                BestScore = Score;
                Best = Candidate;
            }
        }

        const size_t CoarseBest = Best;
        BestScore = std::numeric_limits<float>::lowest();

        for (size_t Candidate = CoarseBest - CoarseOffsetStep; Candidate <= CoarseBest + CoarseOffsetStep; ++Candidate)
        {
            const float Score = Correlate(Padded.data() + Candidate, Reference, OverlapLength, 1);

            if (Score > BestScore)
            {
                BestScore = Score;
                Best = Candidate;
            }
        }

        return Best;
    }
}

float ComputeSpeechGain(std::span<const float> Samples)
{
    if (Samples.empty())
    {
        return 1.0f;
    }

    double SumOfSquares = 0.0;
    float Peak = 0.0f;

    for (const float Sample : Samples)
    {
        SumOfSquares += static_cast<double>(Sample) * static_cast<double>(Sample);
        Peak = std::max(Peak, std::abs(Sample));
    }

    const float Rms = static_cast<float>(std::sqrt(SumOfSquares / static_cast<double>(Samples.size())));

    if (Rms < SilenceRmsThreshold)
    {
        return 1.0f;
    }

    const float Gain = std::min(TargetSpeechRms / Rms, MaximumSpeechGain);

    return std::min(Gain, SpeechPeakCeiling / Peak);
}

size_t FindSpeechStart(std::span<const float> Samples, int SampleRate)
{
    float Peak = 0.0f;

    for (const float Sample : Samples)
    {
        Peak = std::max(Peak, std::abs(Sample));
    }

    if (Peak <= 0.0f)
    {
        return 0;
    }

    const float Threshold = Peak * LeadingSilenceThreshold;
    const auto Onset = std::find_if(Samples.begin(), Samples.end(),
                                    [Threshold](float Sample) { return std::abs(Sample) >= Threshold; });
    const size_t OnsetIndex = static_cast<size_t>(std::distance(Samples.begin(), Onset));
    const size_t Margin = static_cast<size_t>(std::max(SampleRate, 0) * LeadingSilenceMarginSeconds);

    return OnsetIndex > Margin ? OnsetIndex - Margin : 0;
}

std::vector<float> StretchSpeech(std::span<const float> Samples, int SampleRate, float Tempo)
{
    const float Clamped = std::clamp(Tempo, MinimumSpeechSpeed, MaximumSpeechSpeed);
    const size_t FrameLength = static_cast<size_t>(static_cast<float>(std::max(SampleRate, 0)) * StretchFrameSeconds);
    const size_t Hop = FrameLength / 2;

    if (Samples.empty() || Hop == 0 || std::abs(Clamped - 1.0f) < UnstretchedTempoTolerance)
    {
        return {Samples.begin(), Samples.end()};
    }

    const double AnalysisHop = static_cast<double>(Hop) * Clamped;
    const size_t SearchRadius = static_cast<size_t>(static_cast<float>(SampleRate) * StretchSearchSeconds);
    const size_t Lead = SearchRadius + FrameLength;

    std::vector<float> Padded(Lead + Samples.size() + SearchRadius + 3 * FrameLength, 0.0f);
    std::copy(Samples.begin(), Samples.end(), Padded.begin() + static_cast<std::ptrdiff_t>(Lead));

    const std::vector<float> Window = MakeHannWindow(FrameLength);
    const size_t FrameCount = static_cast<size_t>(static_cast<double>(Samples.size()) / AnalysisHop) + 1;

    std::vector<float> Output(FrameCount * Hop + FrameLength, 0.0f);
    std::vector<float> WindowSum(Output.size(), 0.0f);

    size_t Previous = Lead;

    for (size_t Frame = 0; Frame < FrameCount; ++Frame)
    {
        const size_t Target = Lead + static_cast<size_t>(std::llround(static_cast<double>(Frame) * AnalysisHop));
        const size_t Position = Frame == 0
            ? Target
            : FindBestOverlap(Padded, Previous + Hop, Target, SearchRadius, Hop);

        for (size_t Index = 0; Index < FrameLength; ++Index)
        {
            Output[Frame * Hop + Index] += Padded[Position + Index] * Window[Index];
            WindowSum[Frame * Hop + Index] += Window[Index];
        }

        Previous = Position;
    }

    const size_t Length = std::min(Output.size(), static_cast<size_t>(static_cast<double>(Samples.size()) / Clamped));
    Output.resize(Length);

    for (size_t Index = 0; Index < Length; ++Index)
    {
        if (WindowSum[Index] > MinimumWindowSum)
        {
            Output[Index] /= WindowSum[Index];
        }
    }

    return Output;
}

bool IsVoiceModelDirectory(const std::string& ModelDirectory)
{
    if (ModelDirectory.empty())
    {
        return false;
    }

    const bool bIsSupertonic = AllExist(ModelDirectory, {
        "duration_predictor.int8.onnx",
        "text_encoder.int8.onnx",
        "vector_estimator.int8.onnx",
        "vocoder.int8.onnx",
        "tts.json",
        "unicode_indexer.bin",
        "voice.bin"
    });

    if (bIsSupertonic)
    {
        return true;
    }

    return !FindSingleOnnx(ModelDirectory).empty()
        && Exists(Join(ModelDirectory, "tokens.txt"));
}

std::optional<FTSpeechSynthesizer> FTSpeechSynthesizer::Create(const std::string& ModelDirectory,
                                                          const std::string& LanguageCode,
                                                          int NumThreads)
{
    if (ModelDirectory.empty())
    {
        return std::nullopt;
    }

    const bool bIsSupertonic = AllExist(ModelDirectory, {
        "duration_predictor.int8.onnx",
        "text_encoder.int8.onnx",
        "vector_estimator.int8.onnx",
        "vocoder.int8.onnx",
        "tts.json",
        "unicode_indexer.bin",
        "voice.bin"
    });

    const std::string PiperModel = FindSingleOnnx(ModelDirectory);
    const bool bIsPiper = !bIsSupertonic
        && !PiperModel.empty()
        && Exists(Join(ModelDirectory, "tokens.txt"));

    if (!bIsSupertonic && !bIsPiper)
    {
        return std::nullopt;
    }

    // Held as locals for the whole call, because the config stores raw pointers and sherpa-onnx
    // reads them during construction.
    const std::string DurationPredictor = Join(ModelDirectory, "duration_predictor.int8.onnx");
    const std::string TextEncoder = Join(ModelDirectory, "text_encoder.int8.onnx");
    const std::string VectorEstimator = Join(ModelDirectory, "vector_estimator.int8.onnx");
    const std::string Vocoder = Join(ModelDirectory, "vocoder.int8.onnx");
    const std::string TtsJson = Join(ModelDirectory, "tts.json");
    const std::string UnicodeIndexer = Join(ModelDirectory, "unicode_indexer.bin");
    const std::string VoiceStyle = Join(ModelDirectory, "voice.bin");

    const std::string Tokens = Join(ModelDirectory, "tokens.txt");
    const std::string EspeakData = Join(ModelDirectory, "espeak-ng-data");

    SherpaOnnxOfflineTtsConfig Config{};
    Config.max_num_sentences = MaxSentencesPerChunk;
    Config.model.num_threads = NumThreads > 0 ? NumThreads : SynthesisThreads;
    Config.model.provider = "cpu";

    if (bIsSupertonic)
    {
        Config.model.supertonic.duration_predictor = DurationPredictor.c_str();
        Config.model.supertonic.text_encoder = TextEncoder.c_str();
        Config.model.supertonic.vector_estimator = VectorEstimator.c_str();
        Config.model.supertonic.vocoder = Vocoder.c_str();
        Config.model.supertonic.tts_json = TtsJson.c_str();
        Config.model.supertonic.unicode_indexer = UnicodeIndexer.c_str();
        Config.model.supertonic.voice_style = VoiceStyle.c_str();
    }
    else
    {
        Config.model.vits.model = PiperModel.c_str();
        Config.model.vits.tokens = Tokens.c_str();

        // Piper models bundle their own espeak-ng-data for phonemisation, which is separate from
        // the copy used for the IPA display.
        Config.model.vits.data_dir = Exists(EspeakData) ? EspeakData.c_str() : "";
    }

    const SherpaOnnxOfflineTts* Tts = SherpaOnnxCreateOfflineTts(&Config);

    if (Tts == nullptr)
    {
        return std::nullopt;
    }

    std::string PiperLanguage = DerivePiperLanguage(PiperModel);

    if (PiperLanguage.empty())
    {
        PiperLanguage = LanguageCode;
    }

    FTSpeechSynthesizer Result;
    Result.Data->Tts = Tts;
    Result.Data->Engine = bIsSupertonic ? SupertonicEngine : PiperEngine;
    Result.Data->bTakesLanguagePerCall = bIsSupertonic;
    Result.Data->Name = LastPathComponent(ModelDirectory);
    Result.Data->Languages = bIsSupertonic ? SupertonicLanguages() : std::vector{PiperLanguage};

    if (bIsSupertonic)
    {
        Result.Data->LanguageExtra = MakeLanguageExtra(LanguageCode);
    }

    return Result;
}

bool FTSpeechSynthesizer::IsValid() const
{
    return Data != nullptr && Data->Tts != nullptr;
}

std::string FTSpeechSynthesizer::GetEngine() const
{
    return Data->Engine;
}

int FTSpeechSynthesizer::GetSampleRate() const
{
    return IsValid() ? SherpaOnnxOfflineTtsSampleRate(Data->Tts) : 0;
}

int FTSpeechSynthesizer::GetSpeakerCount() const
{
    return IsValid() ? SherpaOnnxOfflineTtsNumSpeakers(Data->Tts) : 0;
}

std::string FTSpeechSynthesizer::GetName() const
{
    return Data->Name;
}

const std::vector<std::string>& FTSpeechSynthesizer::GetLanguages() const
{
    static const std::vector<std::string> None;

    return Data != nullptr ? Data->Languages : None;
}

std::vector<int16_t> FTSpeechSynthesizer::Synthesize(std::string_view Text,
                                                   int SpeakerId,
                                                   float Speed,
                                                   const std::string& LanguageCode) const
{
    std::vector<int16_t> Result;

    if (!IsValid() || Text.empty())
    {
        return Result;
    }

    const std::string Terminated(Text);

    // Held for the whole call, since the config keeps a raw pointer into it. An empty code leaves
    // the model's own language, which is all an engine without a per-call override can do anyway.
    const std::string Extra = LanguageCode.empty() || !Data->bTakesLanguagePerCall
        ? Data->LanguageExtra
        : MakeLanguageExtra(LanguageCode);

    const float Tempo = std::clamp(Speed, MinimumSpeechSpeed, MaximumSpeechSpeed);
    const bool bIsSupertonic = Data->Engine == SupertonicEngine;

    SherpaOnnxGenerationConfig Generation{};
    Generation.sid = SpeakerId;
    Generation.speed = bIsSupertonic ? SupertonicPace : Tempo;
    Generation.num_steps = bIsSupertonic ? SupertonicSteps : 0;
    Generation.extra = Extra.empty() ? nullptr : Extra.c_str();

    const SherpaOnnxGeneratedAudio* Audio =
        SherpaOnnxOfflineTtsGenerateWithConfig(Data->Tts,
                                               Terminated.c_str(),
                                               &Generation,
                                               nullptr,
                                               nullptr);

    if (Audio == nullptr)
    {
        return Result;
    }

    std::vector<float> Stretched;
    std::span<const float> Generated(Audio->samples, static_cast<size_t>(Audio->n));

    if (bIsSupertonic)
    {
        Stretched = StretchSpeech(Generated, Audio->sample_rate, Tempo);
        Generated = Stretched;
    }

    const std::span<const float> Speech = Generated.subspan(FindSpeechStart(Generated, Audio->sample_rate));
    const float Gain = ComputeSpeechGain(Speech);

    Result.reserve(Speech.size());

    for (const float Sample : Speech)
    {
        const float Clamped = std::clamp(Sample * Gain, -1.0f, 1.0f);
        Result.push_back(static_cast<int16_t>(Clamped * 32767.0f));
    }

    SherpaOnnxDestroyOfflineTtsGeneratedAudio(Audio);

    return Result;
}

// --------------------------------------------------------------------- FTSpeechService

FTSpeechService::FTSpeechService(const std::vector<std::string>& ModelDirectories)
{
    LoadVoices(ModelDirectories);
}

void FTSpeechService::LoadVoices(const std::vector<std::string>& ModelDirectories)
{
    AddVoiceModels(CreateVoiceModels(ModelDirectories));
}

std::vector<FTVoiceModel> FTSpeechService::CreateVoiceModels(const std::vector<std::string>& ModelDirectories) const
{
    std::vector<FTVoiceModel> Models;

    for (const std::string& Directory : ModelDirectories)
    {
        const bool bClaimed = std::find(LoadedDirectories.begin(), LoadedDirectories.end(), Directory)
                                  != LoadedDirectories.end()
                              || std::any_of(Models.begin(), Models.end(),
                                             [&Directory](const FTVoiceModel& Model) { return Model.Directory == Directory; });

        if (bClaimed)
        {
            continue;
        }

        if (!IsVoiceModelDirectory(Directory))
        {
            qCDebug(LogSpeech) << "not a voice model, skipped:"
                               << QString::fromStdString(Directory);

            continue;
        }

        std::optional<FTSpeechSynthesizer> Synthesizer = FTSpeechSynthesizer::Create(Directory);

        if (!Synthesizer.has_value() || !Synthesizer->IsValid())
        {
            qCWarning(LogSpeech) << "voice model failed to load, skipped:"
                                 << QString::fromStdString(Directory);

            continue;
        }

        const int SampleRate = Synthesizer->GetSampleRate();

        qCInfo(LogSpeech) << "voice loaded:" << QString::fromStdString(Directory)
                          << "at" << SampleRate << "Hz";

        Models.push_back(FTVoiceModel{Directory, std::move(*Synthesizer), std::make_unique<FTAudioPlayer>(SampleRate)});
    }

    return Models;
}

void FTSpeechService::AddVoiceModels(std::vector<FTVoiceModel> Models)
{
    for (FTVoiceModel& Model : Models)
    {
        Voices.emplace_back(std::move(Model.Synthesizer), std::move(Model.Player));
        LoadedDirectories.push_back(std::move(Model.Directory));
    }
}

void FTSpeechService::UnloadVoices()
{
    if (Voices.empty())
    {
        return;
    }

    // Stopped before anything is destroyed: a player torn down mid-playback takes its audio thread
    // with it while that thread is still reading the samples it was handed.
    Stop();

    Voices.clear();
    LoadedDirectories.clear();

    qCInfo(LogSpeech) << "voices unloaded";

    // The active voice, the speed and the enabled flag stay. They are the user's settings rather
    // than properties of the models, and a reload should come back to the voice they chose.
}

FTSpeechService::Voice* FTSpeechService::GetActive()
{
    if (ActiveVoice < 0 || ActiveVoice >= static_cast<int>(Voices.size()))
    {
        return nullptr;
    }

    return &Voices[static_cast<size_t>(ActiveVoice)];
}

const FTSpeechService::Voice* FTSpeechService::GetActive() const
{
    if (ActiveVoice < 0 || ActiveVoice >= static_cast<int>(Voices.size()))
    {
        return nullptr;
    }

    return &Voices[static_cast<size_t>(ActiveVoice)];
}

bool FTSpeechService::IsAvailable() const
{
    const Voice* Active = GetActive();

    return Active != nullptr && Active->Player->IsAvailable();
}

int FTSpeechService::GetVoiceCount() const
{
    return static_cast<int>(Voices.size());
}

FTVoiceInfo FTSpeechService::GetVoiceInfo(int Index) const
{
    if (Index < 0 || Index >= static_cast<int>(Voices.size()))
    {
        return {};
    }

    const Voice& Entry = Voices[static_cast<size_t>(Index)];

    FTVoiceInfo Info;
    Info.Engine = Entry.Synthesizer.GetEngine();
    Info.Name = Entry.Synthesizer.GetName();
    Info.SampleRate = Entry.Synthesizer.GetSampleRate();
    Info.SpeakerCount = Entry.Synthesizer.GetSpeakerCount();
    Info.Languages = Entry.Synthesizer.GetLanguages();
    Info.bFallback = Index == FindFallbackVoice();

    return Info;
}

int FTSpeechService::FindFallbackVoice() const
{
    // Whichever covers the most languages, which in practice means Supertonic over a Piper model
    // built for one. Derived rather than configured, so adding a model needs no list kept in step.
    int Best = -1;
    size_t Widest = 0;

    for (size_t Index = 0; Index < Voices.size(); ++Index)
    {
        const size_t Count = Voices[Index].Synthesizer.GetLanguages().size();

        if (Best < 0 || Count > Widest)
        {
            Best = static_cast<int>(Index);
            Widest = Count;
        }
    }

    return Best;
}

int FTSpeechService::FindVoiceForLanguage(const std::string& LanguageCode) const
{
    if (Voices.empty())
    {
        return -1;
    }

    if (LanguageCode.empty())
    {
        return ActiveVoice;
    }

    const FTVoiceInfo Active = GetVoiceInfo(ActiveVoice);

    if (IsLanguageSupported(Active, LanguageCode))
    {
        return ActiveVoice;
    }

    for (int Index = 0; Index < GetVoiceCount(); ++Index)
    {
        if (Index != ActiveVoice && IsLanguageSupported(GetVoiceInfo(Index), LanguageCode))
        {
            return Index;
        }
    }

    return ActiveVoice;
}

int FTSpeechService::GetActiveVoice() const
{
    return ActiveVoice;
}

void FTSpeechService::SetActiveVoice(int Index)
{
    if (Index == ActiveVoice || Index < 0 || Index >= static_cast<int>(Voices.size()))
    {
        return;
    }

    // The voice being left keeps its cache, which is the point of holding both open, but it should
    // not keep talking.
    if (Voice* Active = GetActive())
    {
        Active->Player->Stop();
    }

    ActiveVoice = Index;
}

namespace
{
    /** Speaker, speed and language are all part of the identity of the audio, not just the text.
        Language matters because a word can be spelled the same in two of them — "day" said in
        English and in French are different sounds from the same characters.

        The text itself is lower-cased so "Chien" and "chien" share one rendering; the synthesiser
        does not change what it says by case. QString's fold handles the accented letters a plain
        ASCII lower would miss. */
    std::string MakeCacheKey(std::string_view Text, int SpeakerId, float Speed,
                             const std::string& LanguageCode)
    {
        char Suffix[64];
        std::snprintf(Suffix, sizeof(Suffix), "|%d|%.2f|", SpeakerId, static_cast<double>(Speed));

        const QString Lowered =
            QString::fromUtf8(Text.data(), static_cast<int>(Text.size())).toLower();

        return Lowered.toStdString() + Suffix + LanguageCode;
    }

    std::string AppendPauseForEnglishWord(std::string_view Text, const std::string& LanguageCode)
    {
        std::string Result(Text);

        if (LanguageCode != "en" || Result.find(' ') != std::string::npos || Result.empty())
        {
            return Result;
        }

        const char Last = Result.back();

        if (Last != ',' && Last != '.' && Last != '!' && Last != '?')
        {
            Result += ',';
        }

        return Result;
    }
}

bool FTSpeechService::Prepare(std::string_view Text)
{
    Voice* Active = GetActive();

    if (!bEnabled || Text.empty() || Active == nullptr)
    {
        return false;
    }

    return Render(*Active, MakeCacheKey(Text, Active->SpeakerId, Speed, {}), Speed, Text, {}) != nullptr;
}

const std::vector<int16_t>* FTSpeechService::Render(Voice& Active, const std::string& Key,
                                                   float RenderSpeed, std::string_view Text,
                                                   const std::string& LanguageCode)
{
    auto Found = Active.Cache.find(Key);

    if (Found != Active.Cache.end())
    {
        const auto Used = std::find(Active.CacheOrder.begin(), Active.CacheOrder.end(), Key);

        if (Used != Active.CacheOrder.end())
        {
            Active.CacheOrder.erase(Used);
        }

        Active.CacheOrder.push_back(Key);

        return &Found->second;
    }

    QElapsedTimer Timer;
    Timer.start();

    std::vector<int16_t> Samples =
        Active.Synthesizer.Synthesize(Text, Active.SpeakerId, RenderSpeed, LanguageCode);

    qCDebug(LogSpeech) << "synthesis took" << Timer.elapsed() << "ms for"
                       << static_cast<int>(Text.size()) << "characters";

    if (Samples.empty())
    {
        return nullptr;
    }

    Active.CacheOrder.push_back(Key);
    const std::vector<int16_t>* Rendered = &Active.Cache.emplace(Key, std::move(Samples)).first->second;

    EvictExcess(Active);

    return Rendered;
}

void FTSpeechService::EvictExcess(Voice& Active)
{
    while (static_cast<int>(Active.CacheOrder.size()) > MaximumCacheEntries)
    {
        const std::string& Oldest = Active.CacheOrder.front();

        Active.Cache.erase(Oldest);
        Active.CacheOrder.pop_front();
    }
}

void FTSpeechService::SetMaximumCacheEntries(int Value)
{
    MaximumCacheEntries = std::max(Value, 1);

    for (Voice& Entry : Voices)
    {
        EvictExcess(Entry);
    }
}

bool FTSpeechService::IsCached(std::string_view Text) const
{
    const Voice* Active = GetActive();

    if (Text.empty() || Active == nullptr)
    {
        return false;
    }

    return Active->Cache.find(MakeCacheKey(Text, Active->SpeakerId, Speed, {}))
        != Active->Cache.end();
}

bool FTSpeechService::Speak(std::string_view Text, const std::string& LanguageCode, float RateScale)
{
    if (!bEnabled || Text.empty() || Voices.empty())
    {
        return false;
    }

    const int Index = FindVoiceForLanguage(LanguageCode);

    if (Index < 0)
    {
        return false;
    }

    Voice& Chosen = Voices[static_cast<size_t>(Index)];

    qCDebug(LogSpeech) << "voice" << QString::fromStdString(Chosen.Synthesizer.GetName())
                       << "chosen for language" << QString::fromStdString(LanguageCode);

    // Whichever voice was playing stops, including a different one from the voice about to speak:
    // two models have separate players, so without this both would sound at once.
    Stop();

    const std::string Adjusted = AppendPauseForEnglishWord(Text, LanguageCode);

    const float RenderSpeed = std::clamp(Speed * RateScale, MinimumSpeechSpeed, MaximumSpeechSpeed);

    const std::vector<int16_t>* Samples =
        Render(Chosen, MakeCacheKey(Adjusted, Chosen.SpeakerId, RenderSpeed, LanguageCode),
               RenderSpeed, Adjusted, LanguageCode);

    if (Samples == nullptr)
    {
        return false;
    }

    return Chosen.Player->Play(*Samples, Chosen.Synthesizer.GetSampleRate());
}

std::vector<int16_t> FTSpeechService::RenderNarration(int VoiceIndex, std::string_view Text,
                                                     const std::string& LanguageCode) const
{
    if (!bEnabled || Text.empty() || VoiceIndex < 0 || VoiceIndex >= static_cast<int>(Voices.size()))
    {
        return {};
    }

    const Voice& Chosen{Voices[static_cast<size_t>(VoiceIndex)]};

    QElapsedTimer Timer;
    Timer.start();

    std::vector<int16_t> Samples{Chosen.Synthesizer.Synthesize(Text, Chosen.SpeakerId, Speed, LanguageCode)};

    qCDebug(LogSpeech) << "narration synthesis took" << Timer.elapsed() << "ms for"
                       << static_cast<int>(Text.size()) << "characters";

    if (Samples.empty())
    {
        return Samples;
    }

    const size_t GapFrames{static_cast<size_t>(Chosen.Synthesizer.GetSampleRate() * NarrationSentenceGapSeconds)};
    Samples.insert(Samples.end(), GapFrames, int16_t{0});

    return Samples;
}

std::optional<FTNarrationChunk> FTSpeechService::PlayNarration(int VoiceIndex, std::vector<int16_t> Samples, bool bContinues)
{
    if (!bEnabled || Samples.empty() || VoiceIndex < 0 || VoiceIndex >= static_cast<int>(Voices.size()))
    {
        return std::nullopt;
    }

    Voice& Chosen{Voices[static_cast<size_t>(VoiceIndex)]};
    const int SampleRate{Chosen.Synthesizer.GetSampleRate()};

    if (!bContinues)
    {
        Stop();

        if (!Chosen.Player->Play(std::move(Samples), SampleRate))
        {
            return std::nullopt;
        }

        return FTNarrationChunk{0, Chosen.Player->GetQueuedFrames()};
    }

    const uint64_t StartFrame{Chosen.Player->GetQueuedFrames()};

    if (!Chosen.Player->Append(std::move(Samples), SampleRate))
    {
        return std::nullopt;
    }

    return FTNarrationChunk{StartFrame, Chosen.Player->GetQueuedFrames()};
}

uint64_t FTSpeechService::GetNarrationPosition(int VoiceIndex) const
{
    if (VoiceIndex < 0 || VoiceIndex >= static_cast<int>(Voices.size()))
    {
        return 0;
    }

    return Voices[static_cast<size_t>(VoiceIndex)].Player->GetPlayedFrames();
}

bool FTSpeechService::IsNarrationPlaying(int VoiceIndex) const
{
    if (VoiceIndex < 0 || VoiceIndex >= static_cast<int>(Voices.size()))
    {
        return false;
    }

    return Voices[static_cast<size_t>(VoiceIndex)].Player->IsPlaying();
}

void FTSpeechService::Stop()
{
    for (Voice& Entry : Voices)
    {
        Entry.Player->Stop();
    }
}

bool FTSpeechService::IsPlaying() const
{
    return std::any_of(Voices.begin(), Voices.end(), [](const Voice& Entry) { return Entry.Player->IsPlaying(); });
}

bool FTSpeechService::IsEnabled() const
{
    return bEnabled;
}

void FTSpeechService::SetEnabled(bool bValue)
{
    bEnabled = bValue;

    if (!bEnabled)
    {
        Stop();
    }
}

float FTSpeechService::GetSpeed() const
{
    return Speed;
}

void FTSpeechService::SetSpeed(float Value)
{
    const float Clamped = std::clamp(Value, MinimumSpeechSpeed, MaximumSpeechSpeed);

    if (Clamped == Speed)
    {
        return;
    }

    Speed = Clamped;

    // Everything cached was rendered at the old speed.
    ClearCaches();
}

int FTSpeechService::GetSpeakerId() const
{
    const Voice* Active = GetActive();

    return Active != nullptr ? Active->SpeakerId : 0;
}

void FTSpeechService::SetSpeakerId(int Value)
{
    Voice* Active = GetActive();

    if (Active == nullptr || Value == Active->SpeakerId)
    {
        return;
    }

    if (Value < 0 || Value >= Active->Synthesizer.GetSpeakerCount())
    {
        return;
    }

    Active->SpeakerId = Value;

    // Only this voice's cache holds the old speaker.
    Active->Cache.clear();
    Active->CacheOrder.clear();
}

int FTSpeechService::GetCachedCount() const
{
    const Voice* Active = GetActive();

    return Active != nullptr ? static_cast<int>(Active->Cache.size()) : 0;
}

void FTSpeechService::ClearCaches()
{
    for (Voice& Entry : Voices)
    {
        Entry.Cache.clear();
        Entry.CacheOrder.clear();
    }
}
