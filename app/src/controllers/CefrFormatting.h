#pragma once

#include <QString>

#include <optional>

inline QString CefrLetter(const std::optional<int>& Level)
{
    constexpr const char* CefrLetters[] = { "A1", "A2", "B1", "B2", "C1", "C2" };

    if (!Level.has_value() || *Level < 0 || *Level >= 6)
    {
        return {};
    }

    return QString::fromUtf8(CefrLetters[*Level]);
}
