#pragma once

#include <cstdint>
#include <optional>
#include <string>

class FTDatabase;

std::string MakeLanguageErasureScope(const std::string& TargetLanguage);

std::optional<int64_t> ReadErasedDate(FTDatabase& Target);

bool RemoveErasedData(FTDatabase& Target);
