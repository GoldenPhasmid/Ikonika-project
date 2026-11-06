#include "core/FTReaderRepository.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTUuid.h"

#include <QFileInfo>

namespace
{
    constexpr const char* DocumentColumns =
        "Id, FilePath, Title, Format, AddedDate, LastOpenedDate, LastPosition, LastZoom, LastScrollX, "
        "LastScrollY, CloudId, PositionCount";

    FTReaderDocumentRow ReadRow(const FTStatement& Query)
    {
        FTReaderDocumentRow Row;
        Row.Id = Query.GetText(0);
        Row.FilePath = Query.GetText(1);
        Row.Title = Query.GetText(2);
        Row.Format = Query.GetText(3);
        Row.AddedDate = Query.GetInt64(4);
        Row.LastOpenedDate = Query.GetInt64(5);
        Row.LastPosition = Query.GetInt64(6);
        Row.LastZoom = Query.GetDouble(7);
        Row.LastScrollX = Query.GetDouble(8);
        Row.LastScrollY = Query.GetDouble(9);
        Row.CloudId = Query.GetText(10);
        Row.PositionCount = Query.GetInt64(11);

        Row.bExists = QFileInfo::exists(QString::fromStdString(Row.FilePath));

        return Row;
    }
}

FTReaderRepository::FTReaderRepository(FTDatabase& InDatabase, ClockFunction InClock)
    : Target(InDatabase)
    , Clock(std::move(InClock))
{
}

int64_t FTReaderRepository::Now() const
{
    return Clock ? Clock() : GetSystemTimeMilliseconds();
}

std::optional<std::string> FTReaderRepository::AddDocument(const std::string& FilePath,
                                                           const std::string& Title,
                                                           const std::string& Format)
{
    const FTQueryTimer Timer("FTReaderRepository::AddDocument");

    if (FilePath.empty())
    {
        LastError = "empty file path";

        return std::nullopt;
    }

    // The same file opened twice is the same document. The row is rewritten in place rather than
    // duplicated, so its id survives and a later remove drops both entries at once.
    FTStatement Existing = Target.Prepare(
        "SELECT Id FROM ReaderDocuments WHERE FilePath = ?1 AND Deleted = 0");

    if (!Existing.IsValid())
    {
        LastError = Target.GetLastError();

        return std::nullopt;
    }

    Existing.BindText(1, FilePath);

    if (Existing.Step())
    {
        const std::string Id = Existing.GetText(0);

        FTStatement Update = Target.Prepare(
            "UPDATE ReaderDocuments SET Title = ?1, Format = ?2, LastOpenedDate = ?3 "
            "WHERE Id = ?4");
        Update.BindText(1, Title);
        Update.BindText(2, Format);
        Update.BindInt64(3, Now());
        Update.BindText(4, Id);

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
        "INSERT INTO ReaderDocuments (Id, FilePath, Title, Format, AddedDate, LastOpenedDate) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6)");
    Add.BindText(1, Id);
    Add.BindText(2, FilePath);
    Add.BindText(3, Title);
    Add.BindText(4, Format);
    Add.BindInt64(5, NowTime);
    Add.BindInt64(6, NowTime);

    if (!Add.Run())
    {
        LastError = Target.GetLastError();

        return std::nullopt;
    }

    return Id;
}

