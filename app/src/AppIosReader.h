#pragma once

#include <QString>

#include <functional>

class AppIosReader
{
public:
    static void PickDocument(std::function<void(const QString&)> OnPicked);
    static void SetKeepScreenOn(bool bEnabled);
};
