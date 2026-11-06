#include "core/FTSentencePieceTokenizer.h"

#include "core/FTLog.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    // sentencepiece's word-boundary marker, U+2581 in UTF-8.
    constexpr std::string_view MetaSpace = "\xe2\x96\x81";
    constexpr int MaximumPieceCharacterLength = 40;

    std::string JoinPath(const std::string& Directory, const char* Name)
    {
        return (std::filesystem::path(Directory) / Name).string();
    }

    bool ReadWholeFile(const std::string& Path, std::vector<char>& Out)
    {
        std::ifstream Stream(Path, std::ios::binary);

        if (!Stream)
        {
            return false;
        }

        Out.assign(std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>());

        return Stream.good() || Stream.eof();
    }

    /** Python's re \s under Unicode: the set its sentencepiece reference collapses with. */
    bool IsWhitespace(const QChar Character)
    {
        const char32_t Code = static_cast<char32_t>(Character.unicode());

        return (Code >= 0x09 && Code <= 0x0d)
            || (Code >= 0x1c && Code <= 0x1f)
            || Code == 0x20
            || Code == 0x85
            || Code == 0xa0
            || Code == 0x1680
            || (Code >= 0x2000 && Code <= 0x200a)
            || Code == 0x2028
            || Code == 0x2029
            || Code == 0x202f
            || Code == 0x205f
            || Code == 0x3000;
    }

    // --------------------------------------------------------------- minimal protobuf

    /** Wire-format reader for the SentencePieceModel proto packed into a .spm file. */
    class FTProtoReader
    {
    public:
        explicit FTProtoReader(const std::string_view Bytes)
            : Data(Bytes)
        {
        }

        bool AtEnd() const
        {
            return Position >= Data.size();
        }

        bool ReadVarint(uint64_t& Out)
        {
            Out = 0;
            int Shift = 0;

            while (Position < Data.size() && Shift < 64)
            {
                const uint8_t Byte = static_cast<uint8_t>(Data[Position++]);

                Out |= static_cast<uint64_t>(Byte & 0x7f) << Shift;
                Shift += 7;

                if ((Byte & 0x80) == 0)
                {
                    return true;
                }
            }

            return false;
        }

        bool ReadFixed32(uint32_t& Out)
        {
            if (Position + 4 > Data.size())
            {
                return false;
            }

            std::memcpy(&Out, Data.data() + Position, sizeof(Out));
            Position += 4;

            return true;
        }

        std::string_view ReadLengthDelimited()
        {
            uint64_t Length = 0;

            if (!ReadVarint(Length) || Position + Length > Data.size())
            {
                return {};
            }

            const std::string_view Out = Data.substr(Position, static_cast<size_t>(Length));
            Position += static_cast<size_t>(Length);

            return Out;
        }

        bool SkipBytes(size_t Count)
        {
            if (Position + Count > Data.size())
            {
                return false;
            }

            Position += Count;

            return true;
        }

    private:
        const std::string_view Data;
        size_t Position = 0;
    };

    void SkipField(FTProtoReader& Reader, uint64_t WireType)
    {
        uint64_t Value = 0;
        uint32_t Fixed32 = 0;

        switch (WireType)
        {
            case 0:
                Reader.ReadVarint(Value);
                break;
            case 1:
                Reader.SkipBytes(8);
                break;
            case 2:
                Reader.ReadLengthDelimited();
                break;
            case 5:
                Reader.ReadFixed32(Fixed32);
                break;
            default:
                break;
        }
    }

    /**
     * Walks the top-level SentencePieceModel message, calling Visit for every SentencePiece with its
     * piece text and unigram score. Everything else in the model file is skipped.
     */
    bool ParseSentencePieceModel(
        const std::string_view Bytes,
        const std::function<void(std::string_view, float)>& Visit)
    {
        FTProtoReader Reader(Bytes);

        while (!Reader.AtEnd())
        {
            uint64_t Tag = 0;

            if (!Reader.ReadVarint(Tag))
            {
                return false;
            }

            const uint64_t Field = Tag >> 3;
            const uint64_t WireType = Tag & 0x07;

            if (Field == 1 && WireType == 2)
            {
                const std::string_view PieceMessage = Reader.ReadLengthDelimited();

                if (PieceMessage.empty())
                {
                    continue;
                }

                FTProtoReader Inner(PieceMessage);
                std::string_view PieceText;
                float Score = 0.0f;

                while (!Inner.AtEnd())
                {
                    uint64_t InnerTag = 0;

                    if (!Inner.ReadVarint(InnerTag))
                    {
                        return false;
                    }

                    const uint64_t InnerField = InnerTag >> 3;
                    const uint64_t InnerWire = InnerTag & 0x07;

                    if (InnerField == 1 && InnerWire == 2)
                    {
                        PieceText = Inner.ReadLengthDelimited();
                    }
                    else if (InnerField == 2 && InnerWire == 5)
                    {
                        uint32_t Raw = 0;

                        if (!Inner.ReadFixed32(Raw))
                        {
                            return false;
                        }

                        std::memcpy(&Score, &Raw, sizeof(Score));
                    }
                    else
                    {
                        SkipField(Inner, InnerWire);
                    }
                }

                Visit(PieceText, Score);
            }
            else
            {
                SkipField(Reader, WireType);
            }
        }

        return true;
    }
}

