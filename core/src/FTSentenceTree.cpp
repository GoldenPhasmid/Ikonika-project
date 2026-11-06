#include "core/FTSentenceTree.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"

#include <algorithm>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
    constexpr std::string_view TreeMagic{"IKST"};

    constexpr size_t HeaderBytes = 20;

    uint32_t ReadUint32(std::string_view Bytes, size_t Offset)
    {
        const auto Byte = [&](size_t Index)
        {
            return static_cast<uint32_t>(static_cast<unsigned char>(Bytes[Offset + Index]));
        };

        return Byte(0) | (Byte(1) << 8) | (Byte(2) << 16) | (Byte(3) << 24);
    }

    std::vector<uint32_t> ReadArray(std::string_view Bytes, size_t& Offset, size_t Count)
    {
        std::vector<uint32_t> Result;
        Result.reserve(Count);

        for (size_t Index = 0; Index < Count; ++Index)
        {
            Result.push_back(ReadUint32(Bytes, Offset));
            Offset += sizeof(uint32_t);
        }

        return Result;
    }

    bool IsRangeTable(const std::vector<uint32_t>& Starts, uint32_t First, uint32_t Last)
    {
        return Starts.front() == First && Starts.back() == Last
            && std::is_sorted(Starts.begin(), Starts.end());
    }
}

void FTWordIdSet::Add(uint32_t WordId)
{
    const size_t Word = WordId / 64;

    if (Word >= Bits.size())
    {
        Bits.resize(Word + 1, 0);
    }

    const uint64_t Mask = uint64_t{1} << (WordId % 64);

    if ((Bits[Word] & Mask) == 0)
    {
        Bits[Word] |= Mask;
        ++Count;
    }
}

bool FTWordIdSet::Contains(uint32_t WordId) const
{
    const size_t Word = WordId / 64;

    if (Word >= Bits.size())
    {
        return false;
    }

    return (Bits[Word] & (uint64_t{1} << (WordId % 64))) != 0;
}

size_t FTWordIdSet::GetCount() const
{
    return Count;
}

std::optional<FTSentenceTree> FTSentenceTree::Parse(std::string_view Blob)
{
    if (Blob.size() < HeaderBytes || Blob.substr(0, TreeMagic.size()) != TreeMagic)
    {
        qCWarning(LogDatabase) << "Sentence tree: not a sentence tree blob," << Blob.size()
                               << "bytes";

        return std::nullopt;
    }

    const uint32_t BlobFormat = ReadUint32(Blob, 4);

    if (BlobFormat != Format)
    {
        qCWarning(LogDatabase) << "Sentence tree: format" << BlobFormat << "and this build reads"
                               << Format;

        return std::nullopt;
    }

    const size_t NodeCount = ReadUint32(Blob, 8);
    const size_t ReferenceCount = ReadUint32(Blob, 12);
    const size_t LevelBytes = (ReferenceCount + 3) / 4 * 4;
    const size_t ExpectedBytes = HeaderBytes
        + sizeof(uint32_t) * (NodeCount + 2 * (NodeCount + 1) + ReferenceCount) + LevelBytes;

    if (NodeCount == 0 || Blob.size() != ExpectedBytes)
    {
        qCWarning(LogDatabase) << "Sentence tree:" << NodeCount << "nodes and" << ReferenceCount
                               << "references do not fit" << Blob.size() << "bytes";

        return std::nullopt;
    }

    size_t Offset = HeaderBytes;

    FTSentenceTree Tree;
    Tree.WordSpan = ReadUint32(Blob, 16);
    Tree.NodeWords = ReadArray(Blob, Offset, NodeCount);
    Tree.FirstChildren = ReadArray(Blob, Offset, NodeCount + 1);
    Tree.FirstSentences = ReadArray(Blob, Offset, NodeCount + 1);
    Tree.SentenceIds = ReadArray(Blob, Offset, ReferenceCount);

    Tree.ReferenceLevels.reserve(ReferenceCount);

    for (size_t Reference = 0; Reference < ReferenceCount; ++Reference)
    {
        Tree.ReferenceLevels.push_back(static_cast<uint8_t>(Blob[Offset + Reference]));
    }

    const bool bChildrenValid =
        IsRangeTable(Tree.FirstChildren, 1, static_cast<uint32_t>(NodeCount));
    const bool bSentencesValid =
        IsRangeTable(Tree.FirstSentences, 0, static_cast<uint32_t>(ReferenceCount));

    if (!bChildrenValid || !bSentencesValid)
    {
        qCWarning(LogDatabase) << "Sentence tree: child or sentence ranges are out of order";

        return std::nullopt;
    }

    for (size_t Node = 0; Node < NodeCount; ++Node)
    {
        const bool bHasChildren = Tree.FirstChildren[Node] < Tree.FirstChildren[Node + 1];

        if (bHasChildren && Tree.FirstChildren[Node] <= Node)
        {
            qCWarning(LogDatabase) << "Sentence tree: node" << Node
                                   << "has a child that is not below it";

            return std::nullopt;
        }
    }

    return Tree;
}

