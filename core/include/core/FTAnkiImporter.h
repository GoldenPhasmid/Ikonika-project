#pragma once

#include "core/FTImportSource.h"

#include <string>

/**
 * Imports word-translation pairs from Anki. Accepts the two shapes Anki produces: the plain-text
 * export (a UTF-8 file with fields separated by a configurable separator) and the collection.anki2
 * database. Which one is chosen by the file extension; anything else is treated as plain text.
 */
class FTAnkiImporter final : public FTImportSource
{
public:
    bool Parse(const std::string& Source) override;

private:
    bool ParseTextFile(const std::string& FilePath);
    bool ParseAnkiDatabase(const std::string& FilePath);

    void AddRow(const std::string& TargetWord, const std::string& Translation);
};
