#include "core/FTTsv.h"

#include <sstream>

std::vector<std::vector<std::string>> ParseTsvRows(const std::string& Text)
{
    std::vector<std::vector<std::string>> Rows;

    std::istringstream Stream(Text);
    std::string Line;

    bool bHeaderSkipped = false;

    while (std::getline(Stream, Line))
    {
        if (!Line.empty() && Line.back() == '\r')
        {
            Line.pop_back();
        }

        if (!bHeaderSkipped)
        {
            bHeaderSkipped = true;

            continue;
        }

        if (Line.empty())
        {
            continue;
        }

        std::vector<std::string> Columns;
        std::string Column;

        std::istringstream LineStream(Line);

        while (std::getline(LineStream, Column, '\t'))
        {
            Columns.push_back(Column);
        }

        Rows.push_back(std::move(Columns));
    }

    return Rows;
}
