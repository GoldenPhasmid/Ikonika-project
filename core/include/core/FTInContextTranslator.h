#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

/**
 * An OPUS-MT sentence translation model, driven through onnxruntime's C API. One instance per
 * language pair, created from that pair's model directory.
 *
 * Loads the encoder and the merged decoder sessions out of a model directory and decodes greedily,
 * one token at a time, no beam. The decoder re-encodes the whole generated prefix every step
 * (use_cache_branch=false): the merged export's cached branch was broken by the export tool, and
 * its output diverges from the model's true greedy decoding. A sentence is short enough that the
 * quadratic cost is irrelevant.
 */
class FTInContextTranslator
{
public:
    static std::optional<FTInContextTranslator> Create(const std::string& ModelDirectory);

    /** Empty when the model is not loaded, the text is empty, or decoding fails. */
    std::string Translate(std::string_view Text) const;

    FTInContextTranslator(FTInContextTranslator&& Other) noexcept;
    FTInContextTranslator& operator=(FTInContextTranslator&& Other) noexcept;

    FTInContextTranslator(const FTInContextTranslator&) = delete;
    FTInContextTranslator& operator=(const FTInContextTranslator&) = delete;

    ~FTInContextTranslator();

private:
    struct Impl;
    std::unique_ptr<Impl> Data;

    FTInContextTranslator();

    std::string TranslateSentence(std::string_view Sentence) const;
};