std::vector<FTReaderDocumentRow> FTReaderRepository::ListDocuments() const
{
    const FTQueryTimer Timer("FTReaderRepository::ListDocuments");

    std::vector<FTReaderDocumentRow> Result;

    FTStatement Query = Target.Prepare(
        std::string("SELECT ") + DocumentColumns + " "
        "FROM ReaderDocuments WHERE Deleted = 0 ORDER BY LastOpenedDate DESC, AddedDate DESC");

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

std::optional<FTReaderDocumentRow> FTReaderRepository::FindDocument(const std::string& Id) const
{
    FTStatement Query = Target.Prepare(std::string("SELECT ") + DocumentColumns
                                       + " FROM ReaderDocuments WHERE Id = ?1 AND Deleted = 0");

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

bool FTReaderRepository::RemoveDocument(const std::string& Id)
{
    const FTQueryTimer Timer("FTReaderRepository::RemoveDocument");

    FTStatement Remove = Target.Prepare(
        "UPDATE ReaderDocuments SET Deleted = 1, LastOpenedDate = ?2 WHERE Id = ?1");
    Remove.BindText(1, Id);
    Remove.BindInt64(2, Now());

    if (!Remove.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

bool FTReaderRepository::TouchDocument(const std::string& Id)
{
    const FTQueryTimer Timer("FTReaderRepository::TouchDocument");

    FTStatement Touch = Target.Prepare(
        "UPDATE ReaderDocuments SET LastOpenedDate = ?1 WHERE Id = ?2");
    Touch.BindInt64(1, Now());
    Touch.BindText(2, Id);

    if (!Touch.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

bool FTReaderRepository::SetPosition(const std::string& Id, int64_t Position)
{
    const FTQueryTimer Timer("FTReaderRepository::SetPosition");

    FTStatement Update = Target.Prepare(
        "UPDATE ReaderDocuments SET LastPosition = ?1 WHERE Id = ?2");
    Update.BindInt64(1, Position);
    Update.BindText(2, Id);

    if (!Update.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

bool FTReaderRepository::SetZoom(const std::string& Id, double Zoom)
{
    const FTQueryTimer Timer("FTReaderRepository::SetZoom");

    FTStatement Update = Target.Prepare(
        "UPDATE ReaderDocuments SET LastZoom = ?1 WHERE Id = ?2");
    Update.BindDouble(1, Zoom);
    Update.BindText(2, Id);

    if (!Update.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

bool FTReaderRepository::SetScroll(const std::string& Id, double ScrollX, double ScrollY)
{
    const FTQueryTimer Timer("FTReaderRepository::SetScroll");

    FTStatement Update = Target.Prepare(
        "UPDATE ReaderDocuments SET LastScrollX = ?1, LastScrollY = ?2 WHERE Id = ?3");
    Update.BindDouble(1, ScrollX);
    Update.BindDouble(2, ScrollY);
    Update.BindText(3, Id);

    if (!Update.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

bool FTReaderRepository::SetCloudId(const std::string& Id, const std::string& CloudId)
{
    const FTQueryTimer Timer("FTReaderRepository::SetCloudId");

    FTStatement Update = Target.Prepare(
        "UPDATE ReaderDocuments SET CloudId = ?1 WHERE Id = ?2");
    Update.BindText(1, CloudId);
    Update.BindText(2, Id);

    if (!Update.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

bool FTReaderRepository::SetPositionCount(const std::string& Id, int64_t PositionCount)
{
    FTStatement Update = Target.Prepare(
        "UPDATE ReaderDocuments SET PositionCount = ?1 WHERE Id = ?2");
    Update.BindInt64(1, PositionCount);
    Update.BindText(2, Id);

    if (!Update.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

bool FTReaderRepository::SetTitle(const std::string& Id, const std::string& Title)
{
    FTStatement Update = Target.Prepare(
        "UPDATE ReaderDocuments SET Title = ?1 WHERE Id = ?2");
    Update.BindText(1, Title);
    Update.BindText(2, Id);

    if (!Update.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

bool FTReaderRepository::SetFilePath(const std::string& Id, const std::string& FilePath)
{
    const FTQueryTimer Timer("FTReaderRepository::SetFilePath");

    if (FilePath.empty())
    {
        LastError = "empty file path";

        return false;
    }

    FTStatement Update = Target.Prepare(
        "UPDATE ReaderDocuments SET FilePath = ?1 WHERE Id = ?2");
    Update.BindText(1, FilePath);
    Update.BindText(2, Id);

    if (!Update.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

std::string FTReaderRepository::GetLastError() const
{
    return LastError;
}

bool FTReaderRepository::RemoveAllDocuments()
{
    return Target.Execute("DELETE FROM ReaderDocuments");
}
