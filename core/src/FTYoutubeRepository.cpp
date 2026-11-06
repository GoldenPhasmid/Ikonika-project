#include "core/FTYoutubeRepository.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTUuid.h"

namespace
{
    FTYoutubeVideoRow ReadRow(const FTStatement& Query)
    {
        FTYoutubeVideoRow Row;
        Row.Id = Query.GetText(0);
        Row.VideoId = Query.GetText(1);
        Row.Title = Query.GetText(2);
        Row.AddedDate = Query.GetInt64(3);
        Row.LastOpenedDate = Query.GetInt64(4);

        return Row;
    }
}

FTYoutubeRepository::FTYoutubeRepository(FTDatabase& InDatabase, ClockFunction InClock)
    : Target(InDatabase)
    , Clock(std::move(InClock))
{
}

int64_t FTYoutubeRepository::Now() const
{
    return Clock ? Clock() : GetSystemTimeMilliseconds();
}

std::optional<std::string> FTYoutubeRepository::AddVideo(const std::string& VideoId,
                                                          const std::string& Title)
{
    const FTQueryTimer Timer("FTYoutubeRepository::AddVideo");

    if (VideoId.empty())
    {
        LastError = "empty video id";

        return std::nullopt;
    }

    // The same video pasted twice is the same row. Rewritten in place rather than duplicated, so
    // its id survives and a later remove drops both entries at once.
    FTStatement Existing = Target.Prepare(
        "SELECT Id FROM YoutubeVideos WHERE VideoId = ?1 AND Deleted = 0");

    if (!Existing.IsValid())
    {
        LastError = Target.GetLastError();

        return std::nullopt;
    }

    Existing.BindText(1, VideoId);

    if (Existing.Step())
    {
        const std::string Id = Existing.GetText(0);

        FTStatement Update = Target.Prepare(
            "UPDATE YoutubeVideos SET Title = ?1, LastOpenedDate = ?2 WHERE Id = ?3");
        Update.BindText(1, Title);
        Update.BindInt64(2, Now());
        Update.BindText(3, Id);

        if (!Update.Run())
        {
            LastError = Target.GetLastError();

            return std::nullopt;
        }

        return Id;
    }

    const std::string Id = MakeUuid();
    const int64_t NowTime = Now();

    FTStatement Add = Target.Prepare(
        "INSERT INTO YoutubeVideos (Id, VideoId, Title, AddedDate, LastOpenedDate) "
        "VALUES (?1, ?2, ?3, ?4, ?5)");
    Add.BindText(1, Id);
    Add.BindText(2, VideoId);
    Add.BindText(3, Title);
    Add.BindInt64(4, NowTime);
    Add.BindInt64(5, NowTime);

    if (!Add.Run())
    {
        LastError = Target.GetLastError();

        return std::nullopt;
    }

    return Id;
}

std::vector<FTYoutubeVideoRow> FTYoutubeRepository::ListVideos() const
{
    const FTQueryTimer Timer("FTYoutubeRepository::ListVideos");

    std::vector<FTYoutubeVideoRow> Result;

    FTStatement Query = Target.Prepare(
        "SELECT Id, VideoId, Title, AddedDate, LastOpenedDate "
        "FROM YoutubeVideos WHERE Deleted = 0 ORDER BY LastOpenedDate DESC, AddedDate DESC");

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return Result;
    }

    while (Query.Step())
    {
        Result.push_back(ReadRow(Query));
    }

    return Result;
}

bool FTYoutubeRepository::RemoveVideo(const std::string& Id)
{
    const FTQueryTimer Timer("FTYoutubeRepository::RemoveVideo");

    FTStatement Remove = Target.Prepare("UPDATE YoutubeVideos SET Deleted = 1 WHERE Id = ?1");
    Remove.BindText(1, Id);

    if (!Remove.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

bool FTYoutubeRepository::TouchVideo(const std::string& Id)
{
    const FTQueryTimer Timer("FTYoutubeRepository::TouchVideo");

    FTStatement Touch = Target.Prepare("UPDATE YoutubeVideos SET LastOpenedDate = ?1 WHERE Id = ?2");
    Touch.BindInt64(1, Now());
    Touch.BindText(2, Id);

    if (!Touch.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

std::string FTYoutubeRepository::GetLastError() const
{
    return LastError;
}
