#include "core/FTUserDeckRepository.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTUuid.h"

namespace
{
    constexpr int TargetLanguageParameter = 90;
    constexpr int NativeLanguageParameter = 91;
}

FTUserDeckRepository::FTUserDeckRepository(FTDatabase& InDatabase, ClockFunction InClock)
    : Target(InDatabase)
    , Clock(std::move(InClock))
{
}

void FTUserDeckRepository::SetActiveLanguages(const std::string& InTargetLanguage, const std::string& InNativeLanguage)
{
    Pair.Set(InTargetLanguage, InNativeLanguage);
}

int64_t FTUserDeckRepository::Now() const
{
    return Clock ? Clock() : GetSystemTimeMilliseconds();
}

bool FTUserDeckRepository::Run(const char* Label, const std::string& Sql,
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

std::vector<FTUserDeckCategoryRow> FTUserDeckRepository::ListCategories() const
{
    const FTQueryTimer Timer("FTUserDeckRepository::ListCategories");

    std::vector<FTUserDeckCategoryRow> Result;

    FTStatement Query{Target.Prepare(
        "SELECT Id, Name, Colour FROM UserDeckCategories "
        "WHERE Deleted = 0 AND " + Pair.Clause("UserDeckCategories") + " "
        "ORDER BY CreatedDate, Name COLLATE NOCASE")};

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return Result;
    }

    Pair.Bind(Query);

    while (Query.Step())
    {
        Result.push_back(FTUserDeckCategoryRow{Query.GetText(0), Query.GetText(1), Query.GetInt(2)});
    }

    return Result;
}

std::optional<std::string> FTUserDeckRepository::CreateCategory(const std::string& Name, int Colour)
{
    if (Name.empty() || !Pair.IsSet())
    {
        LastError = Name.empty() ? "empty category name" : "no active language pair";

        return std::nullopt;
    }

    const std::string Id{MakeUuid()};
    const int64_t NowTime{Now()};

    const bool bCreated = Run("FTUserDeckRepository::CreateCategory",
                              "INSERT INTO UserDeckCategories (Id, Name, Colour, TargetLanguage, NativeLanguage, "
                              "                                CreatedDate, LastUpdatedDate) "
                              "VALUES (?1, ?2, ?3, ?90, ?91, ?4, ?4)",
                              [&](FTStatement& Statement)
                              {
                                  Statement.BindText(1, Id);
                                  Statement.BindText(2, Name);
                                  Statement.BindInt(3, Colour);
                                  Statement.BindInt64(4, NowTime);
                                  Statement.BindText(TargetLanguageParameter, Pair.GetTargetLanguage());
                                  Statement.BindText(NativeLanguageParameter, Pair.GetNativeLanguage());
                              });

    if (!bCreated)
    {
        return std::nullopt;
    }

    return Id;
}

bool FTUserDeckRepository::RenameCategory(const std::string& Id, const std::string& Name)
{
    if (Name.empty())
    {
        LastError = "empty category name";

        return false;
    }

    return Run("FTUserDeckRepository::RenameCategory",
               "UPDATE UserDeckCategories SET Name = ?2, LastUpdatedDate = ?3 WHERE Id = ?1 AND Deleted = 0",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, Id);
                   Statement.BindText(2, Name);
                   Statement.BindInt64(3, Now());
               });
}

bool FTUserDeckRepository::DeleteCategory(const std::string& Id)
{
    const int64_t NowTime{Now()};

    const auto Bind = [&](FTStatement& Statement)
    {
        Statement.BindText(1, Id);
        Statement.BindInt64(2, NowTime);
    };

    return Run("FTUserDeckRepository::DeleteCategory",
               "UPDATE UserDeckCategories SET Deleted = 1, LastUpdatedDate = ?2 WHERE Id = ?1",
               Bind)
        && Run("FTUserDeckRepository::ReleaseCategoryDecks",
               "UPDATE UserDecks SET CategoryId = '', LastUpdatedDate = ?2 WHERE CategoryId = ?1 AND Deleted = 0",
               Bind);
}

