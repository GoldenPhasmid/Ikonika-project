#include "core/FTInContextTranslator.h"

#include "core/FTLog.h"
#include "core/FTSentencePieceTokenizer.h"
#include "core/FTSentenceSplitter.h"
#include "core/FTSettings.h"

#include <onnxruntime_c_api.h>

#include <windows.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr int MaxDecodeLength = 128;

    struct FTDecoderShape
    {
        int LayerCount = 0;
        int64_t HeadCount = 0;
        int64_t HeadDimension = 0;
    };

    /**
     * An onnxruntime handle held by scope rather than by hand.
     *
     * Every Ort type is freed through its own Release function on the OrtApi table, so the
     * deleter carries that function alongside the pointer. Nothing here is copyable, which is
     * what stops a handle being released twice.
     */
    template<typename T>
    struct FTOrtRelease
    {
        void(ORT_API_CALL* Release)(T*) = nullptr;

        void operator()(T* Value) const
        {
            if (Release != nullptr && Value != nullptr)
            {
                Release(Value);
            }
        }
    };

    template<typename T>
    using FTOrtOwned = std::unique_ptr<T, FTOrtRelease<T>>;

    template<typename T>
    FTOrtOwned<T> MakeOrtOwned(T* Value, void(ORT_API_CALL* Release)(T*))
    {
        return FTOrtOwned<T>(Value, FTOrtRelease<T>{Release});
    }

    std::string JoinPath(const std::string& Directory, const char* Name)
    {
        return (std::filesystem::path(Directory) / Name).string();
    }

    std::wstring ToWide(const std::string& Value)
    {
        if (Value.empty())
        {
            return {};
        }

        const int Length = MultiByteToWideChar(CP_UTF8, 0, Value.data(), static_cast<int>(Value.size()), nullptr, 0);
        std::wstring Result(static_cast<size_t>(Length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, Value.data(), static_cast<int>(Value.size()), Result.data(), Length);

        return Result;
    }

    bool CheckStatus(const OrtApi* Api, const OrtStatus* Status, const char* What)
    {
        if (Status == nullptr)
        {
            return true;
        }

        qCWarning(LogTranslate) << What << QString::fromUtf8(Api->GetErrorMessage(Status));
        Api->ReleaseStatus(const_cast<OrtStatus*>(Status));

        return false;
    }

    const OrtApi* LoadOnnxRuntime(HMODULE& OutModule)
    {
        OutModule = LoadLibraryW(L"onnxruntime.dll");

        if (OutModule == nullptr)
        {
            qCWarning(LogTranslate) << "failed to load onnxruntime.dll";

            return nullptr;
        }

        const auto GetApiBase = reinterpret_cast<const OrtApiBase* (*)()>(GetProcAddress(OutModule, "OrtGetApiBase"));

        if (GetApiBase == nullptr)
        {
            qCWarning(LogTranslate) << "onnxruntime.dll has no OrtGetApiBase";

            return nullptr;
        }

        const OrtApiBase* Base = GetApiBase();

        if (Base == nullptr)
        {
            return nullptr;
        }

        const OrtApi* Api = Base->GetApi(ORT_API_VERSION);

        if (Api == nullptr)
        {
            qCWarning(LogTranslate) << "onnxruntime is older than ORT_API_VERSION" << ORT_API_VERSION;
        }

        return Api;
    }

    bool CreateSession(const OrtApi* Api,
                       OrtEnv* Env,
                       const std::string& ModelPath,
                       OrtSession** Out)
    {
        OrtSessionOptions* RawOptions = nullptr;

        if (!CheckStatus(Api, Api->CreateSessionOptions(&RawOptions), "CreateSessionOptions"))
        {
            return false;
        }

        const FTOrtOwned<OrtSessionOptions> Options =
            MakeOrtOwned(RawOptions, Api->ReleaseSessionOptions);

        if (!CheckStatus(Api, Api->SetSessionGraphOptimizationLevel(Options.get(), ORT_ENABLE_BASIC),
                         "SetSessionGraphOptimizationLevel"))
        {
            return false;
        }

        // The merged decoder is corrupted by the EXTENDED/ALL optimizers; BASIC is what runs it.
        // Silent from ERROR up hides the benign MergeShapeInfo noise without hiding real failures.
        if (!CheckStatus(Api, Api->SetSessionLogSeverityLevel(Options.get(), 3), "SetSessionLogSeverityLevel"))
        {
            return false;
        }

        const std::wstring Wide = ToWide(ModelPath);

        return CheckStatus(Api, Api->CreateSession(Env, Wide.c_str(), Options.get(), Out), "CreateSession");
    }

    std::optional<FTDecoderShape> ReadDecoderShape(const OrtApi* Api, const OrtSession* Decoder)
    {
        size_t InputCount = 0;

        if (!CheckStatus(Api, Api->SessionGetInputCount(Decoder, &InputCount), "SessionGetInputCount"))
        {
            return std::nullopt;
        }

        OrtAllocator* Allocator = nullptr;

        if (!CheckStatus(Api, Api->GetAllocatorWithDefaultOptions(&Allocator), "GetAllocatorWithDefaultOptions"))
        {
            return std::nullopt;
        }

        FTDecoderShape Shape;

        for (size_t Index = 0; Index < InputCount; ++Index)
        {
            char* RawName = nullptr;

            if (!CheckStatus(Api, Api->SessionGetInputName(Decoder, Index, Allocator, &RawName), "SessionGetInputName"))
            {
                return std::nullopt;
            }

            const std::string Name = RawName;
            Api->AllocatorFree(Allocator, RawName);

            if (Name.rfind("past_key_values.", 0) != 0 || Name.find(".decoder.key") == std::string::npos)
            {
                continue;
            }

            ++Shape.LayerCount;

            if (Shape.HeadCount > 0)
            {
                continue;
            }

            OrtTypeInfo* RawTypeInfo = nullptr;

            if (!CheckStatus(Api, Api->SessionGetInputTypeInfo(Decoder, Index, &RawTypeInfo), "SessionGetInputTypeInfo"))
            {
                return std::nullopt;
            }

            const FTOrtOwned<OrtTypeInfo> TypeInfo = MakeOrtOwned(RawTypeInfo, Api->ReleaseTypeInfo);
            const OrtTensorTypeAndShapeInfo* TensorInfo = nullptr;

            if (!CheckStatus(Api, Api->CastTypeInfoToTensorInfo(TypeInfo.get(), &TensorInfo), "CastTypeInfoToTensorInfo"))
            {
                return std::nullopt;
            }

            size_t DimensionCount = 0;
            Api->GetDimensionsCount(TensorInfo, &DimensionCount);

            if (DimensionCount != 4)
            {
                break;
            }

            std::vector<int64_t> Dimensions(DimensionCount);
            Api->GetDimensions(TensorInfo, Dimensions.data(), DimensionCount);

            Shape.HeadCount = Dimensions[1];
            Shape.HeadDimension = Dimensions[3];
        }

        if (Shape.LayerCount == 0 || Shape.HeadCount <= 0 || Shape.HeadDimension <= 0)
        {
            qCWarning(LogTranslate) << "the decoder has no readable past key shape";

            return std::nullopt;
        }

        return Shape;
    }

    /** Owns the input buffers and the OrtValues built on top of them, in run scope. Every member
        frees itself, so this needs none of the five special members. */
    struct FTInputSet
    {
        const OrtApi* Api = nullptr;
        OrtMemoryInfo* Memory = nullptr;

        std::vector<std::unique_ptr<std::vector<int64_t>>> Int64Buffers;
        std::vector<std::unique_ptr<std::vector<float>>> FloatBuffers;
        std::vector<std::unique_ptr<std::vector<uint8_t>>> BoolBuffers;
        std::vector<FTOrtOwned<OrtValue>> Values;

        FTInputSet(const OrtApi* InApi, OrtMemoryInfo* InMemory)
            : Api(InApi)
            , Memory(InMemory)
        {
        }

        /** The pointers Api->Run takes, valid while this set is alive. */
        std::vector<const OrtValue*> GetPointers() const
        {
            std::vector<const OrtValue*> Pointers;
            Pointers.reserve(Values.size());

            for (const FTOrtOwned<OrtValue>& Value : Values)
            {
                Pointers.push_back(Value.get());
            }

            return Pointers;
        }

        bool AddInt64(const std::vector<int64_t>& Data, const std::initializer_list<int64_t>& Shape)
        {
            Int64Buffers.push_back(std::make_unique<std::vector<int64_t>>(Data));

            std::vector<int64_t> ShapeVec(Shape);
            OrtValue* Value = nullptr;

            const OrtStatus* Status = Api->CreateTensorWithDataAsOrtValue(
                Memory,
                Int64Buffers.back()->data(),
                Data.size() * sizeof(int64_t),
                ShapeVec.data(),
                ShapeVec.size(),
                ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64,
                &Value);

            if (!CheckStatus(Api, Status, "CreateInt64Tensor"))
            {
                return false;
            }

            Values.push_back(MakeOrtOwned(Value, Api->ReleaseValue));

            return true;
        }

        bool AddFloat(const std::vector<float>& Data, const std::initializer_list<int64_t>& Shape)
        {
            FloatBuffers.push_back(std::make_unique<std::vector<float>>(Data));

            // A zero-length tensor still needs a non-null address. This one is never read from
            // or written to, since the tensor it backs has no elements.
            static float EmptyTensorPlaceholder = 0.0f;
            void* Pointer = Data.empty()
                ? static_cast<void*>(&EmptyTensorPlaceholder)
                : static_cast<void*>(FloatBuffers.back()->data());

            std::vector<int64_t> ShapeVec(Shape);
            OrtValue* Value = nullptr;

            const OrtStatus* Status = Api->CreateTensorWithDataAsOrtValue(
                Memory,
                Pointer,
                Data.size() * sizeof(float),
                ShapeVec.data(),
                ShapeVec.size(),
                ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                &Value);

            if (!CheckStatus(Api, Status, "CreateFloatTensor"))
            {
                return false;
            }

            Values.push_back(MakeOrtOwned(Value, Api->ReleaseValue));

            return true;
        }

        bool AddBool(const std::vector<uint8_t>& Data, const std::initializer_list<int64_t>& Shape)
        {
            BoolBuffers.push_back(std::make_unique<std::vector<uint8_t>>(Data));

            std::vector<int64_t> ShapeVec(Shape);
            OrtValue* Value = nullptr;

            const OrtStatus* Status = Api->CreateTensorWithDataAsOrtValue(
                Memory,
                BoolBuffers.back()->data(),
                Data.size() * sizeof(uint8_t),
                ShapeVec.data(),
                ShapeVec.size(),
                ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL,
                &Value);

            if (!CheckStatus(Api, Status, "CreateBoolTensor"))
            {
                return false;
            }

            Values.push_back(MakeOrtOwned(Value, Api->ReleaseValue));

            return true;
        }
    };

    QString NormalizePunctuation(const QString& Sentence)
    {
        QString Result;
        Result.reserve(Sentence.size());

        for (const QChar Character : Sentence)
        {
            switch (Character.unicode())
            {
            case 0x2018:
            case 0x2019:
            case 0x201A:
            case 0x201B:
            case 0x2032:
            case 0x00B4:
                Result += u'\'';
                break;
            case 0x201C:
            case 0x201D:
            case 0x201E:
            case 0x201F:
            case 0x2033:
            case 0x00AB:
            case 0x00BB:
                Result += u'"';
                break;
            case 0x2010:
            case 0x2011:
            case 0x2012:
            case 0x2013:
            case 0x2212:
                Result += u'-';
                break;
            case 0x2014:
            case 0x2015:
                Result += QStringLiteral(" - ");
                break;
            case 0x2026:
                Result += QStringLiteral("...");
                break;
            case 0x00A0:
            case 0x2007:
            case 0x2009:
            case 0x200A:
            case 0x202F:
                Result += u' ';
                break;
            case 0x00AD:
            case 0x200B:
            case 0xFEFF:
                break;
            default:
                Result += Character;
                break;
            }
        }

        return Result.simplified();
    }
}

