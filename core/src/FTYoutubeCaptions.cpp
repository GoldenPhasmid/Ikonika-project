#include "core/FTYoutubeCaptions.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>

namespace
{
    std::string::size_type FindMatchingBracket(const std::string& Text,
                                                std::string::size_type OpenPos)
    {
        int Depth = 0;
        bool bInString = false;
        bool bEscaped = false;

        for (std::string::size_type Index = OpenPos; Index < Text.size(); ++Index)
        {
            const char Character = Text[Index];

            if (bEscaped)
            {
                bEscaped = false;

                continue;
            }

            if (Character == '\\')
            {
                bEscaped = true;

                continue;
            }

            if (Character == '"')
            {
                bInString = !bInString;

                continue;
            }

            if (bInString)
            {
                continue;
            }

            if (Character == '[')
            {
                ++Depth;
            }
            else if (Character == ']')
            {
                --Depth;

                if (Depth == 0)
                {
                    return Index;
                }
            }
        }

        return std::string::npos;
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

    std::vector<std::string> SplitLines(const std::string& Text)
    {
        std::vector<std::string> Lines;
        std::string Current;

        for (char Character : Text)
        {
            if (Character == '\n')
            {
                if (!Current.empty() && Current.back() == '\r')
                {
                    Current.pop_back();
                }

                Lines.push_back(Current);
                Current.clear();
            }
            else
            {
                Current += Character;
            }
        }

        Lines.push_back(Current);

        return Lines;
    }

    bool ParseVttTimestamp(const std::string& Text, int64_t& OutMs)
    {
        std::vector<std::string> Parts;
        std::string Current;

        for (char Character : Text)
        {
            if (Character == ':')
            {
                Parts.push_back(Current);
                Current.clear();
            }
            else
            {
                Current += Character;
            }
        }

        Parts.push_back(Current);

        if (Parts.size() < 2 || Parts.size() > 3)
        {
            return false;
        }

        try
        {
            const int64_t Hours = Parts.size() == 3 ? std::stoll(Parts[0]) : 0;
            const int64_t Minutes = std::stoll(Parts[Parts.size() == 3 ? 1 : 0]);
            const double Seconds = std::stod(Parts[Parts.size() == 3 ? 2 : 1]);

            OutMs = Hours * 3600000 + Minutes * 60000
                + static_cast<int64_t>(Seconds * 1000.0 + 0.5);

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::string StripTags(const std::string& Text)
    {
        std::string Result;
        bool bInTag = false;

        for (char Character : Text)
        {
            if (Character == '<')
            {
                bInTag = true;

                continue;
            }

            if (Character == '>')
            {
                bInTag = false;

                continue;
            }

            if (!bInTag)
            {
                Result += Character;
            }
        }

        return Result;
    }
}

std::vector<FTCaptionCue> ParseWebVtt(const std::string& Vtt)
{
    std::vector<FTCaptionCue> Cues;

    const std::vector<std::string> Lines = SplitLines(Vtt);

    for (size_t Index = 0; Index < Lines.size(); ++Index)
    {
        const std::string::size_type ArrowPos = Lines[Index].find("-->");

        if (ArrowPos == std::string::npos)
        {
            continue;
        }

        const std::string StartText = Trim(Lines[Index].substr(0, ArrowPos));
        const std::string AfterArrow = Trim(Lines[Index].substr(ArrowPos + 3));
        const std::string::size_type SpacePos = AfterArrow.find_first_of(" \t");
        const std::string EndText =
            SpacePos == std::string::npos ? AfterArrow : AfterArrow.substr(0, SpacePos);

        int64_t StartMs = 0;
        int64_t EndMs = 0;

        if (!ParseVttTimestamp(StartText, StartMs) || !ParseVttTimestamp(EndText, EndMs))
        {
            continue;
        }

        std::string Text;
        size_t TextIndex = Index + 1;

        while (TextIndex < Lines.size() && !Trim(Lines[TextIndex]).empty())
        {
            if (!Text.empty())
            {
                Text += " ";
            }

            Text += StripTags(Lines[TextIndex]);

            ++TextIndex;
        }

        FTCaptionCue Cue;
        Cue.StartMs = StartMs;
        Cue.DurationMs = std::max<int64_t>(0, EndMs - StartMs);
        Cue.Text = Trim(Text);

        if (!Cue.Text.empty())
        {
            Cues.push_back(std::move(Cue));
        }

        Index = TextIndex;
    }

    return Cues;
}

std::vector<FTCaptionTrack> ParseCaptionTrackList(const std::string& WatchPageHtml)
{
    std::vector<FTCaptionTrack> Tracks;

    const std::string Marker = "\"captionTracks\":[";
    const std::string::size_type MarkerPos = WatchPageHtml.find(Marker);

    if (MarkerPos == std::string::npos)
    {
        return Tracks;
    }

    const std::string::size_type OpenBracket = MarkerPos + Marker.size() - 1;
    const std::string::size_type CloseBracket = FindMatchingBracket(WatchPageHtml, OpenBracket);

    if (CloseBracket == std::string::npos)
    {
        return Tracks;
    }

    const std::string ArrayJson =
        WatchPageHtml.substr(OpenBracket, CloseBracket - OpenBracket + 1);

    const QJsonDocument Document =
        QJsonDocument::fromJson(QByteArray::fromStdString(ArrayJson));

    if (!Document.isArray())
    {
        return Tracks;
    }

    for (const QJsonValue& Value : Document.array())
    {
        const QJsonObject Object = Value.toObject();

        FTCaptionTrack Track;
        Track.BaseUrl = Object.value(QLatin1String("baseUrl")).toString().toStdString();
        Track.LanguageCode = Object.value(QLatin1String("languageCode")).toString().toStdString();
        Track.bAutoGenerated =
            Object.value(QLatin1String("kind")).toString() == QLatin1String("asr");

        if (!Track.BaseUrl.empty())
        {
            Tracks.push_back(std::move(Track));
        }
    }

    return Tracks;
}

const FTCaptionTrack* PickCaptionTrack(const std::vector<FTCaptionTrack>& Tracks,
                                       const std::string& PreferredLanguage)
{
    const FTCaptionTrack* ManualMatch = nullptr;
    const FTCaptionTrack* AutoMatch = nullptr;
    const FTCaptionTrack* FirstManual = nullptr;

    for (const FTCaptionTrack& Track : Tracks)
    {
        if (Track.LanguageCode == PreferredLanguage)
        {
            if (Track.bAutoGenerated)
            {
                AutoMatch = AutoMatch != nullptr ? AutoMatch : &Track;
            }
            else
            {
                ManualMatch = ManualMatch != nullptr ? ManualMatch : &Track;
            }
        }
        else if (!Track.bAutoGenerated && FirstManual == nullptr)
        {
            FirstManual = &Track;
        }
    }

    if (ManualMatch != nullptr)
    {
        return ManualMatch;
    }

    if (AutoMatch != nullptr)
    {
        return AutoMatch;
    }

    if (FirstManual != nullptr)
    {
        return FirstManual;
    }

    return Tracks.empty() ? nullptr : &Tracks.front();
}
