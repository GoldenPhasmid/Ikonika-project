#include "core/FTCloudBookRepository.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTUuid.h"

namespace
{
    constexpr const char* BookColumns =
        "Id, Title, Format, FileSize, DriveFileId, AddedDate, LastOpenedDate, LastPosition, LastZoom, "
        "LastScrollX, LastScrollY, LastUpdatedDate, PositionCount";

    FTCloudBookRow ReadRow(const FTStatement& Query)
    {
        FTCloudBookRow Row;
        Row.Id = Query.GetText(0);
        Row.Title = Query.GetText(1);
        Row.Format = Query.GetText(2);
        Row.FileSize = Query.GetInt64(3);
        Row.DriveFileId = Query.GetText(4);
        Row.AddedDate = Query.GetInt64(5);
        Row.LastOpenedDate = Query.GetInt64(6);
        Row.LastPosition = Query.GetInt64(7);
        Row.LastZoom = Query.GetDouble(8);
        Row.LastScrollX = Query.GetDouble(9);
        Row.LastScrollY = Query.GetDouble(10);
        Row.LastUpdatedDate = Query.GetInt64(11);
        Row.PositionCount = Query.GetInt64(12);

        return Row;
    }
}

FTCloudBookRepository::FTCloudBookRepository(FTDatabase& InDatabase, ClockFunction InClock)
    : Target(InDatabase)
    , Clock(std::move(InClock))
{
}

int64_t FTCloudBookRepository::Now() const
{
    return Clock ? Clock() : GetSystemTimeMilliseconds();
}

std::optional<std::string> FTCloudBookRepository::AddBook(const std::string& Title, const std::string& Format,
                                                          int64_t FileSize, const std::string& DriveFileId,
                                                          const FTReadingPlace& Place)
{
    const FTQueryTimer Timer("FTCloudBookRepository::AddBook");

    if (DriveFileId.empty())
    {
        LastError = "empty Drive file id";

        return std::nullopt;
    }

    const std::string Id = MakeUuid();
    const int64_t NowTime = Now();

    FTStatement Add = Target.Prepare(
        "INSERT INTO CloudBooks (Id, Title, Format, FileSize, DriveFileId, AddedDate, LastOpenedDate, "
        "                        LastPosition, LastZoom, LastScrollX, LastScrollY, LastUpdatedDate, PositionCount) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?6, ?7, ?8, ?9, ?10, ?6, ?11)");
    Add.BindText(1, Id);
    Add.BindText(2, Title);
    Add.BindText(3, Format);
    Add.BindInt64(4, FileSize);
    Add.BindText(5, DriveFileId);
    Add.BindInt64(6, NowTime);
    Add.BindInt64(7, Place.Position);
    Add.BindDouble(8, Place.Zoom);
    Add.BindDouble(9, Place.ScrollX);
    Add.BindDouble(10, Place.ScrollY);
    Add.BindInt64(11, Place.PositionCount);

    if (!Add.Run())
    {
        LastError = Target.GetLastError();

        return std::nullopt;
    }

    return Id;
}

std::vector<FTCloudBookRow> FTCloudBookRepository::ListBooks() const
{
    const FTQueryTimer Timer("FTCloudBookRepository::ListBooks");

    std::vector<FTCloudBookRow> Result;

    FTStatement Query = Target.Prepare(std::string("SELECT ") + BookColumns
                                       + " FROM CloudBooks WHERE Deleted = 0 ORDER BY LastOpenedDate DESC");

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

std::vector<std::string> FTCloudBookRepository::ListDeletedIds() const
{
    std::vector<std::string> Result;

    FTStatement Query = Target.Prepare("SELECT Id FROM CloudBooks WHERE Deleted = 1");

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return Result;
    }

    while (Query.Step())
    {
        Result.push_back(Query.GetText(0));
    }

    return Result;
}

std::optional<FTCloudBookRow> FTCloudBookRepository::FindBook(const std::string& Id) const
{
    FTStatement Query = Target.Prepare(std::string("SELECT ") + BookColumns
                                       + " FROM CloudBooks WHERE Id = ?1 AND Deleted = 0");

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return std::nullopt;
    }

    Query.BindText(1, Id);

    if (!Query.Step())
    {
        return std::nullopt;
    }

    return ReadRow(Query);
}

bool FTCloudBookRepository::RemoveBook(const std::string& Id)
{
    const FTQueryTimer Timer("FTCloudBookRepository::RemoveBook");

    FTStatement Remove = Target.Prepare(
        "UPDATE CloudBooks SET Deleted = 1, LastUpdatedDate = ?2 WHERE Id = ?1");
    Remove.BindText(1, Id);
    Remove.BindInt64(2, Now());

    if (!Remove.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

bool FTCloudBookRepository::SetReadingPlace(const std::string& Id, const FTReadingPlace& Place)
{
    const FTQueryTimer Timer("FTCloudBookRepository::SetReadingPlace");

    FTStatement Update = Target.Prepare(
        "UPDATE CloudBooks SET LastPosition = ?1, LastZoom = ?2, LastScrollX = ?3, LastScrollY = ?4, "
        "                      LastOpenedDate = ?5, LastUpdatedDate = ?5, "
        "                      PositionCount = CASE WHEN ?7 > 0 THEN ?7 ELSE PositionCount END "
        "WHERE Id = ?6 AND Deleted = 0");
    Update.BindInt64(1, Place.Position);
    Update.BindDouble(2, Place.Zoom);
    Update.BindDouble(3, Place.ScrollX);
    Update.BindDouble(4, Place.ScrollY);
    Update.BindInt64(5, Now());
    Update.BindText(6, Id);
    Update.BindInt64(7, Place.PositionCount);

    if (!Update.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

std::string FTCloudBookRepository::GetLastError() const
{
    return LastError;
}