// --------------------------------------------------------------- FTInContextTranslator

/** A library handle freed by scope. Declared before every Ort handle below so it outlives them:
    members are destroyed in reverse declaration order, and releasing an Ort object after its
    library is gone would call into unmapped code. */
struct FTOrtLibrary
{
    HMODULE Module = nullptr;

    FTOrtLibrary() = default;

    explicit FTOrtLibrary(HMODULE InModule)
        : Module(InModule)
    {
    }

    ~FTOrtLibrary()
    {
        if (Module != nullptr)
        {
            FreeLibrary(Module);
        }
    }

    FTOrtLibrary(const FTOrtLibrary&) = delete;
    FTOrtLibrary& operator=(const FTOrtLibrary&) = delete;

    FTOrtLibrary(FTOrtLibrary&& Other) noexcept
        : Module(std::exchange(Other.Module, nullptr))
    {
    }

    FTOrtLibrary& operator=(FTOrtLibrary&& Other) noexcept
    {
        if (this != &Other)
        {
            if (Module != nullptr)
            {
                FreeLibrary(Module);
            }

            Module = std::exchange(Other.Module, nullptr);
        }

        return *this;
    }
};

struct FTInContextTranslator::Impl
{
    FTOrtLibrary Library;
    const OrtApi* Api = nullptr;