// ------------------------------------------------------------------ FTSentencePieceTokenizer

std::optional<FTSentencePieceTokenizer> FTSentencePieceTokenizer::Create(const std::string& ModelDirectory)
{
    FTSentencePieceTokenizer Tokenizer;

    if (Tokenizer.Load(ModelDirectory))
    {
        return Tokenizer;
    }

    return std::nullopt;
}

bool FTSentencePieceTokenizer::Load(const std::string& ModelDirectory)
{
    const std::string SourcePath = JoinPath(ModelDirectory, "source.spm");
    const std::string VocabPath = JoinPath(ModelDirectory, "vocab.json");

    std::vector<char> SourceBytes;

    if (!ReadWholeFile(SourcePath, SourceBytes))
    {
        qCWarning(LogTranslate) << "missing source.spm:" << QString::fromStdString(SourcePath);

        return false;
    }

    if (!ParseSentencePieceModel(
            std::string_view(SourceBytes.data(), SourceBytes.size()),
            [this](std::string_view Text, float Score)
            {
                SourceScores.emplace(std::string(Text), Score);
                MaxPieceByteLength = std::max(MaxPieceByteLength, static_cast<int>(Text.size()));
            }))
    {
        qCWarning(LogTranslate) << "failed to parse" << QString::fromStdString(SourcePath);

        return false;
    }

    std::vector<char> VocabBytes;

    if (!ReadWholeFile(VocabPath, VocabBytes))
    {
        qCWarning(LogTranslate) << "missing vocab.json:" << QString::fromStdString(VocabPath);

        return false;
    }

    QJsonParseError Error;
    const QJsonDocument Document = QJsonDocument::fromJson(
        QByteArray(VocabBytes.data(), static_cast<int>(VocabBytes.size())), &Error);

    if (Document.isNull() || !Document.isObject())
    {
        qCWarning(LogTranslate) << "vocab.json is not valid JSON:" << Error.errorString();

        return false;
    }

    const QJsonObject Object = Document.object();
    PieceToId.reserve(static_cast<size_t>(Object.size()));

    for (auto It = Object.constBegin(); It != Object.constEnd(); ++It)
    {
        const int64_t Id = static_cast<int64_t>(It.value().toInteger(-1));

        if (Id < 0)
        {
            continue;
        }

        const std::string Piece = It.key().toStdString();
        PieceToId.emplace(Piece, Id);

        if (IdToPiece.size() <= static_cast<size_t>(Id))
        {
            IdToPiece.resize(static_cast<size_t>(Id) + 1);
        }

        IdToPiece[static_cast<size_t>(Id)] = Piece;
        VocabularySize = std::max(VocabularySize, Id + 1);
    }

    if (SourceScores.empty() || PieceToId.empty())
    {
        qCWarning(LogTranslate) << "empty tokenizer data:" << QString::fromStdString(ModelDirectory);

        return false;
    }

    // Marian's decoder_start_token_id is the pad token, which these exports keep as the last id of
    // the merged vocab. The id differs per pair (fr-en 59513, es-en 65000), so it must come from the
    // files, not from a constant that happens to match one model.
    const auto Pad = PieceToId.find("<pad>");
    DecoderStartToken = Pad == PieceToId.end() ? VocabularySize - 1 : Pad->second;
    EndOfSentenceToken = FindToken("</s>").value_or(EndOfSentenceToken);
    UnknownToken = FindToken("<unk>").value_or(UnknownToken);

    qCInfo(LogTranslate) << "loaded tokenizer from" << QString::fromStdString(ModelDirectory)
                         << "pieces:" << SourceScores.size() << "vocab:" << VocabularySize;

    return true;
}

int64_t FTSentencePieceTokenizer::IdForPiece(const std::string& Piece) const
{
    const auto It = PieceToId.find(Piece);

    return It == PieceToId.end() ? UnknownToken : It->second;
}

