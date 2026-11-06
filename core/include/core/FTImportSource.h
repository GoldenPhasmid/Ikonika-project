#pragma once

#include <string>
#include <vector>

/** One word with one translation, exactly as the source file carried them. Nothing is cleaned up
    here: the word may still hold its article, the translation may be a comma-separated list. The
    user decides what to fix after import. */
struct FTImportRow
{
    std::string TargetWord;
    std::string Translation;
};

/**
 * A source of word-translation pairs for import. One implementation per foreign format; the Anki
 * importer is the first of them. A source parses a file into rows and reports its own failures.
 */
class FTImportSource
{
public:
    virtual ~FTImportSource() = default;

    /** Parses Source into GetRows(). False on failure, with GetLastError() set. */
    virtual bool Parse(const std::string& Source) = 0;

    const std::vector<FTImportRow>& GetRows() const { return Rows; }

    /** Why the last Parse() failed, empty on success. */
    const std::string& GetLastError() const { return LastError; }

protected:
    std::vector<FTImportRow> Rows;
    std::string LastError;
};