    FTOrtOwned<OrtEnv> Env;
    FTOrtOwned<OrtSession> Encoder;
    FTOrtOwned<OrtSession> Decoder;
    FTOrtOwned<OrtMemoryInfo> MemoryInfo;

    FTSentencePieceTokenizer Tokenizer;
    int64_t VocabularySize = 0;

    FTDecoderShape DecoderShape;
    std::optional<int64_t> TargetToken;
    std::vector<int64_t> ExcludedTokens;
};

FTInContextTranslator::FTInContextTranslator()
    : Data(std::make_unique<Impl>())
{
}

FTInContextTranslator::~FTInContextTranslator() = default;

FTInContextTranslator::FTInContextTranslator(FTInContextTranslator&& Other) noexcept = default;

FTInContextTranslator& FTInContextTranslator::operator=(FTInContextTranslator&& Other) noexcept = default;

std::optional<FTInContextTranslator> FTInContextTranslator::Create(const std::string& ModelDirectory)
{
    FTInContextTranslator Result;
    Impl& ResultData = *Result.Data;

    HMODULE Module = nullptr;
    ResultData.Api = LoadOnnxRuntime(Module);
    ResultData.Library = FTOrtLibrary(Module);

    if (ResultData.Api == nullptr)
    {
        return std::nullopt;
    }

    const OrtApi* Api = ResultData.Api;

    OrtEnv* RawEnv = nullptr;

    if (!CheckStatus(Api, Api->CreateEnv(ORT_LOGGING_LEVEL_ERROR, "Ikonika", &RawEnv), "CreateEnv"))
    {
        return std::nullopt;
    }

    ResultData.Env = MakeOrtOwned(RawEnv, Api->ReleaseEnv);

    OrtMemoryInfo* RawMemoryInfo = nullptr;

    if (!CheckStatus(Api, Api->CreateCpuMemoryInfo(OrtDeviceAllocator, OrtMemTypeCPUInput, &RawMemoryInfo),
                     "CreateCpuMemoryInfo"))
    {
        return std::nullopt;
    }

    ResultData.MemoryInfo = MakeOrtOwned(RawMemoryInfo, Api->ReleaseMemoryInfo);

    OrtSession* RawEncoder = nullptr;
    OrtSession* RawDecoder = nullptr;

    if (!CreateSession(Api, ResultData.Env.get(), JoinPath(ModelDirectory, "encoder_model.onnx"), &RawEncoder)
        || !CreateSession(Api, ResultData.Env.get(), JoinPath(ModelDirectory, "decoder_model_merged.onnx"), &RawDecoder))
    {
        ResultData.Encoder = MakeOrtOwned(RawEncoder, Api->ReleaseSession);
        ResultData.Decoder = MakeOrtOwned(RawDecoder, Api->ReleaseSession);

        return std::nullopt;
    }

    ResultData.Encoder = MakeOrtOwned(RawEncoder, Api->ReleaseSession);
    ResultData.Decoder = MakeOrtOwned(RawDecoder, Api->ReleaseSession);

    const std::optional<FTDecoderShape> DecoderShape = ReadDecoderShape(Api, ResultData.Decoder.get());

    if (!DecoderShape.has_value())
    {
        return std::nullopt;
    }

    ResultData.DecoderShape = *DecoderShape;

    std::optional<FTSentencePieceTokenizer> Tokenizer = FTSentencePieceTokenizer::Create(ModelDirectory);

    if (!Tokenizer.has_value())
    {
        return std::nullopt;
    }

    ResultData.Tokenizer = std::move(*Tokenizer);
    ResultData.VocabularySize = ResultData.Tokenizer.GetVocabularySize();

    ResultData.ExcludedTokens.push_back(ResultData.Tokenizer.GetDecoderStartToken());

    QFile SettingsFile(QString::fromStdString(JoinPath(ModelDirectory, "translation.json")));

    if (!SettingsFile.open(QIODevice::ReadOnly))
    {
        return Result;
    }

    const QJsonObject Settings = QJsonDocument::fromJson(SettingsFile.readAll()).object();
    const QString TargetToken = Settings.value("targetToken").toString();
    const QString ExcludedCharacters = Settings.value("excludedCharacters").toString();

    if (!TargetToken.isEmpty())
    {
        ResultData.TargetToken = ResultData.Tokenizer.FindToken(TargetToken.toStdString());

        if (!ResultData.TargetToken.has_value())
        {
            qCWarning(LogTranslate) << "target token" << TargetToken << "is not in the vocabulary of"
                                    << QString::fromStdString(ModelDirectory);

            return std::nullopt;
        }
    }

    for (int64_t Id = 0; Id < ResultData.VocabularySize && !ExcludedCharacters.isEmpty(); ++Id)
    {
        const std::string_view Piece = ResultData.Tokenizer.GetPiece(Id);
        const QString PieceText = QString::fromUtf8(Piece.data(), static_cast<qsizetype>(Piece.size()));

        for (const QChar Character : ExcludedCharacters)
        {
            if (PieceText.contains(Character))
            {
                ResultData.ExcludedTokens.push_back(Id);

                break;
            }
        }
    }

    qCInfo(LogTranslate) << "translation settings from" << QString::fromStdString(ModelDirectory)
                         << "target token:" << TargetToken << "excluded tokens:" << ResultData.ExcludedTokens.size();

    return Result;
}

