#pragma once

#include <string>
#include <vector>

inline const std::string UndeterminedLanguage{"und"};

std::string DetectTextLanguage(const std::vector<std::string>& Paragraphs);
