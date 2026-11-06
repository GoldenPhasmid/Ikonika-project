#pragma once

#include <catch2/catch_tostring.hpp>

#include <QString>

namespace Catch
{
    template<>
    struct StringMaker<QString>
    {
        static std::string convert(const QString& Value)
        {
            return Value.toStdString();
        }
    };
}