std::vector<FTUserDeckRow> FTUserDeckRepository::ListDecks() const
{
    const FTQueryTimer Timer("FTUserDeckRepository::ListDecks");

    std::vector<FTUserDeckRow> Result;

    FTStatement Query{Target.Prepare(
        "SELECT UserDecks.Id, UserDecks.Name, UserDecks.Description, UserDecks.CategoryId, "
        "       UserDecks.DailyLearning, UserDecks.CreatedDate, "
        "       COUNT(Cards.Id), "
        "       COALESCE(SUM(Cards.LeitnerStage = 0 AND Cards.CompletedDate IS NULL), 0), "
        "       COALESCE(SUM(Cards.LeitnerStage > 0 AND Cards.CompletedDate IS NULL), 0), "
        "       COALESCE(SUM(Cards.CompletedDate IS NOT NULL), 0) "
        "FROM UserDecks "
        "LEFT JOIN UserDeckCards ON UserDeckCards.DeckId = UserDecks.Id AND UserDeckCards.Deleted = 0 "
        "LEFT JOIN Cards ON Cards.Id = UserDeckCards.CardId AND Cards.Deleted = 0 "
        "WHERE UserDecks.Deleted = 0 AND " + Pair.Clause("UserDecks") + " "
        "GROUP BY UserDecks.Id "
        "ORDER BY UserDecks.CreatedDate, UserDecks.Name COLLATE NOCASE")};

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return Result;
    }

    Pair.Bind(Query);

    while (Query.Step())
    {
        FTUserDeckRow Row;
        Row.Id = Query.GetText(0);
        Row.Name = Query.GetText(1);
        Row.Description = Query.GetText(2);
        Row.CategoryId = Query.GetText(3);
        Row.bDailyLearning = Query.GetInt(4) != 0;
        Row.CreatedDate = Query.GetInt64(5);
        Row.CardCount = Query.GetInt(6);
        Row.PoolCount = Query.GetInt(7);
        Row.LearningCount = Query.GetInt(8);
        Row.LearnedCount = Query.GetInt(9);

        Result.push_back(std::move(Row));
    }

    return Result;
}

std::optional<std::string> FTUserDeckRepository::CreateDeck(const std::string& Name, const std::string& CategoryId,
                                                           const std::string& Description)
{
    if (Name.empty() || !Pair.IsSet())
    {
        LastError = Name.empty() ? "empty deck name" : "no active language pair";

        return std::nullopt;
    }

    const std::string Id{MakeUuid()};
    const int64_t NowTime{Now()};

    const bool bCreated = Run("FTUserDeckRepository::CreateDeck",
                              "INSERT INTO UserDecks (Id, Name, CategoryId, TargetLanguage, NativeLanguage, "
                              "                       CreatedDate, LastUpdatedDate, Description) "
                              "VALUES (?1, ?2, ?3, ?90, ?91, ?4, ?4, ?5)",
                              [&](FTStatement& Statement)
                              {
                                  Statement.BindText(1, Id);
                                  Statement.BindText(2, Name);
                                  Statement.BindText(3, CategoryId);
                                  Statement.BindInt64(4, NowTime);
                                  Statement.BindText(5, Description);
                                  Statement.BindText(TargetLanguageParameter, Pair.GetTargetLanguage());
                                  Statement.BindText(NativeLanguageParameter, Pair.GetNativeLanguage());
                              });

    if (!bCreated)
    {
        return std::nullopt;
    }

    return Id;
}

bool FTUserDeckRepository::RenameDeck(const std::string& Id, const std::string& Name)
{
    if (Name.empty())
    {
        LastError = "empty deck name";

        return false;
    }

    return Run("FTUserDeckRepository::RenameDeck",
               "UPDATE UserDecks SET Name = ?2, LastUpdatedDate = ?3 WHERE Id = ?1 AND Deleted = 0",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, Id);
                   Statement.BindText(2, Name);
                   Statement.BindInt64(3, Now());
               });
}

bool FTUserDeckRepository::EditDeck(const std::string& Id, const std::string& Name, const std::string& Description)
{
    if (Name.empty())
    {
        LastError = "empty deck name";

        return false;
    }

    return Run("FTUserDeckRepository::EditDeck",
               "UPDATE UserDecks SET Name = ?2, Description = ?3, LastUpdatedDate = ?4 WHERE Id = ?1 AND Deleted = 0",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, Id);
                   Statement.BindText(2, Name);
                   Statement.BindText(3, Description);
                   Statement.BindInt64(4, Now());
               });
}

bool FTUserDeckRepository::SetDailyLearning(const std::string& Id, bool bDailyLearning)
{
    return Run("FTUserDeckRepository::SetDailyLearning",
               "UPDATE UserDecks SET DailyLearning = ?2, LastUpdatedDate = ?3 WHERE Id = ?1 AND Deleted = 0",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, Id);
                   Statement.BindInt(2, bDailyLearning ? 1 : 0);
                   Statement.BindInt64(3, Now());
               });
}

