#pragma once

#include <string>
#include <vector>

struct FAccentFold
{
    const char* Utf8;
    char Base;
};

const std::vector<FAccentFold>& AccentFoldTable();

std::string FoldAccents(const std::string& Value);

std::string FoldCase(const std::string& Value);