std::vector<int64_t> FTSentencePieceTokenizer::Encode(std::string_view Text) const
{
    if (Text.empty())
    {
        return {EndOfSentenceToken};
    }

    const QString Normalized = QString::fromUtf8(Text.data(), static_cast<int>(Text.size()))
                                   .normalized(QString::NormalizationForm_KC);

    QString Collapsed;
    Collapsed.reserve(Normalized.size());
    bool bPreviousWasWhitespace = true;

    for (const QChar Character : Normalized)
    {
        if (IsWhitespace(Character))
        {
            if (!bPreviousWasWhitespace)
            {
                Collapsed.append(QChar(' '));
            }

            bPreviousWasWhitespace = true;
        }
        else
        {
            Collapsed.append(Character);
            bPreviousWasWhitespace = false;
        }
    }

    while (!Collapsed.isEmpty() && Collapsed.endsWith(QChar(' ')))
    {
        Collapsed.chop(1);
    }

    if (Collapsed.isEmpty())
    {
        return {EndOfSentenceToken};
    }

    // Metaspace form: leading boundary marker, every space becomes a boundary marker. Matches
    // sentencepiece's piece boundaries after its own collapse.
    const QString Meta = QString::fromUtf8(MetaSpace.data(), static_cast<int>(MetaSpace.size()))
        + Collapsed.replace(QChar(' '), QString::fromUtf8(MetaSpace.data(), static_cast<int>(MetaSpace.size())));

    const QByteArray MetaBytes = Meta.toUtf8();
    const std::string_view MetaView(MetaBytes.constData(), static_cast<size_t>(MetaBytes.size()));

    // Byte offsets of each character start, so piece spans and unk merges split on characters.
    std::vector<size_t> CharacterStarts;
    CharacterStarts.reserve(static_cast<size_t>(MetaBytes.size()));

    for (int Position = 0; Position < MetaBytes.size();)
    {
        CharacterStarts.push_back(static_cast<size_t>(Position));

        const uint8_t Lead = static_cast<uint8_t>(MetaBytes.at(Position));

        Position += Lead < 0x80 ? 1
            : (Lead & 0xe0) == 0xc0 ? 2
            : (Lead & 0xf0) == 0xe0 ? 3
            : 4;
    }

    // The terminal offset makes CharacterStarts indexable by character position, where position
    // CharacterCount means "after the last character".
    CharacterStarts.push_back(static_cast<size_t>(MetaBytes.size()));

    const int CharacterCount = static_cast<int>(CharacterStarts.size()) - 1;

    // Viterbi over the metaspace string. Scores are negative log-probabilities from the unigram
    // model; unknown spans are scored at 0.0, exactly like sentencepiece.
    std::vector<float> BestScores(static_cast<size_t>(CharacterCount + 1), -std::numeric_limits<float>::infinity());
    std::vector<int> BestPredecessors(static_cast<size_t>(CharacterCount + 1), -1);
    std::vector<int64_t> BestPieces(static_cast<size_t>(CharacterCount + 1), 0);

    BestScores[0] = 0.0f;

    for (int i = 0; i < CharacterCount; ++i)
    {
        if (!std::isfinite(BestScores[static_cast<size_t>(i)]))
        {
            continue;
        }

        const size_t SpanStart = CharacterStarts[static_cast<size_t>(i)];
        bool bFoundPiece = false;

        for (int j = i + 1; j <= std::min(CharacterCount, i + MaximumPieceCharacterLength); ++j)
        {
            const size_t SpanEnd = CharacterStarts[static_cast<size_t>(j)];
            const std::string_view Candidate = MetaView.substr(SpanStart, SpanEnd - SpanStart);
            const auto It = SourceScores.find(std::string(Candidate));

            if (It == SourceScores.end())
            {
                continue;
            }

            bFoundPiece = true;

            const float PathScore = BestScores[static_cast<size_t>(i)] + It->second;

            if (PathScore > BestScores[static_cast<size_t>(j)])
            {
                BestScores[static_cast<size_t>(j)] = PathScore;
                BestPredecessors[static_cast<size_t>(j)] = i;
                BestPieces[static_cast<size_t>(j)] = IdForPiece(std::string(Candidate));
            }
        }

        // No piece covers this position: fall back to a one-character unknown span, scored 0.0.
        if (!bFoundPiece)
        {
            const int j = i + 1;

            if (BestScores[static_cast<size_t>(j)] < BestScores[static_cast<size_t>(i)])
            {
                BestScores[static_cast<size_t>(j)] = BestScores[static_cast<size_t>(i)];
                BestPredecessors[static_cast<size_t>(j)] = i;
                BestPieces[static_cast<size_t>(j)] = UnknownToken;
            }
        }
    }

    if (!std::isfinite(BestScores[static_cast<size_t>(CharacterCount)]))
    {
        return {EndOfSentenceToken};
    }

    // Backtrack to the best path, then merge consecutive unk spans into one surface piece and
    // re-look it up: sentencepiece returns the surface text, and the merged vocab may hold it.
    std::vector<int> BestStarts;
    std::vector<int64_t> RawPieces;

    for (int i = CharacterCount; i > 0;)
    {
        const int Predecessor = BestPredecessors[static_cast<size_t>(i)];

        BestStarts.push_back(Predecessor);
        RawPieces.push_back(BestPieces[static_cast<size_t>(i)]);
        i = Predecessor;
    }

    std::reverse(BestStarts.begin(), BestStarts.end());
    std::reverse(RawPieces.begin(), RawPieces.end());

    std::vector<int64_t> Result;
    Result.reserve(RawPieces.size() + 1);

    size_t UnkRunStart = std::string::npos;

    for (size_t Index = 0; Index < RawPieces.size(); ++Index)
    {
        if (RawPieces[Index] == UnknownToken)
        {
            if (UnkRunStart == std::string::npos)
            {
                UnkRunStart = Index;
            }

            continue;
        }

        if (UnkRunStart != std::string::npos)
        {
            const size_t SurfaceStart = CharacterStarts[static_cast<size_t>(BestStarts[UnkRunStart])];
            const size_t SurfaceEnd = CharacterStarts[static_cast<size_t>(BestStarts[Index])];
            const std::string Surface(MetaView.substr(SurfaceStart, SurfaceEnd - SurfaceStart));

            Result.push_back(IdForPiece(Surface));
            UnkRunStart = std::string::npos;
        }

        Result.push_back(RawPieces[Index]);
    }

    if (UnkRunStart != std::string::npos)
    {
        const size_t SurfaceStart = CharacterStarts[static_cast<size_t>(BestStarts[UnkRunStart])];
        const std::string Surface(MetaView.substr(SurfaceStart, MetaView.size() - SurfaceStart));

        Result.push_back(IdForPiece(Surface));
    }

    Result.push_back(EndOfSentenceToken);

    return Result;
}

