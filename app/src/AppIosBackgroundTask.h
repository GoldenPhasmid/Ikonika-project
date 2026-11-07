#pragma once

#include <QString>

class AppIosBackgroundTask
{
public:
    static unsigned long Begin(const QString& Name);
    static void End(unsigned long Task);
};
