#pragma once

#include "core/FTDictionaryEntry.h"
#include "core/FTLevelBelief.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

struct FTDiscoveryRow
{
    int64_t DeckId = 0;

    std::string Headword;

    std::string PartOfSpeech;

    std::string Gist;

    std::optional<int> Gender;

    std::string Translation;

    int TeachingRank = 0;

    bool bAskable = false;
};

enum class EFTDiscoveryPrompt
{
    Word,
    Sentence,
    Beginner
};

enum class EFTDiscoveryOutcome
{
    Known,
    Almost,
    New
};

struct FTDiscoveryOption
{
    std::string Text;

    int RowIndex = -1;

    bool bCorrect = false;

    bool bOtherMeaning = false;
};

struct FTDiscoveryQuestion
{
    int RowIndex = -1;

    FTDictionarySense Meaning;

    EFTDiscoveryPrompt Prompt = EFTDiscoveryPrompt::Word;

    std::vector<FTDiscoveryOption> Options;

    std::string Translation;

    int ExampleSpanStart = -1;

    int ExampleSpanLength = 0;

    std::string BeginnerSentence;

    int BeginnerSpanStart = -1;

    int BeginnerSpanLength = 0;

    int ClueStep = 0;

    std::vector<bool> Struck;

    std::optional<int> Chosen;

    std::optional<EFTDiscoveryOutcome> Outcome;
};

struct FTDiscoveryResult
{
    int RowIndex = -1;

    FTDictionarySense Meaning;

    std::string Translation;

    EFTDiscoveryOutcome Outcome = EFTDiscoveryOutcome::New;

    bool bLearn = true;
};

struct FTDiscoverySources
{
    std::function<std::vector<FTDictionarySense>(const FTDiscoveryRow& Row)> LoadMeanings;

    std::function<std::vector<std::string>(const FTDiscoveryRow& Row)> LoadForms;

    std::function<bool(const FTDiscoveryRow& Row, const std::string& Example)> IsKnownExample;
};

struct FTDiscoveryLearner
{
    int CutoffRank = 0;

    int KnownWordCount = 0;
};

int GetDiscoveryClueCount(EFTDiscoveryPrompt Prompt);

bool ShowsDiscoveryExample(const FTDiscoveryQuestion& Question);

bool ShowsDiscoverySentence(const FTDiscoveryQuestion& Question);

std::string NormalizeDiscoveryTranslation(const std::string& Text);

std::string CleanDiscoveryTranslation(const std::string& Text);

class FTDiscoveryPlanner
{
public:
    static constexpr int DefaultQuestionCount = 12;

    static constexpr int BeginnerWordCount = 1000;

    FTDiscoveryPlanner(std::vector<FTDiscoveryRow> InRows, FTDiscoverySources InSources, const FTDiscoveryLearner& Learner,
                       int InQuestionCount, uint32_t Seed);

    const std::vector<FTDiscoveryRow>& GetRows() const;

    const FTDiscoveryQuestion* GetCurrentQuestion() const;

    int GetAskedCount() const;

    int GetQuestionCount() const;

    bool IsFinished() const;

    void Choose(int OptionIndex);

    void AskClue();

    void Advance();

    const std::vector<FTDiscoveryResult>& GetResults() const;

    void SetLearn(int ResultIndex, bool bLearn);

    void SetTranslation(int ResultIndex, const std::string& Translation);

    int GetCurrentResultIndex() const;

    int GetLevelCutoffRank() const;

    bool IsBeginner() const;

private:
    std::optional<FTDiscoveryQuestion> PickFreshQuestion();

    std::optional<FTDiscoveryQuestion> PickFreshQuestion(bool bBeginnerPrompt);

    std::optional<FTDiscoveryQuestion> BuildQuestion(int RowIndex, EFTDiscoveryPrompt Prompt);

    EFTDiscoveryPrompt ChoosePrompt(const FTDiscoveryRow& Row);

    const std::vector<FTDictionarySense>& LoadMeanings(int RowIndex);

    const std::vector<std::string>& LoadForms(int RowIndex);

    bool AddBeginnerSentence(FTDiscoveryQuestion& Question);

    std::vector<int> FindNeighbours(int RowIndex) const;

    bool AddNativeOptions(FTDiscoveryQuestion& Question, const std::vector<FTDictionarySense>& Meanings);

    void Settle(EFTDiscoveryOutcome Outcome);

    std::vector<FTDiscoveryRow> Rows;

    FTDiscoverySources Sources;

    std::vector<std::optional<std::vector<FTDictionarySense>>> MeaningCache;

    std::vector<std::optional<std::vector<std::string>>> FormCache;

    FTLevelBelief Belief;

    std::mt19937 Random;

    int QuestionCount = DefaultQuestionCount;

    int AskedCount = 0;

    std::optional<FTDiscoveryQuestion> Current;

    std::vector<FTDiscoveryResult> Results;

    std::set<std::string> AskedHeadwords;

    std::set<int> UnaskableRows;

    std::set<int> UnaskableBeginnerRows;

    int CutoffRank = 0;

    bool bBeginner = false;
};
