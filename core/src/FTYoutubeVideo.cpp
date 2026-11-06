#include "core/FTYoutubeVideo.h"

namespace
{
    bool IsIdCharacter(char Character)
    {
        return (Character >= 'A' && Character <= 'Z')
            || (Character >= 'a' && Character <= 'z')
            || (Character >= '0' && Character <= '9')
            || Character == '_' || Character == '-';
    }

    /** YouTube ids are always exactly 11 characters, so a run of id characters that comes out
        shorter or longer than that is not one. */
    bool IsValidId(const std::string& Candidate)
    {
        if (Candidate.size() != 11)
        {
            return false;
        }

        for (char Character : Candidate)
        {
            if (!IsIdCharacter(Character))
            {
                return false;
            }
        }

        return true;
    }

    /** Every id character starting at Start, stopping at the first one that is not - '&', '?',
        '#', or the end of the string. */
    std::string TakeCandidate(const std::string& Text, std::string::size_type Start)
    {
        std::string::size_type End = Start;

        while (End < Text.size() && IsIdCharacter(Text[End]))
        {
            ++End;
        }

        return Text.substr(Start, End - Start);
    }

    std::string Trim(const std::string& Text)
    {
        const std::string::size_type Start = Text.find_first_not_of(" \t\r\n");

        if (Start == std::string::npos)
        {
            return {};
        }

        const std::string::size_type End = Text.find_last_not_of(" \t\r\n");

        return Text.substr(Start, End - Start + 1);
    }
}

std::optional<std::string> ExtractYoutubeVideoId(const std::string& UrlOrId)
{
    const std::string Trimmed = Trim(UrlOrId);

    if (IsValidId(Trimmed))
    {
        return Trimmed;
    }

    // Checked in this order because "v=" is the one marker generic enough to appear by accident
    // elsewhere in a URL; the three path markers are specific enough to trust immediately, and are
    // tried first for that reason.
    for (const std::string& Marker : {std::string("youtu.be/"), std::string("/embed/"),
                                       std::string("/shorts/"), std::string("v=")})
    {
        const std::string::size_type Found = Trimmed.find(Marker);

        if (Found == std::string::npos)
        {
            continue;
        }

        const std::string Candidate = TakeCandidate(Trimmed, Found + Marker.size());

        if (IsValidId(Candidate))
        {
            return Candidate;
        }
    }

    return std::nullopt;
}
