#pragma once

#include "core/FTTypes.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class FTDatabase;

struct FTCloudBookRow
{
    std::string Id;
    std::string Title;
    std::string Format;
    int64_t FileSize = 0;
    std::string DriveFileId;
    FTTimestamp AddedDate = 0;
    FTTimestamp LastOpenedDate = 0;
    int64_t LastPosition = 0;
    double LastZoom = 0.0;
    double LastScrollX = 0.0;
    double LastScrollY = 0.0;
    int64_t PositionCount = 0;
    FTTimestamp LastUpdatedDate = 0;
};

struct FTReadingPlace
{
    int64_t Position = 0;
    double Zoom = 0.0;
    double ScrollX = 0.0;
    double ScrollY = 0.0;
    int64_t PositionCount = 0;
};

class FTCloudBookRepository
{
public:
    using ClockFunction = std::function<int64_t()>;

    explicit FTCloudBookRepository(FTDatabase& InDatabase, ClockFunction InClock = {});

    std::optional<std::string> AddBook(const std::string& Title, const std::string& Format, int64_t FileSize,
                                       const std::string& DriveFileId, const FTReadingPlace& Place);

    std::vector<FTCloudBookRow> ListBooks() const;

    std::vector<std::string> ListDeletedIds() const;

    std::optional<FTCloudBookRow> FindBook(const std::string& Id) const;

    bool RemoveBook(const std::string& Id);

    bool SetReadingPlace(const std::string& Id, const FTReadingPlace& Place);

    std::string GetLastError() const;

private:
    int64_t Now() const;

    FTDatabase& Target;
    ClockFunction Clock;
    mutable std::string LastError;
};
