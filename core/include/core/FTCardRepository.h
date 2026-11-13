#pragma once

#include "core/FTCardStore.h"
#include "core/FTCard.h"
#include "core/FTTypes.h"

#include "core/FTLanguagePairScope.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class FTDatabase;
class FTSettings;
class FTStatement;

struct FTDailyStats
{
    FTDayNumber Day = 0;
    int LearnedCount = 0;
    int ReviewedCount = 0;
    int DueCount = 0;
    int GoalCount = 0;
};

struct FTDueCountForDay
{
    FTDayNumber Day = 0;
    int Count = 0;
};

struct FTStageCount
{
    int Stage = 0;
    bool bCompleted = false;
    int Count = 0;
};

struct FTSearchHistoryEntry
{
    std::string Term;
    std::string PartOfSpeech;
    int64_t WordId = 0;
};

struct FTStageProbe
{
    std::string TargetWord;
    std::string PartOfSpeech;
    std::optional<int> Gender;
    std::string Translation;
};

struct FTWordTranslationMatch
{
    int Stage = 0;
    std::string CardId;
    std::string PartOfSpeech;
    std::optional<int> Gender;
};

using FTCardAnnotator = std::function<void(std::vector<FTCard>&)>;

struct FTKnownVerbTense
{
    std::string Headword;

    std::string Tense;
};

struct FTKnownWord
{
    std::string TargetWord;

    std::string PartOfSpeech;

    std::string MeaningGist;
};

struct FTStudiedLanguage
{
    std::string TargetLanguage;

    std::string NativeLanguage;
};

struct FTStudiedHeadword
{
    std::string Headword;

    std::string PartOfSpeech;

    std::string Translation;
};

class FTCardRepository
{
public:
    using ClockFunction = std::function<int64_t()>;

    explicit FTCardRepository(FTDatabase& InDatabase, ClockFunction InClock = {});

    void SetActiveLanguages(const std::string& TargetLanguage, const std::string& NativeLanguage);

    void SetCardAnnotator(FTCardAnnotator InAnnotator);

    const std::string& GetTargetLanguage() const;
    const std::string& GetNativeLanguage() const;

    std::optional<std::string> AddCard(const FTCard& Word);

    bool DeleteCards(const std::vector<std::string>& CardIds);

    bool DeleteLanguage(const std::string& TargetLanguage);

    int CountLanguageCards(const std::string& TargetLanguage) const;

    std::vector<FTCardHandle> LoadCards() const;

    std::optional<FTCardHandle> LoadCard(const std::string& CardId) const;

    std::optional<int> FindWordStage(const std::string& TargetWord,
                                     const std::string& PartOfSpeech) const;

    std::vector<FTCard> LoadRecentlyStudiedCards(int Limit) const;

    std::vector<FTCard> FindCardsByHeadword(const std::string& SearchText, int Limit) const;

    bool IsWordCompleted(const std::string& TargetWord, const std::string& PartOfSpeech) const;

    std::optional<int> FindWordTranslationStage(const std::string& TargetWord,
                                                const std::string& PartOfSpeech,
                                                const std::optional<int>& Gender,
                                                const std::string& Translation) const;

    std::optional<int> FindMeaningStage(const std::string& TargetWord, const std::string& PartOfSpeech,
                                        const std::optional<int>& Gender, const std::string& MeaningGist) const;

    std::vector<std::optional<int>> FindWordTranslationStages(const std::vector<FTStageProbe>& Probes) const;

    std::optional<int> FindWordTranslationStageAnyReading(const std::string& TargetWord, const std::string& Translation) const;

    std::optional<FTWordTranslationMatch> FindWordTranslationMatch(const std::string& TargetWord,
                                                                   const std::string& PartOfSpeech,
                                                                   const std::string& Translation) const;

    std::vector<std::string> FindCardTranslationsForReading(const std::string& TargetWord,
                                                             const std::string& PartOfSpeech,
                                                             const std::optional<int>& Gender) const;

    std::vector<FTCardHandle> LoadDueCards(FTDayNumber Today) const;