std::string FTInContextTranslator::Translate(std::string_view Text) const
{
    const QString Source = QString::fromUtf8(Text.data(), static_cast<qsizetype>(Text.size()));

    std::string Result;

    for (const FTSentenceRange& Range : SplitSentences(Source, FTSettings::Get().GetLanguageRules().TargetLanguage))
    {
        const QString Sentence = NormalizePunctuation(Source.mid(Range.Start, Range.End - Range.Start));

        const std::string Translation = TranslateSentence(Sentence.toStdString());

        if (Translation.empty())
        {
            continue;
        }

        if (!Result.empty())
        {
            Result += ' ';
        }

        Result += Translation;
    }

    return Result;
}

std::string FTInContextTranslator::TranslateSentence(std::string_view Sentence) const
{
    const Impl& Data = *this->Data;
    const OrtApi* Api = Data.Api;

    std::vector<int64_t> SourceIds = Data.Tokenizer.Encode(Sentence);

    // A lone eos token means empty or whitespace-only input.
    if (SourceIds.size() <= 1)
    {
        return {};
    }

    if (Data.TargetToken.has_value())
    {
        SourceIds.insert(SourceIds.begin(), *Data.TargetToken);
    }

    const int64_t EncoderLength = static_cast<int64_t>(SourceIds.size());
    const std::vector<int64_t> Mask(static_cast<size_t>(EncoderLength), 1);

    // -------------------------------------------------------------- encoder
    FTOrtOwned<OrtValue> HiddenState;

    {
        FTInputSet Inputs(Api, Data.MemoryInfo.get());
        Inputs.AddInt64(SourceIds, {1, EncoderLength});
        Inputs.AddInt64(Mask, {1, EncoderLength});

        const std::vector<const OrtValue*> InputPointers = Inputs.GetPointers();

        const char* InputNames[] = {"input_ids", "attention_mask"};
        const char* OutputNames[] = {"last_hidden_state"};
        OrtValue* Outputs[1] = {nullptr};

        if (!CheckStatus(Api,
                         Api->Run(Data.Encoder.get(), nullptr, InputNames, InputPointers.data(), 2, OutputNames, 1, Outputs),
                         "encoder run"))
        {
            return {};
        }

        HiddenState = MakeOrtOwned(Outputs[0], Api->ReleaseValue);
    }

    void* RawHidden = nullptr;

    if (!CheckStatus(Api, Api->GetTensorMutableData(HiddenState.get(), &RawHidden), "GetTensorMutableData(hidden)"))
    {
        return {};
    }

    const float* HiddenData = static_cast<const float*>(RawHidden);

    // The encoder hidden size, read from the actual output rather than assumed.
    int64_t HiddenSize = 0;
    OrtTensorTypeAndShapeInfo* RawShapeInfo = nullptr;

    if (CheckStatus(Api, Api->GetTensorTypeAndShape(HiddenState.get(), &RawShapeInfo), "GetTensorTypeAndShape(hidden)"))
    {
        const FTOrtOwned<OrtTensorTypeAndShapeInfo> ShapeInfo =
            MakeOrtOwned(RawShapeInfo, Api->ReleaseTensorTypeAndShapeInfo);

        size_t DimensionCount = 0;
        Api->GetDimensionsCount(ShapeInfo.get(), &DimensionCount);

        std::vector<int64_t> Dimensions(DimensionCount);
        Api->GetDimensions(ShapeInfo.get(), Dimensions.data(), DimensionCount);

        if (DimensionCount >= 3)
        {
            HiddenSize = Dimensions[2];
        }
    }

    if (HiddenSize <= 0)
    {
        return {};
    }

    // -------------------------------------------------------------- decoder
    const std::vector<float> HiddenView(HiddenData, HiddenData + static_cast<size_t>(EncoderLength) * static_cast<size_t>(HiddenSize));

    const FTDecoderShape& Shape = Data.DecoderShape;
    std::vector<std::string> PastNames;

    for (int Layer = 0; Layer < Shape.LayerCount; ++Layer)
    {
        for (const char* Kind : {"decoder.key", "decoder.value", "encoder.key", "encoder.value"})
        {
            PastNames.push_back("past_key_values." + std::to_string(Layer) + "." + Kind);
        }
    }

    const std::vector<float> EmptyPast;  // (1, 8, 0, 64), the else branch ignores it

    std::vector<int64_t> Generated{Data.Tokenizer.GetDecoderStartToken()};
    std::vector<const char*> InputNames;
    InputNames.reserve(4 + PastNames.size());
    InputNames.push_back("encoder_attention_mask");
    InputNames.push_back("input_ids");
    InputNames.push_back("encoder_hidden_states");
    InputNames.push_back("use_cache_branch");

    for (const std::string& Name : PastNames)
    {
        InputNames.push_back(Name.c_str());
    }

    for (int Step = 0; Step < MaxDecodeLength; ++Step)
    {
        FTInputSet Inputs(Api, Data.MemoryInfo.get());
        Inputs.AddInt64(Mask, {1, EncoderLength});
        Inputs.AddInt64(Generated, {1, static_cast<int64_t>(Generated.size())});
        Inputs.AddFloat(HiddenView, {1, EncoderLength, HiddenSize});
        Inputs.AddBool({0}, {1});

        for (int Layer = 0; Layer < Shape.LayerCount; ++Layer)
        {
            for (int Count = 0; Count < 4; ++Count)
            {
                Inputs.AddFloat(EmptyPast, {1, Shape.HeadCount, 0, Shape.HeadDimension});
            }
        }

        const std::vector<const OrtValue*> InputPointers = Inputs.GetPointers();

        const char* OutputNames[] = {"logits"};
        OrtValue* Outputs[1] = {nullptr};

        if (!CheckStatus(Api,
                         Api->Run(Data.Decoder.get(), nullptr, InputNames.data(), InputPointers.data(),
                                  InputPointers.size(), OutputNames, 1, Outputs),
                         "decoder run"))
        {
            break;
        }

        const FTOrtOwned<OrtValue> LogitsValue = MakeOrtOwned(Outputs[0], Api->ReleaseValue);

        void* RawLogits = nullptr;

        if (!CheckStatus(Api, Api->GetTensorMutableData(LogitsValue.get(), &RawLogits), "GetTensorMutableData(logits)"))
        {
            break;
        }

        float* Logits = static_cast<float*>(RawLogits);
        float* LastRow = Logits + (Generated.size() - 1) * static_cast<size_t>(Data.VocabularySize);

        for (const int64_t Id : Data.ExcludedTokens)
        {
            LastRow[static_cast<size_t>(Id)] = -std::numeric_limits<float>::infinity();
        }

        int64_t BestId = Data.Tokenizer.GetUnknownToken();
        float BestScore = -std::numeric_limits<float>::infinity();

        for (int64_t Id = 0; Id < Data.VocabularySize; ++Id)
        {
            if (LastRow[static_cast<size_t>(Id)] > BestScore)
            {
                BestScore = LastRow[static_cast<size_t>(Id)];
                BestId = Id;
            }
        }

        if (BestId == Data.Tokenizer.GetEndOfSentenceToken())
        {
            break;
        }

        Generated.push_back(BestId);
    }

    return Data.Tokenizer.Decode(Generated);
}