bool FTUserDeckRepository::MoveDeck(const std::string& Id, const std::string& CategoryId)
{
    return Run("FTUserDeckRepository::MoveDeck",
               "UPDATE UserDecks SET CategoryId = ?2, LastUpdatedDate = ?3 WHERE Id = ?1 AND Deleted = 0",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, Id);
                   Statement.BindText(2, CategoryId);
                   Statement.BindInt64(3, Now());
               });
}

bool FTUserDeckRepository::DeleteDeck(const std::string& Id)
{
    const int64_t NowTime{Now()};

    const auto Bind = [&](FTStatement& Statement)
    {
        Statement.BindText(1, Id);
        Statement.BindInt64(2, NowTime);
    };

    return Run("FTUserDeckRepository::DeleteDeck",
               "UPDATE UserDecks SET Deleted = 1, LastUpdatedDate = ?2 WHERE Id = ?1",
               Bind)
        && Run("FTUserDeckRepository::DeleteDeckCards",
               "UPDATE UserDeckCards SET Deleted = 1, LastUpdatedDate = ?2 WHERE DeckId = ?1 AND Deleted = 0",
               Bind);
}

bool FTUserDeckRepository::AddCard(const std::string& DeckId, const std::string& CardId)
{
    return Run("FTUserDeckRepository::AddCard",
               "INSERT INTO UserDeckCards (DeckId, CardId, LastUpdatedDate, Deleted) VALUES (?1, ?2, ?3, 0) "
               "ON CONFLICT (CardId) DO UPDATE SET DeckId = excluded.DeckId, Deleted = 0, "
               "                                   LastUpdatedDate = excluded.LastUpdatedDate",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, DeckId);
                   Statement.BindText(2, CardId);
                   Statement.BindInt64(3, Now());
               });
}

bool FTUserDeckRepository::RemoveCard(const std::string& DeckId, const std::string& CardId)
{
    return Run("FTUserDeckRepository::RemoveCard",
               "UPDATE UserDeckCards SET Deleted = 1, LastUpdatedDate = ?3 WHERE DeckId = ?1 AND CardId = ?2",
               [&](FTStatement& Statement)
               {
                   Statement.BindText(1, DeckId);
                   Statement.BindText(2, CardId);
                   Statement.BindInt64(3, Now());
               });
}

std::optional<std::string> FTUserDeckRepository::FindDeckOf(const std::string& CardId) const
{
    const FTQueryTimer Timer("FTUserDeckRepository::FindDeckOf");

    FTStatement Query{Target.Prepare("SELECT DeckId FROM UserDeckCards WHERE CardId = ?1 AND Deleted = 0")};

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return std::nullopt;
    }

    Query.BindText(1, CardId);

    return Query.Step() ? std::optional<std::string>{Query.GetText(0)} : std::nullopt;
}

std::vector<std::string> FTUserDeckRepository::ListCardIds(const std::string& DeckId) const
{
    const FTQueryTimer Timer("FTUserDeckRepository::ListCardIds");

    std::vector<std::string> Result;

    FTStatement Query{Target.Prepare(
        "SELECT UserDeckCards.CardId FROM UserDeckCards "
        "JOIN Cards ON Cards.Id = UserDeckCards.CardId "
        "WHERE UserDeckCards.DeckId = ?1 AND UserDeckCards.Deleted = 0 AND Cards.Deleted = 0 "
        "ORDER BY Cards.TargetWord COLLATE NOCASE")};

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return Result;
    }

    Query.BindText(1, DeckId);

    while (Query.Step())
    {
        Result.push_back(Query.GetText(0));
    }

    return Result;
}

std::unordered_set<std::string> FTUserDeckRepository::ListCardIdsOutsideDailyLearning() const
{
    const FTQueryTimer Timer("FTUserDeckRepository::ListCardIdsOutsideDailyLearning");

    std::unordered_set<std::string> Result;

    FTStatement Query{Target.Prepare(
        "SELECT UserDeckCards.CardId FROM UserDeckCards "
        "JOIN UserDecks ON UserDecks.Id = UserDeckCards.DeckId "
        "WHERE UserDeckCards.Deleted = 0 AND UserDecks.Deleted = 0 AND UserDecks.DailyLearning = 0 "
        "AND " + Pair.Clause("UserDecks"))};

    if (!Query.IsValid())
    {
        LastError = Target.GetLastError();

        return Result;
    }

    Pair.Bind(Query);

    while (Query.Step())
    {
        Result.insert(Query.GetText(0));
    }

    return Result;
}

std::string FTUserDeckRepository::GetLastError() const
{
    return LastError;
}
