#pragma once

#include "core/FTTypes.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class FTDatabase;
class FTStatement;

struct FTBookCollectionRow
{
    std::string Id;
    std::string Name;
    int Colour = 0;
    FTTimestamp CreatedDate = 0;
};

struct FTBookCollectionMember
{
    std::string CollectionId;
    std::string BookKey;
};

class FTBookShelfRepository
{
public:
    using ClockFunction = std::function<int64_t()>;

    explicit FTBookShelfRepository(FTDatabase& InDatabase, ClockFunction InClock = {});

    std::unordered_set<std::string> ListFavorites() const;

    bool SetFavorite(const std::string& BookKey, bool bFavorite);

    std::vector<FTBookCollectionRow> ListCollections() const;

    std::vector<FTBookCollectionMember> ListMembers() const;

    std::optional<std::string> CreateCollection(const std::string& Name, int Colour);

    bool RenameCollection(const std::string& Id, const std::string& Name);

    bool DeleteCollection(const std::string& Id);

    bool AddToCollection(const std::string& CollectionId, const std::string& BookKey);

    bool RemoveFromCollection(const std::string& CollectionId, const std::string& BookKey);

    bool RenameBookKey(const std::string& OldKey, const std::string& NewKey);

    bool AddBookCard(const std::string& BookKey, const std::string& CardId);

    std::vector<std::string> ListBookCards(const std::string& BookKey) const;

    std::unordered_map<std::string, int> CountBookCards() const;

    std::string GetLastError() const;

private:
    int64_t Now() const;

    bool Run(const char* Label, const std::string& Sql, const std::function<void(FTStatement&)>& Bind);

    FTDatabase& Target;
    ClockFunction Clock;
    mutable std::string LastError;
};
