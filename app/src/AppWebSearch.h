#pragma once

#include "core/FTWebSearch.h"

#include <QString>

#include <vector>

class AppWebSearch
{
public:
    static std::vector<FTWebSearchSource> GetSources();

    static int GetActiveSourceIndex();

    static void Open(const QString& Word);
};
