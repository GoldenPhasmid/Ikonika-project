#pragma once

#include <string>
#include <string_view>
#include <vector>

bool IsEnglishForm(std::string_view Word, std::string_view Base);

bool MentionsTranslation(std::string_view Text, const std::vector<std::string>& Translations);
