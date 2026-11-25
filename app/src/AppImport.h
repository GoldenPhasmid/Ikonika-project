#pragma once

#include "core/FTImportSource.h"

#include <QString>

#include <memory>

class AppImport
{
public:
    static std::unique_ptr<FTImportSource> Parse(const QString& Path);
};
