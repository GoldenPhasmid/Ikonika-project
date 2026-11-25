#include "core/FTLanguageDetection.h"

#include "core/FTLog.h"

#include <compact_lang_det.h>

namespace
{
    constexpr size_t SampleBytes{32 * 1024};

    std::string GatherSample(const std::vector<std::string>& Paragraphs)
    {
        std::string Sample;
        const size_t Middle{Paragraphs.size() / 2};

        for (size_t Offset = 0; Offset < Paragraphs.size() && Sample.size() < SampleBytes; ++Offset)
        {
            Sample += Paragraphs[(Middle + Offset) % Paragraphs.size()];
            Sample += '\n';
        }

        return Sample;
    }
}

std::string DetectTextLanguage(const std::vector<std::string>& Paragraphs)
{
    const std::string Sample{GatherSample(Paragraphs)};

    if (Sample.empty())
    {
        return UndeterminedLanguage;
    }

    bool bReliable{false};
    const CLD2::Language Detected{CLD2::DetectLanguage(Sample.data(), static_cast<int>(Sample.size()), true, &bReliable)};

    if (!bReliable || Detected == CLD2::UNKNOWN_LANGUAGE)
    {
        qCInfo(LogApp) << "Language detection found no reliable language in" << Sample.size() << "bytes";

        return UndeterminedLanguage;
    }

    return CLD2::LanguageCode(Detected);
}
