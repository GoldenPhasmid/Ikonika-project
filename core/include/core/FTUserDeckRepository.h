#pragma once

#include "core/FTLanguagePairScope.h"
#include "core/FTTypes.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

class FTDatabase;
class FTStatement;

struct FTUserDeckCategoryRow
{
    std::string Id;
    std::string Name;
    int Colour = 0;
};

struct FTUserDeckRow
{
    std::string Id;
    std::string Name;
    std::string Description;
    std::string CategoryId;
    bool bDailyLearning = true;
    int64_t CreatedDate = 0;
    int CardCount = 0;
    int PoolCount = 0;
    int LearningCount = 0;
    int LearnedCount = 0;
};

class FTUserDeckRepository
{
public:
    using ClockFunction = std::function<int64_t()>;

    explicit FTUserDeckRepository(FTDatabase& InDatabase, ClockFunction InClock = {});

    void SetActiveLanguages(const std::string& InTargetLanguage, const std::string& InNativeLanguage);

    std::vector<FTUserDeckCategoryRow> ListCategories() const;

    std::optional<std::string> CreateCategory(const std::string& Name, int Colour);

    bool RenameCategory(const std::string& Id, const std::string& Name);

    bool DeleteCategory(const std::string& Id);

    std::vector<FTUserDeckRow> ListDecks() const;

    std::optional<std::string> CreateDeck(const std::string& Name, const std::string& CategoryId, const std::string& Description = {});

    bool RenameDeck(const std::string& Id, const std::string& Name);

    bool EditDeck(const std::string& Id, const std::string& Name, const std::string& Description);

    bool SetDailyLearning(const std::string& Id, bool bDailyLearning);

    bool MoveDeck(const std::string& Id, const std::string& CategoryId);

    bool DeleteDeck(const std::string& Id);

    bool AddCard(const std::string& DeckId, const std::string& CardId);

    bool RemoveCard(const std::string& DeckId, const std::string& CardId);

    std::optional<std::string> FindDeckOf(const std::string& CardId) const;

    std::vector<std::string> ListCardIds(const std::string& DeckId) const;

    std::unordered_set<std::string> ListCardIdsOutsideDailyLearning() const;

    std::string GetLastError() const;

private:
    int64_t Now() const;

    bool Run(const char* Label, const std::string& Sql, const std::function<void(FTStatement&)>& Bind);

    FTDatabase& Target;
    ClockFunction Clock;
    FTLanguagePairScope Pair;
    mutable std::string LastError;
};
