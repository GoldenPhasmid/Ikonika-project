#pragma once

#include "core/FTImportSource.h"

#include <string>

/**
 * Imports word-translation pairs from a comma-separated file. The format the companion exporter
 * writes: one pair per line, fields quoted and ""-escaped where a comma, quote or newline needs
 * it, and an optional "Word,Translation" header row that is skipped when present.
 */
class FTCsvImporter final : public FTImportSource
{
public:
    bool Parse(const std::string& Source) override;
};
