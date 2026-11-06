#pragma once

#include <QString>

class AppIosStorage
{
public:
    static void ExcludeFromBackup(const QString& Directory);
};
