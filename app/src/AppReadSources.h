#pragma once

#include "core/FTCardRepository.h"
#include "core/FTDatabase.h"
#include "core/FTDictionaryRepository.h"

#include <memory>
#include <optional>
#include <string>

struct FReadSourcesRequest
{
    std::string UserDatabasePath;
    std::string DictionaryPath;
    std::string TargetLanguage;
    std::string NativeLanguage;
};

/**
 * Read-only connections to the user database and the open dictionary, owned by one background task. A connection
 * belongs to the thread that opened it, so a task captures what it needs on the main thread with Capture and opens its
 * own connections with Open on its own thread.
 */
class AppReadSources
{
public:
    /** Nothing when the user database or the dictionary has no file to reopen, which is the case for an in-memory one. */
    static std::optional<FReadSourcesRequest> Capture();

    static std::unique_ptr<AppReadSources> Open(const FReadSourcesRequest& Request);

    FTDatabase& GetDictionaryConnection();
    const FTDictionaryRepository& GetDictionary() const;
    const FTCardRepository& GetCards() const;

private:
    AppReadSources() = default;

    std::optional<FTDatabase> UserConnection;
    std::optional<FTDatabase> DictionaryConnection;
    std::unique_ptr<FTCardRepository> Cards;
    std::unique_ptr<FTDictionaryRepository> Dictionary;
};
