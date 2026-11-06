#pragma once

#include "core/FTReaderDocument.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

inline constexpr int BookCatalogSchemaVersion = 1;

struct FTCatalogBook
{
    std::string Id;
    std::string Title;
    std::string Author;
    std::string Level;
    std::string FileName;
    std::string Url;
    std::int64_t Bytes = 0;
    std::string Sha256;

    EFTDocumentFormat GetFormat() const;
};

struct FTBookCatalog
{
    int SchemaVersion = 0;
    std::string TargetLanguage;
    std::vector<FTCatalogBook> Books;

    const FTCatalogBook* FindBook(const std::string& Id) const;
};

std::optional<FTBookCatalog> ParseBookCatalog(const std::string& Json);
