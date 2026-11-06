#pragma once

#include "core/FTTypes.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class FTDatabase;

/** One row of the YouTube library. */
struct FTYoutubeVideoRow
{
    std::string Id;
    std::string VideoId;
    std::string Title;
    FTTimestamp AddedDate = 0;
    FTTimestamp LastOpenedDate = 0;
};

/**
 * The YouTube reader's library: every video the user has pasted a link to. The same shape as
 * FTReaderRepository - a reference kept for the video id and a title, nothing about the video or
 * its captions stored alongside it.
 *
 * Local-only. The sync merge leaves this table alone, the same reasoning as the book reader's
 * library: nothing here is meaningful to reproduce on another device by copying the row alone,
 * and a video id needs no existence check the way a file path does.
 */
class FTYoutubeRepository
{
public:
    using ClockFunction = std::function<int64_t()>;

    /** An injectable clock for tests, like FTReaderRepository's. Defaults to the system clock. */
    explicit FTYoutubeRepository(FTDatabase& InDatabase, ClockFunction InClock = {});

    /**
     * Records a video the user opened. A video id already in the library updates that row's title
     * rather than adding a duplicate, so pasting the same link twice does not list it twice.
     * Returns the row id, or nothing on failure.
     */
    std::optional<std::string> AddVideo(const std::string& VideoId, const std::string& Title);

    /** Every video that has not been deleted, most recently opened first. */
    std::vector<FTYoutubeVideoRow> ListVideos() const;

    /** Soft-deletes one video from the library. */
    bool RemoveVideo(const std::string& Id);

    /** Marks a video as opened just now, moving it to the top of the list. */
    bool TouchVideo(const std::string& Id);

    std::string GetLastError() const;

private:
    /** The injectable clock, or the system clock when none was given. */
    int64_t Now() const;

    FTDatabase& Target;
    ClockFunction Clock;
    mutable std::string LastError;
};
