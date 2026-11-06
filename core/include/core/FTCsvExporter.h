#pragma once

#include "core/FTImportSource.h"

#include <string>
#include <vector>

/**
 * Writes word-translation pairs as a comma-separated file. The inverse of FTCsvImporter: a
 * "Word,Translation" header line, then one pair per line, fields quoted and ""-escaped whenever a
 * comma, quote or newline would otherwise split them.
 */
class FTCsvExporter
{
public:
    /** Writes Rows to FilePath. False on failure, with GetLastError() set. */
    bool Write(const std::string& FilePath, const std::vector<FTImportRow>& Rows);

    const std::string& GetLastError() const { return LastError; }

private:
    std::string LastError;
};
