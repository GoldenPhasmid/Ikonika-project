#pragma once

#include "core/FTReaderDocument.h"
#include "core/FTTypes.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class FTDatabase;

/** One row of the reader library. Exists is computed at read time, not stored. */
struct FTReaderDocumentRow
{
    std::string Id;
    std::string FilePath;
    std::string Title;
    std::string Format;
    FTTimestamp AddedDate = 0;
    FTTimestamp LastOpenedDate = 0;
    int64_t LastPosition = 0;
    double LastZoom = 0.0;
    double LastScrollX = 0.0;
    double LastScrollY = 0.0;
    std::string CloudId;
    int64_t PositionCount = 0;
    int64_t ReadAloudParagraph = -1;
    int64_t ReadAloudCharacter = 0;
    bool bExists = true;
};

/**
 * The book reader's library: every document the user has opened, as a path and a title.
 *
 * A reference only. The file is never copied or imported; extraction happens on open and nothing
 * derived is persisted, so a row is cheap and adding a document is just recording where it is.
 *
 * Local-only. The sync merge leaves this table alone, since a path means nothing on another
 * device. Existence is re-checked against the filesystem on every list, so a file that moved or
 * was deleted shows up as missing rather than silently opening nothing.
 */
class FTReaderRepository
{
public:
    using ClockFunction = std::function<int64_t()>;

    /** An injectable clock for tests, like FTCardRepository's. Defaults to the system clock. */
    explicit FTReaderRepository(FTDatabase& InDatabase, ClockFunction InClock = {});

    /**
     * Records a document the user opened. A path already in the library updates that row's title
     * and format rather than adding a duplicate, so opening the same file twice does not list it
     * twice. Returns the row id, or nothing on failure.
     */
    std::optional<std::string> AddDocument(const std::string& FilePath, const std::string& Title,
                                           const std::string& Format);

    /** Every document that has not been deleted, most recently opened first. */
    std::vector<FTReaderDocumentRow> ListDocuments() const;

    std::optional<FTReaderDocumentRow> FindDocument(const std::string& Id) const;

    /** Soft-deletes one document from the library. The file itself is left alone. */
    bool RemoveDocument(const std::string& Id);

    bool RemoveAllDocuments();

    /** Marks a document as opened just now, moving it to the top of the list. */
    bool TouchDocument(const std::string& Id);

    bool SetPosition(const std::string& Id, int64_t Position);

    bool SetZoom(const std::string& Id, double Zoom);

    bool SetScroll(const std::string& Id, double ScrollX, double ScrollY);

    bool SetCloudId(const std::string& Id, const std::string& CloudId);

    bool SetPositionCount(const std::string& Id, int64_t PositionCount);

    /** Where read aloud last started a sentence, so the next Play can continue there. */
    bool SetReadAloudPlace(const std::string& Id, int64_t Paragraph, int64_t Character);

    bool SetFilePath(const std::string& Id, const std::string& FilePath);

    bool SetTitle(const std::string& Id, const std::string& Title);

    std::string GetLastError() const;

private:
    /** The injectable clock, or the system clock when none was given. */
    int64_t Now() const;

    FTDatabase& Target;
    ClockFunction Clock;
    mutable std::string LastError;
};