std::string FTSentencePieceTokenizer::Decode(const std::vector<int64_t>& Ids) const
{
    std::string Out;

    for (const int64_t Id : Ids)
    {
        if (Id == EndOfSentenceToken || Id == UnknownToken || Id == DecoderStartToken)
        {
            continue;
        }

        if (Id < 0 || static_cast<int64_t>(IdToPiece.size()) <= Id)
        {
            continue;
        }

        const std::string& Piece = IdToPiece[static_cast<size_t>(Id)];

        if (Piece.size() >= MetaSpace.size()
            && Piece.compare(0, MetaSpace.size(), MetaSpace) == 0)
        {
            Out.append(" ");
            Out.append(Piece.data() + MetaSpace.size(), Piece.size() - MetaSpace.size());
        }
        else
        {
            Out.append(Piece);
        }
    }

    while (!Out.empty() && (Out.front() == ' ' || Out.back() == ' '))
    {
        if (!Out.empty() && Out.front() == ' ')
        {
            Out.erase(0, 1);
        }

        if (!Out.empty() && Out.back() == ' ')
        {
            Out.pop_back();
        }
    }

    return Out;
}

int64_t FTSentencePieceTokenizer::GetVocabularySize() const
{
    return VocabularySize;
}

int64_t FTSentencePieceTokenizer::GetDecoderStartToken() const
{
    return DecoderStartToken;
}

int64_t FTSentencePieceTokenizer::GetEndOfSentenceToken() const
{
    return EndOfSentenceToken;
}

int64_t FTSentencePieceTokenizer::GetUnknownToken() const
{
    return UnknownToken;
}

std::optional<int64_t> FTSentencePieceTokenizer::FindToken(const std::string& Piece) const
{
    const auto It = PieceToId.find(Piece);

    if (It == PieceToId.end())
    {
        return std::nullopt;
    }

    return It->second;
}

std::string_view FTSentencePieceTokenizer::GetPiece(int64_t Id) const
{
    if (Id < 0 || static_cast<int64_t>(IdToPiece.size()) <= Id)
    {
        return {};
    }

    return IdToPiece[static_cast<size_t>(Id)];
}
