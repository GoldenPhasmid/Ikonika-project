#pragma once

#include <optional>
#include <string>

bool ExtractZipArchive(const std::string& ArchivePath, const std::string& DestinationDirectory);

std::optional<std::string> ReadZipEntry(const std::string& ArchivePath, const std::string& EntryName);
