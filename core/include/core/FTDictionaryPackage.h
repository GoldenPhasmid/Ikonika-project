#pragma once

#include <optional>
#include <string>
#include <vector>

inline constexpr int DictionarySchemaVersion = 11;
inline constexpr int OldestDictionarySchemaVersion = 10;

bool IsReadableDictionarySchema(int SchemaVersion);

class FTDatabase;

/** A value from the metadata table, empty when the table or the row is absent. Absent is the
    normal case for a dictionary built before that table or that row existed. */
std::string ReadDictionaryMetadata(FTDatabase& Target, const std::string& Key);

/** Every dictionary file is named this way, which is what the scanner matches on. */
inline constexpr const char* DictionaryFileSuffix = "-dictionary.db";

/**
 * Metadata describing an installed dictionary for UI display without reopening the file.
 *
 * - Contents: Target language, supported translation languages, and display properties.
 * - Structure: Grouped per target language (1:N translations) in a single file to prevent file explosion.
 */
struct FTDictionaryPackage
{
    std::string FilePath;

    /** ISO 639-1, lowercase. The language being learned. */
    std::string TargetLanguage;

    /** ISO 639-1, lowercase. Every language this file can translate into, at least one. */
    std::vector<std::string> NativeLanguages;

    /** 0 for a file built before the metadata table existed. */
    int SchemaVersion = 0;

    int WordCount = 0;

    bool SupportsNative(const std::string& LanguageCode) const;

    /** False unless the file was built for exactly this app's schema. */
    bool IsCompatible() const;
};

/**
 * Reads metadata for a dictionary file, returning `std::nullopt` if invalid, empty, or missing language info.
 *
 * - Fast Path: Reads directly from an embedded metadata table.
 * - Fallback: Scans rows to infer languages (adds a few ms on large files).
 */
std::optional<FTDictionaryPackage> ReadDictionaryPackage(const std::string& FilePath);

/**
 * Scans a directory and its immediate subdirectories for dictionary packages.
 *
 * - Scope: Checks root and 1-level deep subdirectories.
 * - Layout Support: Unifies flat (installed) and nested (build tree, e.g., `fr/dict.db`) layouts without special-casing.
 */
std::vector<FTDictionaryPackage> FindDictionaryPackages(const std::string& Directory,
                                                        std::vector<FTDictionaryPackage>* OutIncompatible = nullptr);
