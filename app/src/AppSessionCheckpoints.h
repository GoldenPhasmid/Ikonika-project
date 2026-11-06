#pragma once

#include <string>
#include <utility>
#include <vector>

class AppSessionCheckpoints
{
public:
    static void Save(const std::string& Kind, const std::string& SessionKey,
                     const std::string& StateJson);

    static void Clear(const std::string& Kind, const std::string& SessionKey);

    static std::vector<std::pair<std::string, std::string>> LoadAll(const std::string& Kind);
};
