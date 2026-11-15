#include "core/FTBookShelfRepository.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTUuid.h"

FTBookShelfRepository::FTBookShelfRepository(FTDatabase& InDatabase, ClockFunction InClock)
    : Target(InDatabase)
    , Clock(std::move(InClock))
{
}

int64_t FTBookShelfRepository::Now() const
{
    return Clock ? Clock() : GetSystemTimeMilliseconds();
}

bool FTBookShelfRepository::Run(const char* Label, const std::string& Sql,
                                const std::function<void(FTStatement&)>& Bind)
{
    const FTQueryTimer Timer(Label);

    FTStatement Statement{Target.Prepare(Sql)};

    if (!Statement.IsValid())
    {
        LastError = Target.GetLastError();

        return false;
    }

    Bind(Statement);

    if (!Statement.Run())
    {
        LastError = Target.GetLastError();

        return false;
    }

    return true;
}

std::unordered_set<std::string> FTBookShelfRepository::ListFavorites() const
{
    const FTQueryTimer Timer("FTBookShelfRepository::ListFavorites");

    std::unordered_set<std::string> Result;

    FTStatement Query{Target.Prepare("SELECT BookKey FROM FavoriteBooks WHERE Favorite = 1")};

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return Result;
    }

    while (Query.Step())
    {
        Result.insert(Query.GetText(0));
    }

    return Result;
}

bool FTBookShelfRepository::SetFavorite(const std::string& BookKey, bool bFavorite)
{
    return Run("FTBookShelfRepository::SetFavorite",
               "INSERT INTO FavoriteBooks (BookKey, Favorite, LastUpdatedDate) VALUES (?1, ?2, ?3) "
               "ON CONFLICT (BookKey) DO UPDATE SET Favorite = excluded.Favorite, "
               "                                    LastUpdatedDate = excluded.LastUpdatedDate",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, BookKey);
                   Statement.BindInt(2, bFavorite ? 1 : 0);
                   Statement.BindInt64(3, Now());
               });
}

std::vector<FTBookCollectionRow> FTBookShelfRepository::ListCollections() const
{
    const FTQueryTimer Timer("FTBookShelfRepository::ListCollections");

    std::vector<FTBookCollectionRow> Result;

    FTStatement Query{Target.Prepare(
        "SELECT Id, Name, Colour, CreatedDate FROM BookCollections WHERE Deleted = 0 "
        "ORDER BY Name COLLATE NOCASE, CreatedDate")};

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return Result;
    }

    while (Query.Step())
    {
        Result.push_back(FTBookCollectionRow{Query.GetText(0), Query.GetText(1), Query.GetInt(2), Query.GetInt64(3)});
    }

    return Result;
}

std::vector<FTBookCollectionMember> FTBookShelfRepository::ListMembers() const
{
    const FTQueryTimer Timer("FTBookShelfRepository::ListMembers");

    std::vector<FTBookCollectionMember> Result;

    FTStatement Query{Target.Prepare(
        "SELECT M.CollectionId, M.BookKey FROM BookCollectionMembers M "
        "JOIN BookCollections C ON C.Id = M.CollectionId "
        "WHERE M.Deleted = 0 AND C.Deleted = 0")};

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return Result;
    }

    while (Query.Step())
    {
        Result.push_back(FTBookCollectionMember{Query.GetText(0), Query.GetText(1)});
    }

    return Result;
}

std::optional<std::string> FTBookShelfRepository::CreateCollection(const std::string& Name, int Colour)
{
    if (Name.empty())
    {
        LastError = "empty collection name";

        return std::nullopt;
    }

    const std::string Id{MakeUuid()};
    const int64_t NowTime{Now()};

    const bool bCreated = Run("FTBookShelfRepository::CreateCollection",
                              "INSERT INTO BookCollections (Id, Name, Colour, CreatedDate, LastUpdatedDate) "
                              "VALUES (?1, ?2, ?3, ?4, ?4)",
                              [&](FTStatement& Statement)
                              {
                                  Statement.BindText(1, Id);
                                  Statement.BindText(2, Name);
                                  Statement.BindInt(3, Colour);
                                  Statement.BindInt64(4, NowTime);
                              });

    if (!bCreated)
    {
        return std::nullopt;
    }

    return Id;
}

bool FTBookShelfRepository::RenameCollection(const std::string& Id, const std::string& Name)
{
    if (Name.empty())
    {
        LastError = "empty collection name";

        return false;
    }

    return Run("FTBookShelfRepository::RenameCollection",
               "UPDATE BookCollections SET Name = ?2, LastUpdatedDate = ?3 WHERE Id = ?1 AND Deleted = 0",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, Id);
                   Statement.BindText(2, Name);
                   Statement.BindInt64(3, Now());
               });
}

bool FTBookShelfRepository::DeleteCollection(const std::string& Id)
{
    const int64_t NowTime{Now()};

    return Run("FTBookShelfRepository::DeleteCollection",
               "UPDATE BookCollections SET Deleted = 1, LastUpdatedDate = ?2 WHERE Id = ?1",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, Id);
                   Statement.BindInt64(2, NowTime);
               })
        && Run("FTBookShelfRepository::DeleteCollectionMembers",
               "UPDATE BookCollectionMembers SET Deleted = 1, LastUpdatedDate = ?2 "
               "WHERE CollectionId = ?1 AND Deleted = 0",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, Id);
                   Statement.BindInt64(2, NowTime);
               });
}

