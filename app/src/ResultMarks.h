#pragma once

#include <QVariantList>

#include <vector>

inline QVariantList ToResultMarks(const std::vector<bool>& Results)
{
    QVariantList Marks;

    for (const bool bCorrect : Results)
    {
        Marks.append(bCorrect ? 1.0 : 0.0);
    }

    return Marks;
}