    std::vector<FTCardHandle> LoadCardsDueOnDay(FTDayNumber Day) const;

    std::vector<FTCardHandle> LoadUnlearnedCards() const;

    std::vector<FTStudiedHeadword> ListStudiedHeadwords() const;

    std::vector<std::string> ListKnownHeadwords() const;

    std::vector<FTKnownVerbTense> ListKnownVerbTenses() const;

    bool AddFormsCards(const std::vector<FTCard>& Cards);

    bool MarkFormsCardsLearned(const std::vector<std::string>& CardIds, FTDayNumber Today);

    std::optional<FTCardHandle> LoadFormsCard(const std::string& CardId) const;

    std::vector<FTCardHandle> LoadFormsCards() const;

    int CountDueCards(FTDayNumber Today) const;

    int CountCards() const;

    int CountUnlearnedCards() const;

    bool UpdateCardSchedule(const FTCard& Word);

    bool ReturnCardToPool(const std::string& CardId);

    bool MarkCardCompleted(const std::string& CardId);

    bool RecordProgressEvent(const std::string& CardId, const char* EventId, int EventValue, FTDayNumber Day);

    bool UpdateCardContent(const std::string& CardId, const std::string& TargetWord,
                           const std::string& Translation,
                           const std::optional<int>& Gender,
                           const std::string& PartOfSpeech,
                           const std::string& MeaningGist, const std::string& Hint = {});

    bool PromoteCards(const std::vector<std::string>& CardIds);

    bool MarkWordTriaged(const std::string& TargetWord, const std::string& PartOfSpeech,
                         const std::optional<int>& Gender, const std::string& MeaningGist = {});

    bool UnmarkWordTriaged(const std::string& TargetWord, const std::string& PartOfSpeech,
                           const std::optional<int>& Gender, const std::string& MeaningGist);

    bool IsWordTriaged(const std::string& TargetWord, const std::string& PartOfSpeech,
                       const std::optional<int>& Gender) const;

    int CountTriagedWords() const;

    std::vector<FTKnownWord> ListKnownWords() const;

    bool IsWordUnseen(const std::string& TargetWord, const std::string& PartOfSpeech,
                      const std::optional<int>& Gender, const std::string& Translation) const;

    bool IsWordOwnedOrTriaged(const std::string& TargetWord, const std::string& PartOfSpeech) const;

    bool SetPlacementCutoff(const std::string& TargetLanguage, int CutoffRank);

    std::optional<int> FindPlacementCutoff(const std::string& TargetLanguage) const;

    std::optional<FTStudiedLanguage> FindLastStudiedLanguage() const;

    int GetPlacementCutoff() const;

    bool RecordSearch(const std::string& Term, const std::string& PartOfSpeech, int64_t WordId);

    std::vector<FTSearchHistoryEntry> LoadRecentSearches(int Limit) const;

    bool RecordDailyProgress(FTDayNumber Day, int LearnedCount, int ReviewedCount, int DailyGoal);

    std::vector<FTDailyStats> LoadDailyStats(FTDayNumber FirstDay, FTDayNumber LastDay) const;

    std::vector<FTDueCountForDay> CountDueCardsByDay(FTDayNumber FirstDay, FTDayNumber LastDay) const;

    std::vector<FTStageCount> CountCardsByStage() const;

    bool DeleteEverything();

    std::string GetLastError() const;

private:
    int64_t Now() const;

    FTCard WithStudiedTranslation(FTCard Card) const;
    std::vector<FTCard> LoadFormsRows() const;

    bool ApplyProgressEvent(const FTCard& Loaded, const char* EventId, int EventValue, FTDayNumber Day);

    std::vector<FTCard> ReadCardRows(FTStatement& Query) const;
    std::vector<FTCardHandle> ReadCards(FTStatement& Query) const;

    void RefreshStoredCard(const std::string& CardId) const;

    FTDatabase& Target;
    ClockFunction Clock;

    FTLanguagePairScope Pair;

    FTCardAnnotator Annotator;

    mutable FTCardStore Store;
};

void AdoptLastStudiedLanguage(const FTCardRepository& Cards, FTSettings& Settings);