std::optional<FTSentenceTree> FTSentenceTree::Load(FTDatabase& Database)
{
    const FTQueryTimer Timer("FTSentenceTree::Load");

    FTStatement Exists = Database.Prepare(
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'sentence_tree'");

    if (!Exists.IsValid() || !Exists.Step())
    {
        qCInfo(LogDatabase) << "Sentence tree: the dictionary has no sentence index";

        return std::nullopt;
    }

    FTStatement Query = Database.Prepare("SELECT data FROM sentence_tree WHERE id = 1");

    if (!Query.IsValid() || !Query.Step())
    {
        qCWarning(LogDatabase) << "Sentence tree: the sentence index table is empty";

        return std::nullopt;
    }

    const std::string Blob = Query.GetBlob(0);

    std::optional<FTSentenceTree> Tree = Parse(Blob);

    if (Tree.has_value())
    {
        qCInfo(LogDatabase) << "Sentence tree: loaded" << Tree->GetNodeCount() << "nodes,"
                            << Tree->GetSentenceReferenceCount() << "sentence references,"
                            << Tree->GetMemoryBytes() << "bytes";
    }

    return Tree;
}

template <typename FTKeyFilter, typename FTKeyMask, typename FTVisitor>
void FTSentenceTree::VisitSentences(FTKeyFilter&& IsKeyOpen, const FTSentenceQuery& Query, FTKeyMask&& KeyMask,
                                    FTVisitor&& Visit) const
{
    if (NodeWords.empty() || Query.MinLength > Query.MaxLength || Query.LevelMask == 0)
    {
        return;
    }

    struct FPendingNode
    {
        uint32_t Node = 0;
        uint32_t Depth = 0;
        uint64_t Mask = 0;
    };

    std::vector<FPendingNode> Pending{FPendingNode{}};

    while (!Pending.empty())
    {
        const FPendingNode Current = Pending.back();
        Pending.pop_back();

        if (Current.Depth >= Query.MinLength)
        {
            for (uint32_t Reference = FirstSentences[Current.Node]; Reference < FirstSentences[Current.Node + 1];
                 ++Reference)
            {
                if ((Query.LevelMask & MakeLevelMask(ReferenceLevels[Reference])) != 0)
                {
                    Visit(SentenceIds[Reference], Current.Mask, ReferenceLevels[Reference]);
                }
            }
        }

        for (uint32_t Child = FirstChildren[Current.Node]; Child < FirstChildren[Current.Node + 1]; ++Child)
        {
            const uint32_t Key = NodeWords[Child];

            if (!IsKeyOpen(Key))
            {
                continue;
            }

            const uint32_t ChildDepth = Current.Depth + (IsWordKey(Key) ? 1 : 0);

            if (ChildDepth <= Query.MaxLength)
            {
                Pending.push_back(FPendingNode{Child, ChildDepth, Current.Mask | KeyMask(Key)});
            }
        }
    }
}

std::vector<uint32_t> FTSentenceTree::FindWritableSentences(const FTWordIdSet& KnownWords,
                                                            const FTSentenceQuery& Query) const
{
    std::vector<uint32_t> Found;

    if (Query.MaxCount == 0)
    {
        return Found;
    }

    const auto NoFocus = [](uint32_t) { return uint64_t{0}; };
    const auto IsKnown = [&KnownWords](uint32_t Key) { return KnownWords.Contains(Key); };

    if (Query.MaxCount == std::numeric_limits<size_t>::max())
    {
        VisitSentences(IsKnown, Query, NoFocus,
            [&](uint32_t SentenceId, uint64_t, uint8_t)
            {
                Found.push_back(SentenceId);
            });

        std::sort(Found.begin(), Found.end());
        Found.erase(std::unique(Found.begin(), Found.end()), Found.end());

        return Found;
    }

    std::mt19937_64 Engine{Query.Seed};
    std::unordered_set<uint32_t> Chosen;
    uint64_t Seen = 0;

    VisitSentences(IsKnown, Query, NoFocus,
        [&](uint32_t SentenceId, uint64_t, uint8_t)
        {
            if (Chosen.count(SentenceId) != 0)
            {
                return;
            }

            ++Seen;

            if (Found.size() < Query.MaxCount)
            {
                Found.push_back(SentenceId);
                Chosen.insert(SentenceId);

                return;
            }

            const uint64_t Slot = Engine() % Seen;

            if (Slot < Query.MaxCount)
            {
                Chosen.erase(Found[Slot]);
                Found[Slot] = SentenceId;
                Chosen.insert(SentenceId);
            }
        });

    std::sort(Found.begin(), Found.end());

    return Found;
}

std::vector<FTFocusedSentence> FTSentenceTree::FindFocusedSentences(
    const FTWordIdSet& KnownWords, const std::vector<std::vector<uint32_t>>& FocusWordIds,
    const FTSentenceQuery& Query) const
{
    return CollectFocusedSentences([&KnownWords](uint32_t Key) { return KnownWords.Contains(Key); },
                                   FocusWordIds, Query);
}

std::vector<FTFocusedSentence> FTSentenceTree::FindSentencesWithWords(
    const std::vector<std::vector<uint32_t>>& FocusWordIds, const FTSentenceQuery& Query) const
{
    return CollectFocusedSentences([](uint32_t) { return true; }, FocusWordIds, Query);
}

template <typename FTKeyFilter>
std::vector<FTFocusedSentence> FTSentenceTree::CollectFocusedSentences(
    FTKeyFilter&& IsKeyOpen, const std::vector<std::vector<uint32_t>>& FocusWordIds,
    const FTSentenceQuery& Query) const
{
    std::vector<FTFocusedSentence> Found;

    constexpr size_t MaximumFocusWords = 64;

    std::unordered_map<uint32_t, uint64_t> FocusBits;

    for (size_t Index = 0; Index < FocusWordIds.size() && Index < MaximumFocusWords; ++Index)
    {
        for (const uint32_t WordId : FocusWordIds[Index])
        {
            FocusBits[WordId] |= uint64_t{1} << Index;
        }
    }

    if (FocusBits.empty())
    {
        return Found;
    }

    std::unordered_map<uint32_t, FTFocusedSentence> FoundBySentence;

    VisitSentences(IsKeyOpen, Query,
        [&FocusBits](uint32_t Key)
        {
            const auto Bits = FocusBits.find(Key);

            return Bits != FocusBits.end() ? Bits->second : uint64_t{0};
        },
        [&FoundBySentence](uint32_t SentenceId, uint64_t Mask, uint8_t Level)
        {
            if (Mask == 0)
            {
                return;
            }

            FTFocusedSentence& Entry = FoundBySentence[SentenceId];
            Entry.SentenceId = SentenceId;
            Entry.FocusMask |= Mask;
            Entry.Level = Level;
        });

    Found.reserve(FoundBySentence.size());

    for (const auto& [SentenceId, Entry] : FoundBySentence)
    {
        Found.push_back(Entry);
    }

    std::sort(Found.begin(), Found.end(),
              [](const FTFocusedSentence& Left, const FTFocusedSentence& Right)
              {
                  return Left.SentenceId < Right.SentenceId;
              });

    return Found;
}

uint32_t FTSentenceTree::GetWordSpan() const
{
    return WordSpan;
}

bool FTSentenceTree::IsWordKey(uint32_t Key) const
{
    return WordSpan == 0 || Key < WordSpan;
}

uint32_t FTSentenceTree::MakeTenseKey(uint32_t WordId, uint32_t Tense) const
{
    return Tense * WordSpan + WordId;
}

size_t FTSentenceTree::GetNodeCount() const
{
    return NodeWords.size();
}

size_t FTSentenceTree::GetSentenceReferenceCount() const
{
    return SentenceIds.size();
}

size_t FTSentenceTree::GetMemoryBytes() const
{
    return sizeof(uint32_t)
        * (NodeWords.size() + FirstChildren.size() + FirstSentences.size() + SentenceIds.size())
        + ReferenceLevels.size();
}
