#pragma once

#include "core/FTDictionaryEntry.h"
#include "core/FTSentenceTree.h"

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

class FTDatabase;

struct FTLearnableWord
{
    std::string Headword;

    std::string PartOfSpeech;

    std::optional<int> Gender;

    std::string Translation;

    int FrequencyRank = 0;
};

struct FTLearnableWordFilter
{
    std::optional<int> CefrLevel;

    bool bOnlyUngraded = false;

    std::string PartOfSpeech;

    std::string Category;

    int AboveTeachingRank = 0;
};

struct FTHeadwordForm
{
    std::string Headword;

    std::string PartOfSpeech;

    std::string Form;
};

struct FTRankedWord
{
    uint32_t WordId = 0;

    bool bVerb = false;
};

struct FTFormSentence
{
    int First = 0;

    int Number = 0;

    uint32_t SentenceId = 0;
};

struct FTLevelRank
{
    int Level = 0;

    int TeachingRank = 0;
};

struct FTPartOfSpeechCount
{
    std::string PartOfSpeech;
    int Count = 0;
};

struct FTCategory
{
    std::string Code;

    int WordCount = 0;
};

inline constexpr int MinimumCategoryWords = 10;

struct FTDeck
{
    int64_t Id = 0;

    std::string Name;

    std::string Category;

    int WordCount = 0;
};

struct FTDeckWord
{
    std::string Headword;
    std::string PartOfSpeech;

    std::string Gist;

    std::string Context;

    std::optional<int> Gender;

    std::string Translation;

    std::optional<int> CefrLevel;

    std::optional<int> TeachingRank;
};

struct FTReverseLookupMatch
{
    std::string Headword;
    std::string PartOfSpeech;

    std::optional<int> Gender;

    int TeachingRank = 0;

    std::optional<int> FrequencyRank;

    std::optional<int> CefrLevel;
};

struct FTDictionarySearchMatch
{
    std::string Headword;
    std::string PartOfSpeech;
    std::optional<int> CefrLevel;
    std::optional<int> TeachingRank;
    std::string Translation;
    bool bExactMatch = false;
    std::optional<int> FrequencyRank;
    std::optional<double> TranslationProbability;
};

class FTDictionaryRepository
{
public:
    FTDictionaryRepository(FTDatabase& InDatabase, std::string InNativeLanguage = {});

    const std::string& GetNativeLanguage() const;

    std::vector<FTDictionaryEntry> LookUp(const std::string& Headword) const;

    std::optional<FTDictionaryEntry> LookUpForForms(const std::string& Headword, const std::string& PartOfSpeech) const;

    std::optional<FTDictionaryEntry> LookUpById(int64_t WordId) const;

    std::optional<FTDictionarySearchMatch> LookUpExact(const std::string& Headword,
                                                        const std::string& PartOfSpeech) const;

    std::vector<FTLearnableWord> LoadLearnableWords(int Limit, int Offset,
                                                   const FTLearnableWordFilter& Filter = {}) const;

    std::vector<FTPartOfSpeechCount> CountLearnableWords(
        const FTLearnableWordFilter& Filter = {}) const;

    std::vector<FTCategory> ListCategories() const;

    bool HasCategory(const std::string& Code) const;

    bool HasReading(const std::string& Headword, const std::string& PartOfSpeech) const;

    std::unordered_set<std::string> FindHeadwordsWithReading(
        const std::vector<std::string>& Headwords, const std::string& PartOfSpeech) const;

    std::unordered_set<std::string> FindHeadwordsWithTag(
        const std::vector<std::string>& Headwords, const std::string& Tag) const;

    std::vector<FTDictionarySense> LoadMeanings(const std::string& Headword,
                                                const std::string& PartOfSpeech) const;

    std::string FindPhonetic(const std::string& Headword, const std::string& PartOfSpeech) const;

    bool HasMeaningExamples() const;

    bool HasDecks() const;

    bool HasDeckCategories() const;

    std::vector<FTDeck> ListDecks() const;

    std::vector<FTDeckWord> LoadDeckWords(int64_t DeckId) const;

    std::map<int64_t, std::vector<FTDeckWord>> LoadDeckReadings() const;

    std::vector<FTDictionaryTranslation> LoadTranslationsForReading(
        const std::string& Headword,
        const std::string& PartOfSpeech,
        const std::optional<int>& Gender,
        int Limit) const;

    std::vector<FTDictionaryEntry> LookUpIgnoringAccents(const std::string& Headword) const;

    std::vector<FTReverseLookupMatch> LookUpByTranslation(const std::string& NativeWord,
                                                          int Limit = 24) const;

    std::vector<FTReverseLookupMatch> LookUpPrefix(const std::string& Prefix,
                                                    int Limit = 24) const;

    std::vector<FTDictionarySearchMatch> Search(const std::string& SearchText, int Limit,
                                                bool bNativeQuery = false) const;

    FTWordIdSet ResolveWordIds(const std::vector<std::string>& Headwords) const;

    std::vector<uint32_t> ListWordIds(const std::string& Headword) const;

    std::vector<FTHeadwordForm> ListHeadwordForms(const std::vector<std::string>& Headwords) const;

    std::vector<FTRankedWord> ListWordsUpToTeachingRank(int TeachingRank) const;

    int GetTeachingRankCeiling(int LearnableWordsAbove) const;

    std::vector<FTLevelRank> LoadLevelTeachingRanks() const;

    std::vector<FTTreeSentence> LoadTreeSentences(const std::vector<uint32_t>& SentenceIds) const;

    bool HasFormSentences() const;

    std::vector<FTFormSentence> FindFormSentences(const std::string& Headword, const std::string& Kind,
                                                  const std::string& Tense) const;

    std::vector<std::vector<FTTreeSentence>> FindSentencesWithPhrases(const std::vector<std::string>& Phrases,
                                                                      size_t LimitPerPhrase) const;

    static std::string MakeFoldedExpression(const std::string& Column);

    static bool HasSearchIndices(FTDatabase& Database);

private:
    std::optional<int64_t> GetNativeLanguageFilter() const;

    std::vector<FTLearnableWord> LoadAllLearnableWords(
        const FTLearnableWordFilter& Filter,
        int Limit = std::numeric_limits<int>::max(), int Offset = 0) const;

    bool HasLearnableReadings() const;

    std::vector<FTLearnableWord> LoadLearnableReadings(const FTLearnableWordFilter& Filter, int64_t LanguageId,
                                                       int Limit, int Offset) const;

    std::vector<FTPartOfSpeechCount> CountLearnableReadings(const FTLearnableWordFilter& Filter,
                                                            int64_t LanguageId) const;

    FTDatabase& Target;

    std::string NativeLanguage;

    mutable std::optional<int64_t> ResolvedNativeLanguageId;

    mutable std::optional<bool> ResolvedMeaningExamples;

    mutable std::optional<bool> ResolvedDecks;
    mutable std::optional<bool> ResolvedFormSentences;

    mutable std::optional<bool> ResolvedDeckCategories;

    mutable std::optional<bool> ResolvedLearnableReadings;

    mutable std::map<std::string, std::vector<FTLearnableWord>> CategoryOfferCache;

    mutable std::map<int, int> RankCeilingCache;

    mutable std::optional<std::vector<FTLevelRank>> ResolvedLevelTeachingRanks;
};
