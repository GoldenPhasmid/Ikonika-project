#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class FTDatabase;

class FTWordIdSet
{
public:
    void Add(uint32_t WordId);

    bool Contains(uint32_t WordId) const;

    size_t GetCount() const;

private:
    std::vector<uint64_t> Bits;

    size_t Count = 0;
};

struct FTTreeSentence
{
    uint32_t Id = 0;

    std::string Text;

    std::string Translation;

    int GapStart = -1;

    int GapLength = 0;

    int Level = 1;

    int ChangedCount = 0;
};

constexpr uint32_t MakeLevelMask(uint32_t Level)
{
    return Level < 32 ? uint32_t{1} << Level : 0;
}

struct FTFocusedSentence
{
    uint32_t SentenceId = 0;

    uint64_t FocusMask = 0;

    int Level = 0;
};

struct FTSentenceQuery
{
    size_t MaxCount = std::numeric_limits<size_t>::max();

    uint32_t MinLength = 0;

    uint32_t MaxLength = std::numeric_limits<uint32_t>::max();

    uint32_t LevelMask = std::numeric_limits<uint32_t>::max();

    uint64_t Seed = 0;
};

class FTSentenceTree
{
public:
    static constexpr uint32_t Format = 3;

    static std::optional<FTSentenceTree> Parse(std::string_view Blob);

    static std::optional<FTSentenceTree> Load(FTDatabase& Database);

    std::vector<uint32_t> FindWritableSentences(const FTWordIdSet& KnownWords,
                                                const FTSentenceQuery& Query = {}) const;

    std::vector<FTFocusedSentence> FindFocusedSentences(const FTWordIdSet& KnownWords,
                                                        const std::vector<std::vector<uint32_t>>& FocusWordIds,
                                                        const FTSentenceQuery& Query = {}) const;

    std::vector<FTFocusedSentence> FindSentencesWithWords(const std::vector<std::vector<uint32_t>>& FocusWordIds,
                                                          const FTSentenceQuery& Query = {}) const;

    uint32_t GetWordSpan() const;

    bool IsWordKey(uint32_t Key) const;

    uint32_t MakeTenseKey(uint32_t WordId, uint32_t Tense) const;

    size_t GetNodeCount() const;

    size_t GetSentenceReferenceCount() const;

    size_t GetMemoryBytes() const;

private:
    template <typename FTKeyFilter, typename FTKeyMask, typename FTVisitor>
    void VisitSentences(FTKeyFilter&& IsKeyOpen, const FTSentenceQuery& Query, FTKeyMask&& KeyMask,
                        FTVisitor&& Visit) const;

    template <typename FTKeyFilter>
    std::vector<FTFocusedSentence> CollectFocusedSentences(FTKeyFilter&& IsKeyOpen,
                                                           const std::vector<std::vector<uint32_t>>& FocusWordIds,
                                                           const FTSentenceQuery& Query) const;

    std::vector<uint32_t> NodeWords;
    std::vector<uint32_t> FirstChildren;
    std::vector<uint32_t> FirstSentences;
    std::vector<uint32_t> SentenceIds;
    std::vector<uint8_t> ReferenceLevels;
    uint32_t WordSpan = 0;
};