bool FTBookShelfRepository::AddToCollection(const std::string& CollectionId, const std::string& BookKey)
{
    return Run("FTBookShelfRepository::AddToCollection",
               "INSERT INTO BookCollectionMembers (CollectionId, BookKey, LastUpdatedDate, Deleted) "
               "VALUES (?1, ?2, ?3, 0) "
               "ON CONFLICT (CollectionId, BookKey) DO UPDATE SET Deleted = 0, "
               "                                                  LastUpdatedDate = excluded.LastUpdatedDate",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, CollectionId);
                   Statement.BindText(2, BookKey);
                   Statement.BindInt64(3, Now());
               });
}

bool FTBookShelfRepository::RemoveFromCollection(const std::string& CollectionId, const std::string& BookKey)
{
    return Run("FTBookShelfRepository::RemoveFromCollection",
               "UPDATE BookCollectionMembers SET Deleted = 1, LastUpdatedDate = ?3 "
               "WHERE CollectionId = ?1 AND BookKey = ?2",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, CollectionId);
                   Statement.BindText(2, BookKey);
                   Statement.BindInt64(3, Now());
               });
}

bool FTBookShelfRepository::RenameBookKey(const std::string& OldKey, const std::string& NewKey)
{
    if (OldKey == NewKey)
    {
        return true;
    }

    const int64_t NowTime{Now()};

    const auto BindBoth = [&](FTStatement& Statement)
    {
        Statement.BindText(1, OldKey);
        Statement.BindText(2, NewKey);
        Statement.BindInt64(3, NowTime);
    };

    const auto BindOld = [&](FTStatement& Statement)
    {
        Statement.BindText(1, OldKey);
        Statement.BindInt64(2, NowTime);
    };

    return Run("FTBookShelfRepository::MoveFavorite",
               "INSERT INTO FavoriteBooks (BookKey, Favorite, LastUpdatedDate) "
               "SELECT ?2, Favorite, ?3 FROM FavoriteBooks WHERE BookKey = ?1 AND Favorite = 1 "
               "ON CONFLICT (BookKey) DO UPDATE SET Favorite = 1, LastUpdatedDate = excluded.LastUpdatedDate",
               BindBoth)
        && Run("FTBookShelfRepository::DropOldFavorite",
               "UPDATE FavoriteBooks SET Favorite = 0, LastUpdatedDate = ?2 WHERE BookKey = ?1 AND Favorite = 1",
               BindOld)
        && Run("FTBookShelfRepository::MoveMembers",
               "INSERT INTO BookCollectionMembers (CollectionId, BookKey, LastUpdatedDate, Deleted) "
               "SELECT CollectionId, ?2, ?3, 0 FROM BookCollectionMembers WHERE BookKey = ?1 AND Deleted = 0 "
               "ON CONFLICT (CollectionId, BookKey) DO UPDATE SET Deleted = 0, "
               "                                                  LastUpdatedDate = excluded.LastUpdatedDate",
               BindBoth)
        && Run("FTBookShelfRepository::DropOldMembers",
               "UPDATE BookCollectionMembers SET Deleted = 1, LastUpdatedDate = ?2 "
               "WHERE BookKey = ?1 AND Deleted = 0",
               BindOld)
        && Run("FTBookShelfRepository::MoveCards",
               "UPDATE BookCards SET BookKey = ?2, LastUpdatedDate = ?3 WHERE BookKey = ?1",
               BindBoth);
}

bool FTBookShelfRepository::AddBookCard(const std::string& BookKey, const std::string& CardId)
{
    const int64_t NowTime{Now()};

    return Run("FTBookShelfRepository::AddBookCard",
               "INSERT INTO BookCards (CardId, BookKey, AddedDate, LastUpdatedDate) VALUES (?1, ?2, ?3, ?3) "
               "ON CONFLICT (CardId) DO NOTHING",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, CardId);
                   Statement.BindText(2, BookKey);
                   Statement.BindInt64(3, NowTime);
               });
}

std::vector<std::string> FTBookShelfRepository::ListBookCards(const std::string& BookKey) const
{
    const FTQueryTimer Timer("FTBookShelfRepository::ListBookCards");

    std::vector<std::string> Result;

    FTStatement Query{Target.Prepare(
        "SELECT BookCards.CardId FROM BookCards JOIN Cards ON Cards.Id = BookCards.CardId "
        "WHERE BookCards.BookKey = ?1 AND Cards.Deleted = 0 "
        "ORDER BY BookCards.AddedDate DESC")};

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return Result;
    }

    Query.BindText(1, BookKey);

    while (Query.Step())
    {
        Result.push_back(Query.GetText(0));
    }

    return Result;
}

std::unordered_map<std::string, int> FTBookShelfRepository::CountBookCards() const
{
    const FTQueryTimer Timer("FTBookShelfRepository::CountBookCards");

    std::unordered_map<std::string, int> Result;

    FTStatement Query{Target.Prepare(
        "SELECT BookCards.BookKey, COUNT(*) FROM BookCards JOIN Cards ON Cards.Id = BookCards.CardId "
        "WHERE Cards.Deleted = 0 GROUP BY BookCards.BookKey")};

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return Result;
    }

    while (Query.Step())
    {
        Result[Query.GetText(0)] = Query.GetInt(1);
    }

    return Result;
}

std::string FTBookShelfRepository::GetLastError() const
{
    return LastError;
}
