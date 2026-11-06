#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/**
 * SentencePiece unigram tokenizer for an OPUS-MT model, one instance per language pair.
 *
 * Segments text with the model's original source.spm, whose scores the shipped tokenizer.json was
 * rescored away from. Reads the files out of a model directory; no SentencePiece dependency.
 */
class FTSentencePieceTokenizer
{
public:
    static std::optional<FTSentencePieceTokenizer> Create(const std::string& ModelDirectory);

    /** The ids that feed the encoder, ending in the end-of-sentence token like HF's input_ids. */
    std::vector<int64_t> Encode(std::string_view Text) const;

    /** Model output ids to text. Special tokens are dropped, matching skip_special_tokens=True. */
    std::string Decode(const std::vector<int64_t>& Ids) const;

    /** The merged vocabulary size, which is the width of the decoder's logits. */
    int64_t GetVocabularySize() const;

    int64_t GetDecoderStartToken() const;
    int64_t GetEndOfSentenceToken() const;
    int64_t GetUnknownToken() const;

    std::optional<int64_t> FindToken(const std::string& Piece) const;
    std::string_view GetPiece(int64_t Id) const;

private:
    struct Piece
    {
        std::string Text;
        float Score = 0.0f;
    };

    bool Load(const std::string& ModelDirectory);
    int64_t IdForPiece(const std::string& Piece) const;

    std::vector<Piece> SourcePieces;
    std::unordered_map<std::string, float> SourceScores;
    std::unordered_map<std::string, int64_t> PieceToId;
    std::vector<std::string> IdToPiece;

    int64_t VocabularySize = 0;
    int64_t UnknownToken = 1;
    int64_t EndOfSentenceToken = 0;
    int64_t DecoderStartToken = 0;
    int MaxPieceByteLength = 0;
};
