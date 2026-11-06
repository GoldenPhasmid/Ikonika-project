#include <catch2/catch_test_macros.hpp>

#include "core/FTDatabase.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTSentenceCards.h"
#include "core/FTSentenceTree.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace
{
    struct FTTestSentence
    {
        uint32_t Id = 0;

        std::vector<std::vector<uint32_t>> Paths;

        uint8_t Level = 1;
    };

    struct FTTestNode
    {
        std::map<uint32_t, size_t> Children;

        std::set<uint32_t> Sentences;
    };

    void AppendUint32(std::string& Bytes, uint32_t Value)
    {
        for (int Shift = 0; Shift < 32; Shift += 8)
        {
            Bytes.push_back(static_cast<char>((Value >> Shift) & 0xFF));
        }
    }

    constexpr size_t HeaderBytes = 20;

    std::string BuildBlob(const std::vector<FTTestSentence>& Sentences, uint32_t WordSpan = 0)
    {
        std::vector<FTTestNode> Nodes(1);

        for (const FTTestSentence& Sentence : Sentences)
        {
            for (const std::vector<uint32_t>& Path : Sentence.Paths)
            {
                size_t Node = 0;

                for (const uint32_t WordId : Path)
                {
                    const auto Found = Nodes[Node].Children.find(WordId);

                    if (Found != Nodes[Node].Children.end())
                    {
                        Node = Found->second;

                        continue;
                    }

                    const size_t Child = Nodes.size();
                    Nodes[Node].Children.emplace(WordId, Child);
                    Nodes.emplace_back();
                    Node = Child;
                }

                Nodes[Node].Sentences.insert(Sentence.Id);
            }
        }

        std::vector<size_t> Order{0};
        std::vector<uint32_t> Words{0};

        for (size_t Position = 0; Position < Order.size(); ++Position)
        {
            for (const auto& [WordId, Child] : Nodes[Order[Position]].Children)
            {
                Order.push_back(Child);
                Words.push_back(WordId);
            }
        }

        std::vector<uint32_t> FirstChildren;
        std::vector<uint32_t> FirstSentences;
        std::vector<uint32_t> SentenceIds;
        uint32_t NextChild = 1;

        for (const size_t Node : Order)
        {
            FirstChildren.push_back(NextChild);
            NextChild += static_cast<uint32_t>(Nodes[Node].Children.size());
            FirstSentences.push_back(static_cast<uint32_t>(SentenceIds.size()));
            SentenceIds.insert(SentenceIds.end(), Nodes[Node].Sentences.begin(),
                               Nodes[Node].Sentences.end());
        }

        FirstChildren.push_back(NextChild);
        FirstSentences.push_back(static_cast<uint32_t>(SentenceIds.size()));

        std::string Blob{"IKST"};
        AppendUint32(Blob, FTSentenceTree::Format);
        AppendUint32(Blob, static_cast<uint32_t>(Words.size()));
        AppendUint32(Blob, static_cast<uint32_t>(SentenceIds.size()));
        AppendUint32(Blob, WordSpan);

        for (const std::vector<uint32_t>* Array : {&Words, &FirstChildren, &FirstSentences,
                                                   &SentenceIds})
        {
            for (const uint32_t Value : *Array)
            {
                AppendUint32(Blob, Value);
            }
        }

        std::map<uint32_t, uint8_t> Levels;

        for (const FTTestSentence& Sentence : Sentences)
        {
            Levels[Sentence.Id] = Sentence.Level;
        }

        for (const uint32_t SentenceId : SentenceIds)
        {
            Blob.push_back(static_cast<char>(Levels[SentenceId]));
        }

        Blob.append((4 - SentenceIds.size() % 4) % 4, '\0');

        return Blob;
    }

    FTSentenceTree ParseOrFail(const std::vector<FTTestSentence>& Sentences, uint32_t WordSpan = 0)
    {
        std::optional<FTSentenceTree> Tree = FTSentenceTree::Parse(BuildBlob(Sentences, WordSpan));
        REQUIRE(Tree.has_value());

        return std::move(*Tree);
    }

    FTWordIdSet MakeWordSet(std::initializer_list<uint32_t> WordIds)
    {
        FTWordIdSet Result;

        for (const uint32_t WordId : WordIds)
        {
            Result.Add(WordId);
        }

        return Result;
    }

    std::string ToHex(const std::string& Bytes)
    {
        static constexpr char Digits[] = "0123456789ABCDEF";

        std::string Hex;
        Hex.reserve(Bytes.size() * 2);

        for (const char Byte : Bytes)
        {
            const auto Value = static_cast<unsigned char>(Byte);
            Hex.push_back(Digits[Value >> 4]);
            Hex.push_back(Digits[Value & 0x0F]);
        }

        return Hex;
    }

    void OverwriteUint32(std::string& Blob, size_t Offset, uint32_t Value)
    {
        std::string Bytes;
        AppendUint32(Bytes, Value);
        Blob.replace(Offset, 4, Bytes);
    }

    const std::vector<FTTestSentence> Kitchen{
        {1, {{30, 20, 10}}},
        {2, {{30, 20}}},
        {3, {{30, 25, 10}}},
        {4, {{40, 10}}},
        {5, {{10}}},
    };
}

TEST_CASE("An empty vocabulary writes nothing", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    CHECK(Tree.FindWritableSentences(FTWordIdSet{}).empty());
}

TEST_CASE("A sentence is writable only when every one of its words is known", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    CHECK(Tree.FindWritableSentences(MakeWordSet({30, 20, 10})) == std::vector<uint32_t>{1, 2, 5});
    CHECK(Tree.FindWritableSentences(MakeWordSet({30, 10})) == std::vector<uint32_t>{5});
    CHECK(Tree.FindWritableSentences(MakeWordSet({20, 10})) == std::vector<uint32_t>{5});
}

TEST_CASE("A sentence ending on a shared prefix is found without its longer neighbours",
          "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    CHECK(Tree.FindWritableSentences(MakeWordSet({30, 20})) == std::vector<uint32_t>{2});
}

TEST_CASE("An unknown rare word prunes every sentence below it", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    CHECK(Tree.FindWritableSentences(MakeWordSet({20, 25, 10})) == std::vector<uint32_t>{5});
    CHECK(Tree.FindWritableSentences(MakeWordSet({40, 10})) == std::vector<uint32_t>{4, 5});
}

TEST_CASE("Knowing the whole vocabulary writes every sentence", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    CHECK(Tree.FindWritableSentences(MakeWordSet({10, 20, 25, 30, 40}))
          == std::vector<uint32_t>{1, 2, 3, 4, 5});
}

TEST_CASE("Length counts the words a sentence needs", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);
    const FTWordIdSet Everything = MakeWordSet({10, 20, 25, 30, 40});

    const auto Find = [&](uint32_t MinLength, uint32_t MaxLength)
    {
        FTSentenceQuery Query;
        Query.MinLength = MinLength;
        Query.MaxLength = MaxLength;

        return Tree.FindWritableSentences(Everything, Query);
    };

    CHECK(Find(1, 1) == std::vector<uint32_t>{5});
    CHECK(Find(2, 2) == std::vector<uint32_t>{2, 4});
    CHECK(Find(3, 3) == std::vector<uint32_t>{1, 3});
    CHECK(Find(2, 3) == std::vector<uint32_t>{1, 2, 3, 4});
    CHECK(Find(0, 0).empty());
    CHECK(Find(4, 10).empty());
    CHECK(Find(3, 2).empty());
}

TEST_CASE("A tense key locks a sentence until the verb's tense is known", "[sentencetree][tense]")
{
    const uint32_t WordSpan = 100;
    const uint32_t Present = 1;
    const uint32_t Perfect = 8;

    const FTSentenceTree Tree = ParseOrFail({
        {1, {{30, 130, 20}}},
        {2, {{30, 20}}},
        {3, {{30, 830, 20}}},
    }, WordSpan);

    CHECK(Tree.GetWordSpan() == WordSpan);
    CHECK(Tree.MakeTenseKey(30, Present) == 130);
    CHECK(Tree.MakeTenseKey(30, Perfect) == 830);
    CHECK(Tree.IsWordKey(30));
    CHECK(Tree.IsWordKey(99));
    CHECK_FALSE(Tree.IsWordKey(130));

    CHECK(Tree.FindWritableSentences(MakeWordSet({30, 20})) == std::vector<uint32_t>{2});
    CHECK(Tree.FindWritableSentences(MakeWordSet({30, 20, 130})) == std::vector<uint32_t>{1, 2});
    CHECK(Tree.FindWritableSentences(MakeWordSet({30, 20, 830})) == std::vector<uint32_t>{2, 3});
    CHECK(Tree.FindWritableSentences(MakeWordSet({20, 130})).empty());
}

TEST_CASE("Length counts words and leaves tense keys out", "[sentencetree][tense]")
{
    const FTSentenceTree Tree = ParseOrFail({
        {1, {{30, 130, 230, 430, 830, 20}}},
        {2, {{30, 20, 10}}},
        {3, {{20, 10, 110}}},
    }, 100);

    const FTWordIdSet Everything = MakeWordSet({10, 20, 30, 110, 130, 230, 430, 830});

    const auto Find = [&](uint32_t MinLength, uint32_t MaxLength)
    {
        FTSentenceQuery Query;
        Query.MinLength = MinLength;
        Query.MaxLength = MaxLength;

        return Tree.FindWritableSentences(Everything, Query);
    };

    CHECK(Find(2, 2) == std::vector<uint32_t>{1, 3});
    CHECK(Find(3, 3) == std::vector<uint32_t>{2});
    CHECK(Find(1, 1).empty());
    CHECK(Find(2, 3) == std::vector<uint32_t>{1, 2, 3});
    CHECK(Find(6, 6).empty());
}

TEST_CASE("A tree with no word span treats every key as a word", "[sentencetree][tense]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    CHECK(Tree.GetWordSpan() == 0);
    CHECK(Tree.IsWordKey(10));
    CHECK(Tree.IsWordKey(4000000000u));
}

TEST_CASE("Length and known words both have to hold", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    FTSentenceQuery Query;
    Query.MinLength = 2;

    CHECK(Tree.FindWritableSentences(MakeWordSet({30, 20, 10}), Query)
          == std::vector<uint32_t>{1, 2});
}

TEST_CASE("An ambiguous sentence counts the length of the reading the learner knows",
          "[sentencetree]")
{
    const std::vector<FTTestSentence> Readings{
        {9, {{50, 1}, {60, 2, 1}}},
    };

    const FTSentenceTree Tree = ParseOrFail(Readings);
    const FTWordIdSet Everything = MakeWordSet({1, 2, 50, 60});

    FTSentenceQuery Short;
    Short.MaxLength = 2;

    FTSentenceQuery Long;
    Long.MinLength = 3;

    CHECK(Tree.FindWritableSentences(Everything, Short) == std::vector<uint32_t>{9});
    CHECK(Tree.FindWritableSentences(Everything, Long) == std::vector<uint32_t>{9});
    CHECK(Tree.FindWritableSentences(MakeWordSet({1, 2, 60}), Short).empty());
}

TEST_CASE("MaxCount returns that many distinct writable sentences", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);
    const FTWordIdSet Everything = MakeWordSet({10, 20, 25, 30, 40});
    const std::vector<uint32_t> All = Tree.FindWritableSentences(Everything);

    for (uint64_t Seed = 0; Seed < 50; ++Seed)
    {
        FTSentenceQuery Query;
        Query.MaxCount = 3;
        Query.Seed = Seed;

        const std::vector<uint32_t> Picked = Tree.FindWritableSentences(Everything, Query);

        REQUIRE(Picked.size() == 3);
        CHECK(std::is_sorted(Picked.begin(), Picked.end()));
        CHECK(std::adjacent_find(Picked.begin(), Picked.end()) == Picked.end());
        CHECK(std::includes(All.begin(), All.end(), Picked.begin(), Picked.end()));
    }
}

TEST_CASE("MaxCount above the number of matches returns every match", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    FTSentenceQuery Query;
    Query.MaxCount = 100;

    CHECK(Tree.FindWritableSentences(MakeWordSet({30, 20, 10}), Query)
          == std::vector<uint32_t>{1, 2, 5});
}

TEST_CASE("MaxCount of zero returns nothing", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    FTSentenceQuery Query;
    Query.MaxCount = 0;

    CHECK(Tree.FindWritableSentences(MakeWordSet({10, 20, 25, 30, 40}), Query).empty());
}

TEST_CASE("The same seed picks the same sentences and other seeds reach every one",
          "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);
    const FTWordIdSet Everything = MakeWordSet({10, 20, 25, 30, 40});

    FTSentenceQuery Query;
    Query.MaxCount = 2;
    Query.Seed = 42;

    CHECK(Tree.FindWritableSentences(Everything, Query)
          == Tree.FindWritableSentences(Everything, Query));

    std::set<uint32_t> Reached;
    std::set<std::vector<uint32_t>> Distinct;

    for (uint64_t Seed = 0; Seed < 200; ++Seed)
    {
        Query.Seed = Seed;

        const std::vector<uint32_t> Picked = Tree.FindWritableSentences(Everything, Query);
        Reached.insert(Picked.begin(), Picked.end());
        Distinct.insert(Picked);
    }

    CHECK(Reached == std::set<uint32_t>{1, 2, 3, 4, 5});
    CHECK(Distinct.size() > 5);
}

TEST_CASE("A sentence with several known readings is picked once", "[sentencetree]")
{
    const std::vector<FTTestSentence> Suis{
        {7, {{50, 1}, {60, 1}}},
        {8, {{60, 2}}},
    };

    const FTSentenceTree Tree = ParseOrFail(Suis);

    for (uint64_t Seed = 0; Seed < 50; ++Seed)
    {
        FTSentenceQuery Query;
        Query.MaxCount = 2;
        Query.Seed = Seed;

        CHECK(Tree.FindWritableSentences(MakeWordSet({1, 2, 50, 60}), Query)
              == std::vector<uint32_t>{7, 8});
    }
}

TEST_CASE("A query keeps the sentences of the grammar levels it names", "[sentencetree]")
{
    const std::vector<FTTestSentence> Graded{
        {1, {{30, 20, 10}}, 1},
        {2, {{30, 20}}, 2},
        {3, {{30, 25, 10}}, 3},
        {4, {{40, 10}}, 1},
        {5, {{10}}, 4},
    };

    const FTSentenceTree Tree = ParseOrFail(Graded);
    const FTWordIdSet Everything = MakeWordSet({10, 20, 25, 30, 40});

    const auto Find = [&](uint32_t LevelMask)
    {
        FTSentenceQuery Query;
        Query.LevelMask = LevelMask;

        return Tree.FindWritableSentences(Everything, Query);
    };

    CHECK(Tree.FindWritableSentences(Everything) == std::vector<uint32_t>{1, 2, 3, 4, 5});
    CHECK(Find(MakeLevelMask(1)) == std::vector<uint32_t>{1, 4});
    CHECK(Find(MakeLevelMask(1) | MakeLevelMask(2)) == std::vector<uint32_t>{1, 2, 4});
    CHECK(Find(MakeLevelMask(3) | MakeLevelMask(4)) == std::vector<uint32_t>{3, 5});
    CHECK(Find(MakeLevelMask(1) | MakeLevelMask(4)) == std::vector<uint32_t>{1, 4, 5});
    CHECK(Find(MakeLevelMask(5)).empty());
    CHECK(Find(0).empty());
}

TEST_CASE("A level mask is one bit per level", "[sentencetree]")
{
    CHECK(MakeLevelMask(0) == 1u);
    CHECK(MakeLevelMask(1) == 2u);
    CHECK(MakeLevelMask(4) == 16u);
    CHECK(MakeLevelMask(31) == 0x80000000u);
    CHECK(MakeLevelMask(32) == 0u);
    CHECK(MakeLevelMask(255) == 0u);
}

TEST_CASE("Grammar level, length and count all have to hold", "[sentencetree]")
{
    const std::vector<FTTestSentence> Graded{
        {1, {{30, 20, 10}}, 1},
        {2, {{30, 20}}, 2},
        {3, {{30, 25, 10}}, 1},
        {4, {{40, 10}}, 1},
        {5, {{10}}, 1},
    };

    const FTSentenceTree Tree = ParseOrFail(Graded);
    const FTWordIdSet Everything = MakeWordSet({10, 20, 25, 30, 40});

    FTSentenceQuery Query;
    Query.LevelMask = MakeLevelMask(1);
    Query.MinLength = 2;

    CHECK(Tree.FindWritableSentences(Everything, Query) == std::vector<uint32_t>{1, 3, 4});
    CHECK(Tree.FindWritableSentences(MakeWordSet({40, 10}), Query) == std::vector<uint32_t>{4});

    for (uint64_t Seed = 0; Seed < 30; ++Seed)
    {
        Query.MaxCount = 2;
        Query.Seed = Seed;

        const std::vector<uint32_t> Picked = Tree.FindWritableSentences(Everything, Query);

        REQUIRE(Picked.size() == 2);
        CHECK(std::find(Picked.begin(), Picked.end(), 2u) == Picked.end());
        CHECK(std::find(Picked.begin(), Picked.end(), 5u) == Picked.end());
    }
}

TEST_CASE("Both readings of an ambiguous sentence carry its grammar level", "[sentencetree]")
{
    const std::vector<FTTestSentence> Suis{
        {7, {{50, 1}, {60, 1}}, 3},
    };

    const FTSentenceTree Tree = ParseOrFail(Suis);

    FTSentenceQuery Easy;
    Easy.LevelMask = MakeLevelMask(1) | MakeLevelMask(2);

    FTSentenceQuery Hard;
    Hard.LevelMask = MakeLevelMask(3);

    CHECK(Tree.FindWritableSentences(MakeWordSet({50, 1}), Easy).empty());
    CHECK(Tree.FindWritableSentences(MakeWordSet({60, 1}), Easy).empty());
    CHECK(Tree.FindWritableSentences(MakeWordSet({50, 1}), Hard) == std::vector<uint32_t>{7});
    CHECK(Tree.FindWritableSentences(MakeWordSet({60, 1}), Hard) == std::vector<uint32_t>{7});
}

TEST_CASE("A blob without its level bytes is rejected", "[sentencetree]")
{
    const std::string Blob = BuildBlob(Kitchen);

    CHECK_FALSE(FTSentenceTree::Parse(Blob.substr(0, Blob.size() - 8)).has_value());
}

TEST_CASE("Words outside the tree do not make anything writable", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    CHECK(Tree.FindWritableSentences(MakeWordSet({1, 2, 3, 99999})).empty());
}

TEST_CASE("An ambiguous sentence is writable through either of its readings", "[sentencetree]")
{
    const std::vector<FTTestSentence> Suis{
        {7, {{50, 1}, {60, 1}}},
        {8, {{60, 2}}},
    };

    const FTSentenceTree Tree = ParseOrFail(Suis);

    CHECK(Tree.FindWritableSentences(MakeWordSet({50, 1})) == std::vector<uint32_t>{7});
    CHECK(Tree.FindWritableSentences(MakeWordSet({60, 1})) == std::vector<uint32_t>{7});
    CHECK(Tree.FindWritableSentences(MakeWordSet({50, 60, 1, 2})) == std::vector<uint32_t>{7, 8});
    CHECK(Tree.FindWritableSentences(MakeWordSet({50, 60})).empty());
}

TEST_CASE("Two sentences reduced to the same words both come back", "[sentencetree]")
{
    const std::vector<FTTestSentence> Twins{
        {11, {{5, 3}}},
        {12, {{5, 3}}},
    };

    const FTSentenceTree Tree = ParseOrFail(Twins);

    CHECK(Tree.FindWritableSentences(MakeWordSet({3, 5})) == std::vector<uint32_t>{11, 12});
    CHECK(Tree.GetNodeCount() == 3);
    CHECK(Tree.GetSentenceReferenceCount() == 2);
}

TEST_CASE("A tree with no sentences parses and finds nothing", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail({});

    CHECK(Tree.GetNodeCount() == 1);
    CHECK(Tree.FindWritableSentences(MakeWordSet({1, 2, 3})).empty());
}

TEST_CASE("The tree reports the memory its arrays hold", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    const size_t Nodes = Tree.GetNodeCount();
    const size_t References = Tree.GetSentenceReferenceCount();

    CHECK(Nodes == 9);
    CHECK(References == 5);
    CHECK(Tree.GetMemoryBytes() == 4 * (Nodes + 2 * (Nodes + 1) + References) + References);
}

TEST_CASE("A blob that is not a sentence tree is rejected", "[sentencetree]")
{
    const std::string Blob = BuildBlob(Kitchen);

    SECTION("Wrong magic")
    {
        std::string Broken = Blob;
        Broken[0] = 'X';

        CHECK_FALSE(FTSentenceTree::Parse(Broken).has_value());
    }

    SECTION("Unknown format")
    {
        std::string Broken = Blob;
        OverwriteUint32(Broken, 4, FTSentenceTree::Format + 1);

        CHECK_FALSE(FTSentenceTree::Parse(Broken).has_value());
    }

    SECTION("Truncated")
    {
        CHECK_FALSE(FTSentenceTree::Parse(Blob.substr(0, Blob.size() - 4)).has_value());
        CHECK_FALSE(FTSentenceTree::Parse(Blob.substr(0, 10)).has_value());
        CHECK_FALSE(FTSentenceTree::Parse(std::string()).has_value());
    }

    SECTION("Node count that does not match the size")
    {
        std::string Broken = Blob;
        OverwriteUint32(Broken, 8, 1000);

        CHECK_FALSE(FTSentenceTree::Parse(Broken).has_value());
    }

    SECTION("Sentence ranges out of order")
    {
        const size_t Nodes = 9;
        const size_t FirstSentencesOffset = HeaderBytes + 4 * (Nodes + Nodes + 1);

        std::string Broken = Blob;
        OverwriteUint32(Broken, FirstSentencesOffset + 4, 4);

        CHECK_FALSE(FTSentenceTree::Parse(Broken).has_value());
    }

    SECTION("Children that do not start after the root")
    {
        const size_t Nodes = 9;
        const size_t FirstChildrenOffset = HeaderBytes + 4 * Nodes;

        std::string Broken = Blob;
        OverwriteUint32(Broken, FirstChildrenOffset, 0);

        CHECK_FALSE(FTSentenceTree::Parse(Broken).has_value());
    }
}

TEST_CASE("The word id set holds ids of any size", "[sentencetree]")
{
    FTWordIdSet Set;

    CHECK_FALSE(Set.Contains(0));
    CHECK_FALSE(Set.Contains(1000000));

    Set.Add(0);
    Set.Add(63);
    Set.Add(64);
    Set.Add(71415);
    Set.Add(71415);

    CHECK(Set.Contains(0));
    CHECK(Set.Contains(63));
    CHECK(Set.Contains(64));
    CHECK(Set.Contains(71415));
    CHECK_FALSE(Set.Contains(1));
    CHECK_FALSE(Set.Contains(65));
    CHECK_FALSE(Set.Contains(71416));
    CHECK_FALSE(Set.Contains(4000000000u));
    CHECK(Set.GetCount() == 4);
}

TEST_CASE("The tree and its sentences load from a dictionary database", "[sentencetree]")
{
    std::optional<FTDatabase> Database = FTDatabase::OpenInMemory();
    REQUIRE(Database.has_value());

    SECTION("A dictionary without a sentence index has no tree")
    {
        CHECK_FALSE(FTSentenceTree::Load(*Database).has_value());
    }

    SECTION("A dictionary with one loads it")
    {
        REQUIRE(Database->Execute(
            "CREATE TABLE words (id INTEGER PRIMARY KEY, headword TEXT NOT NULL COLLATE NOCASE)"));
        REQUIRE(Database->Execute(
            "CREATE TABLE sentences (id INTEGER PRIMARY KEY, source_id INTEGER, "
            "language_id INTEGER, text TEXT NOT NULL, translation TEXT, gap_start INTEGER, "
            "gap_length INTEGER, level INTEGER NOT NULL DEFAULT 1, "
            "features INTEGER NOT NULL DEFAULT 0, changed_count INTEGER NOT NULL DEFAULT 0)"));
        REQUIRE(Database->Execute(
            "CREATE TABLE sentence_tree (id INTEGER PRIMARY KEY CHECK (id = 1), "
            "format INTEGER NOT NULL, data BLOB NOT NULL)"));
        REQUIRE(Database->Execute(
            "INSERT INTO words (id, headword) VALUES "
            "(10, 'penser'), (20, 'tête'), (30, 'manger'), (31, 'Manger')"));
        REQUIRE(Database->Execute(
            "INSERT INTO sentences (id, text, translation, gap_start, gap_length, level, "
            "changed_count) VALUES "
            "(1, 'Je pense avec ma tête.', 'I think with my head.', 17, 4, 1, 1), "
            "(2, 'Je pense.', 'I think.', 3, 5, 1, 1), "
            "(3, 'Je mangeais.', NULL, NULL, NULL, 2, 1)"));

        const std::vector<FTTestSentence> Sentences{
            {1, {{20, 10}}},
            {2, {{10}}},
            {3, {{30}}},
        };

        REQUIRE(Database->Execute("INSERT INTO sentence_tree (id, format, data) VALUES (1, 1, X'"
                                  + ToHex(BuildBlob(Sentences)) + "')"));

        const std::optional<FTSentenceTree> Tree = FTSentenceTree::Load(*Database);
        REQUIRE(Tree.has_value());

        const FTDictionaryRepository Repository(*Database);

        const FTWordIdSet Known = Repository.ResolveWordIds({"penser", "TÊTE", "tête", "MANGER"});

        CHECK(Known.Contains(10));
        CHECK(Known.Contains(20));
        CHECK(Known.Contains(30));
        CHECK(Known.Contains(31));
        CHECK(Known.GetCount() == 4);

        CHECK(Repository.ListWordIds("manger") == std::vector<uint32_t>{30, 31});
        CHECK(Repository.ListWordIds("tête") == std::vector<uint32_t>{20});
        CHECK(Repository.ListWordIds("absent").empty());

        const std::vector<uint32_t> Found = Tree->FindWritableSentences(Known);
        REQUIRE(Found == std::vector<uint32_t>{1, 2, 3});

        const std::vector<FTTreeSentence> Loaded = Repository.LoadTreeSentences(Found);
        REQUIRE(Loaded.size() == 3);
        CHECK(Loaded[0].Text == "Je pense avec ma tête.");
        CHECK(Loaded[0].Translation == "I think with my head.");
        CHECK(Loaded[0].GapStart == 17);
        CHECK(Loaded[0].GapLength == 4);
        CHECK(Loaded[0].Level == 1);
        CHECK(Loaded[0].ChangedCount == 1);
        CHECK(Loaded[2].Level == 2);
        CHECK(Loaded[2].Text == "Je mangeais.");
        CHECK(Loaded[2].Translation.empty());
        CHECK(Loaded[2].GapStart == -1);
        CHECK(Loaded[2].GapLength == 0);
    }

    SECTION("A malformed tree is not loaded")
    {
        REQUIRE(Database->Execute(
            "CREATE TABLE sentence_tree (id INTEGER PRIMARY KEY, format INTEGER, data BLOB)"));
        REQUIRE(Database->Execute("INSERT INTO sentence_tree VALUES (1, 1, X'494B5354')"));

        CHECK_FALSE(FTSentenceTree::Load(*Database).has_value());
    }
}

namespace
{
    struct FTRealTree
    {
        std::optional<FTDatabase> Database;

        std::optional<FTSentenceTree> Tree;

        std::string Blob;
    };

    FTRealTree OpenRealTree()
    {
        FTRealTree Result;
        Result.Database = FTDatabase::OpenReadOnly(DICTIONARY_DATABASE_PATH);

        if (!Result.Database.has_value())
        {
            return Result;
        }

        Result.Tree = FTSentenceTree::Load(*Result.Database);

        FTStatement Query = Result.Database->Prepare("SELECT data FROM sentence_tree WHERE id = 1");

        if (Query.IsValid() && Query.Step())
        {
            Result.Blob = Query.GetBlob(0);
        }

        return Result;
    }

    std::vector<uint32_t> LoadTopTaughtWords(FTDatabase& Database, int Count)
    {
        std::vector<uint32_t> Result;

        FTStatement Query = Database.Prepare(
            "SELECT id FROM words WHERE teaching_rank IS NOT NULL "
            "ORDER BY teaching_rank, id LIMIT ?1");
        REQUIRE(Query.IsValid());

        Query.BindInt(1, Count);

        while (Query.Step())
        {
            Result.push_back(static_cast<uint32_t>(Query.GetInt64(0)));
        }

        return Result;
    }

    constexpr uint32_t EveryTense[] = {1, 2, 3, 4, 8};

    FTWordIdSet ToWordSet(const std::vector<uint32_t>& WordIds)
    {
        FTWordIdSet Result;

        for (const uint32_t WordId : WordIds)
        {
            Result.Add(WordId);
        }

        return Result;
    }

    FTWordIdSet ToWordSetWithTenses(const FTSentenceTree& Tree, const std::vector<uint32_t>& WordIds)
    {
        FTWordIdSet Result = ToWordSet(WordIds);

        for (const uint32_t WordId : WordIds)
        {
            for (const uint32_t Tense : EveryTense)
            {
                Result.Add(Tree.MakeTenseKey(WordId, Tense));
            }
        }

        return Result;
    }

    std::map<uint32_t, uint8_t> DecodeLevels(const std::string& Blob)
    {
        const auto Read = [&](size_t Offset)
        {
            uint32_t Value = 0;

            for (int Index = 3; Index >= 0; --Index)
            {
                Value = (Value << 8) | static_cast<unsigned char>(Blob[Offset + Index]);
            }

            return Value;
        };

        const size_t Nodes = Read(8);
        const size_t References = Read(12);
        const size_t IdsOffset = HeaderBytes + 4 * (Nodes + 2 * (Nodes + 1));
        const size_t LevelsOffset = IdsOffset + 4 * References;

        std::map<uint32_t, uint8_t> Levels;

        for (size_t Reference = 0; Reference < References; ++Reference)
        {
            Levels[Read(IdsOffset + 4 * Reference)] = static_cast<uint8_t>(Blob[LevelsOffset + Reference]);
        }

        return Levels;
    }

    std::vector<std::pair<uint32_t, std::vector<uint32_t>>> DecodePaths(const std::string& Blob)
    {
        const auto Read = [&](size_t Offset)
        {
            uint32_t Value = 0;

            for (int Index = 3; Index >= 0; --Index)
            {
                Value = (Value << 8) | static_cast<unsigned char>(Blob[Offset + Index]);
            }

            return Value;
        };

        const size_t Nodes = Read(8);
        const size_t WordsOffset = HeaderBytes;
        const size_t ChildrenOffset = WordsOffset + 4 * Nodes;
        const size_t SentencesOffset = ChildrenOffset + 4 * (Nodes + 1);
        const size_t IdsOffset = SentencesOffset + 4 * (Nodes + 1);

        std::vector<std::pair<uint32_t, std::vector<uint32_t>>> Paths;
        std::vector<std::pair<size_t, std::vector<uint32_t>>> Pending{{0, {}}};

        while (!Pending.empty())
        {
            auto [Node, Path] = std::move(Pending.back());
            Pending.pop_back();

            for (size_t Reference = Read(SentencesOffset + 4 * Node);
                 Reference < Read(SentencesOffset + 4 * (Node + 1)); ++Reference)
            {
                Paths.emplace_back(Read(IdsOffset + 4 * Reference), Path);
            }

            for (size_t Child = Read(ChildrenOffset + 4 * Node);
                 Child < Read(ChildrenOffset + 4 * (Node + 1)); ++Child)
            {
                std::vector<uint32_t> Longer = Path;
                Longer.push_back(Read(WordsOffset + 4 * Child));
                Pending.emplace_back(Child, std::move(Longer));
            }
        }

        return Paths;
    }
}

TEST_CASE("The built dictionary's tree agrees with a brute-force search", "[sentencetree][slow]")
{
    FTRealTree Real = OpenRealTree();

    if (!Real.Tree.has_value())
    {
        SKIP("The built dictionary has no sentence index");
    }

    const std::vector<std::pair<uint32_t, std::vector<uint32_t>>> Paths = DecodePaths(Real.Blob);
    REQUIRE(Paths.size() == Real.Tree->GetSentenceReferenceCount());

    for (const int VocabularySize : {500, 3000, 10000})
    {
        const std::vector<uint32_t> Vocabulary = LoadTopTaughtWords(*Real.Database, VocabularySize);
        const FTWordIdSet Known = ToWordSetWithTenses(*Real.Tree, Vocabulary);

        std::set<uint32_t> Expected;

        for (const auto& [SentenceId, Path] : Paths)
        {
            const bool bWritable = std::all_of(Path.begin(), Path.end(),
                [&](uint32_t WordId)
                {
                    return Known.Contains(WordId);
                });

            if (bWritable)
            {
                Expected.insert(SentenceId);
            }
        }

        const std::vector<uint32_t> Found = Real.Tree->FindWritableSentences(Known);

        INFO("vocabulary " << VocabularySize);
        CHECK_FALSE(Found.empty());
        CHECK(Found == std::vector<uint32_t>(Expected.begin(), Expected.end()));
    }
}

TEST_CASE("The built dictionary's tree honours length and count", "[sentencetree][slow]")
{
    FTRealTree Real = OpenRealTree();

    if (!Real.Tree.has_value())
    {
        SKIP("The built dictionary has no sentence index");
    }

    const std::vector<std::pair<uint32_t, std::vector<uint32_t>>> Paths = DecodePaths(Real.Blob);
    const FTWordIdSet Known =
        ToWordSetWithTenses(*Real.Tree, LoadTopTaughtWords(*Real.Database, 3000));

    FTSentenceQuery Query;
    Query.MinLength = 2;
    Query.MaxLength = 4;

    std::set<uint32_t> Expected;

    for (const auto& [SentenceId, Path] : Paths)
    {
        const auto Words = static_cast<uint32_t>(std::count_if(Path.begin(), Path.end(),
            [&](uint32_t Key)
            {
                return Real.Tree->IsWordKey(Key);
            }));

        const bool bInBounds = Words >= Query.MinLength && Words <= Query.MaxLength;
        const bool bWritable = std::all_of(Path.begin(), Path.end(),
            [&](uint32_t WordId)
            {
                return Known.Contains(WordId);
            });

        if (bInBounds && bWritable)
        {
            Expected.insert(SentenceId);
        }
    }

    const std::vector<uint32_t> Bounded = Real.Tree->FindWritableSentences(Known, Query);
    CHECK(Bounded == std::vector<uint32_t>(Expected.begin(), Expected.end()));

    Query.MaxCount = 20;
    Query.Seed = 7;

    const std::vector<uint32_t> Picked = Real.Tree->FindWritableSentences(Known, Query);
    REQUIRE(Picked.size() == 20);
    CHECK(std::includes(Bounded.begin(), Bounded.end(), Picked.begin(), Picked.end()));

    Query.Seed = 8;
    CHECK(Real.Tree->FindWritableSentences(Known, Query) != Picked);
}

TEST_CASE("The built dictionary's tree carries the grammar level of each sentence",
          "[sentencetree][slow]")
{
    FTRealTree Real = OpenRealTree();

    if (!Real.Tree.has_value())
    {
        SKIP("The built dictionary has no sentence index");
    }

    const std::map<uint32_t, uint8_t> TreeLevels = DecodeLevels(Real.Blob);

    std::map<int, size_t> CountByLevel;
    size_t Mismatched = 0;

    FTStatement Query = Real.Database->Prepare("SELECT id, level FROM sentences");
    REQUIRE(Query.IsValid());

    while (Query.Step())
    {
        const auto Found = TreeLevels.find(static_cast<uint32_t>(Query.GetInt64(0)));
        const int Level = Query.GetInt(1);

        ++CountByLevel[Level];

        if (Found == TreeLevels.end() || Found->second != Level)
        {
            ++Mismatched;
        }
    }

    CHECK(Mismatched == 0);
    REQUIRE(CountByLevel.size() == 4);
    CHECK(CountByLevel.begin()->first == 1);
    CHECK(CountByLevel.rbegin()->first == 4);
    CHECK(CountByLevel[1] > CountByLevel[4]);

    const std::vector<std::pair<uint32_t, std::vector<uint32_t>>> Paths = DecodePaths(Real.Blob);
    const FTWordIdSet Known = ToWordSet(LoadTopTaughtWords(*Real.Database, 3000));

    FTSentenceQuery Easy;
    Easy.LevelMask = MakeLevelMask(1);

    std::set<uint32_t> Expected;

    for (const auto& [SentenceId, Path] : Paths)
    {
        const bool bWritable = std::all_of(Path.begin(), Path.end(),
            [&](uint32_t WordId)
            {
                return Known.Contains(WordId);
            });

        if (bWritable && TreeLevels.at(SentenceId) == 1)
        {
            Expected.insert(SentenceId);
        }
    }

    const std::vector<uint32_t> Found = Real.Tree->FindWritableSentences(Known, Easy);

    CHECK_FALSE(Found.empty());
    CHECK(Found == std::vector<uint32_t>(Expected.begin(), Expected.end()));
    CHECK(Found.size() < Real.Tree->FindWritableSentences(Known).size());
}

TEST_CASE("Known sentences of the built dictionary sit at the level of their grammar",
          "[sentencetree]")
{
    FTRealTree Real = OpenRealTree();

    if (!Real.Tree.has_value())
    {
        SKIP("The built dictionary has no sentence index");
    }

    const auto LevelOf = [&](const std::string& Text)
    {
        FTStatement Query = Real.Database->Prepare("SELECT level FROM sentences WHERE text = ?1");
        Query.BindText(1, Text);

        return Query.IsValid() && Query.Step() ? Query.GetInt(0) : 0;
    };

    CHECK(LevelOf("Je pense avec ma tête.") == 1);
    CHECK(LevelOf("Mon frère travaille dans une banque.") == 1);
    CHECK(LevelOf("Je vis dans un appartement.") == 1);
    CHECK(LevelOf("Je suis née à Osaka en 1977.") == 2);
    CHECK(LevelOf("J'ai entendu un bruit derrière moi.") == 2);
    CHECK(LevelOf("Je l'ai acheté hier.") == 2);
    CHECK(LevelOf("Je veux savoir qui vous a dit cela.") == 3);
    CHECK(LevelOf("Je voudrais faire une réservation pour ce soir.") == 3);
    CHECK(LevelOf("Notre équipe remporta une grande victoire.") == 4);
}

TEST_CASE("Every path in the built tree is sorted rarest word first", "[sentencetree][slow]")
{
    FTRealTree Real = OpenRealTree();

    if (!Real.Tree.has_value())
    {
        SKIP("The built dictionary has no sentence index");
    }

    std::map<uint32_t, int> Ranks;

    FTStatement Query = Real.Database->Prepare(
        "SELECT id, COALESCE(teaching_rank, 1073741824) FROM words");
    REQUIRE(Query.IsValid());

    while (Query.Step())
    {
        Ranks[static_cast<uint32_t>(Query.GetInt64(0))] = Query.GetInt(1);
    }

    const uint32_t WordSpan = Real.Tree->GetWordSpan();
    REQUIRE(WordSpan > 0);

    size_t Unsorted = 0;
    size_t MissingWords = 0;
    size_t TenseKeys = 0;
    size_t TenseKeysWithoutWord = 0;

    for (const auto& [SentenceId, Path] : DecodePaths(Real.Blob))
    {
        for (size_t Index = 0; Index < Path.size(); ++Index)
        {
            const uint32_t Word = Path[Index] % WordSpan;

            if (Ranks.count(Word) == 0)
            {
                ++MissingWords;
            }

            if (Index > 0 && Ranks[Path[Index - 1] % WordSpan] < Ranks[Word])
            {
                ++Unsorted;
            }

            if (Real.Tree->IsWordKey(Path[Index]))
            {
                continue;
            }

            ++TenseKeys;

            if (std::find(Path.begin(), Path.begin() + Index, Word) == Path.begin() + Index)
            {
                ++TenseKeysWithoutWord;
            }
        }
    }

    CHECK(MissingWords == 0);
    CHECK(Unsorted == 0);
    CHECK(TenseKeys > 0);
    CHECK(TenseKeysWithoutWord == 0);
}

TEST_CASE("Every sentence the built tree names has its text and translation", "[sentencetree][slow]")
{
    FTRealTree Real = OpenRealTree();

    if (!Real.Tree.has_value())
    {
        SKIP("The built dictionary has no sentence index");
    }

    std::set<uint32_t> Named;

    for (const auto& [SentenceId, Path] : DecodePaths(Real.Blob))
    {
        Named.insert(SentenceId);
    }

    FTStatement Query = Real.Database->Prepare(
        "SELECT COUNT(*), MIN(id), MAX(id) FROM sentences "
        "WHERE text <> '' AND translation IS NOT NULL AND translation <> ''");
    REQUIRE(Query.IsValid());
    REQUIRE(Query.Step());

    CHECK(static_cast<size_t>(Query.GetInt64(0)) == Named.size());
    CHECK(static_cast<uint32_t>(Query.GetInt64(1)) == *Named.begin());
    CHECK(static_cast<uint32_t>(Query.GetInt64(2)) == *Named.rbegin());
}

TEST_CASE("Knowing a handful of words writes the sentences built from them", "[sentencetree]")
{
    FTRealTree Real = OpenRealTree();

    if (!Real.Tree.has_value())
    {
        SKIP("The built dictionary has no sentence index");
    }

    const FTDictionaryRepository Repository(*Real.Database);

    const std::vector<std::string> Vocabulary{
        "penser", "tête", "être", "avoir", "faire", "aller", "manger", "pomme", "aimer",
        "chat", "chien", "pas", "très", "bien", "voir", "vouloir", "pouvoir", "livre"};

    std::vector<std::string> WithSmallWords = Vocabulary;
    WithSmallWords.insert(WithSmallWords.end(), {"je", "tu", "il", "avec", "mon", "ma", "et", "de"});

    const uint32_t Present = 1;

    const auto Write = [&](const std::vector<std::string>& Headwords,
                           const std::vector<std::string>& PresentVerbs = {})
    {
        FTWordIdSet Known = Repository.ResolveWordIds(Headwords);

        for (const std::string& Verb : PresentVerbs)
        {
            for (const uint32_t WordId : Repository.ListWordIds(Verb))
            {
                Known.Add(Real.Tree->MakeTenseKey(WordId, Present));
            }
        }

        return Repository.LoadTreeSentences(Real.Tree->FindWritableSentences(Known));
    };

    const auto Contains = [](const std::vector<FTTreeSentence>& Sentences, const std::string& Text)
    {
        return std::any_of(Sentences.begin(), Sentences.end(),
            [&](const FTTreeSentence& Sentence)
            {
                return Sentence.Text == Text;
            });
    };

    const std::vector<FTTreeSentence> Sentences = Write(WithSmallWords, {"penser"});

    REQUIRE_FALSE(Sentences.empty());

    CHECK(Contains(Sentences, "Je pense avec ma tête."));
    CHECK_FALSE(Contains(Sentences, "Je n'ai aucun stylo."));
    CHECK_FALSE(Contains(Write(Vocabulary, {"penser"}), "Je pense avec ma tête."));
    CHECK(Contains(Write(WithSmallWords), "Je pense avec ma tête."));
    CHECK_FALSE(Contains(Write(WithSmallWords), "Je veux une pomme."));
    CHECK_FALSE(Contains(Write(WithSmallWords, {"aimer"}), "Je veux une pomme."));
    CHECK(Contains(Write(WithSmallWords, {"vouloir"}), "Je veux une pomme."));

    for (const FTTreeSentence& Sentence : Sentences)
    {
        CHECK_FALSE(Sentence.Translation.empty());
    }
}

TEST_CASE("The built dictionary's gaps name a word inside the sentence", "[sentencetree]")
{
    FTRealTree Real = OpenRealTree();

    if (!Real.Tree.has_value())
    {
        SKIP("The built dictionary has no sentence index");
    }

    FTStatement Count = Real.Database->Prepare("SELECT MAX(id) FROM sentences");
    REQUIRE(Count.IsValid());
    REQUIRE(Count.Step());

    std::vector<uint32_t> Sampled;

    for (uint32_t SentenceId = 1; SentenceId <= static_cast<uint32_t>(Count.GetInt64(0));
         SentenceId += 37)
    {
        Sampled.push_back(SentenceId);
    }

    const FTDictionaryRepository Repository(*Real.Database);
    const std::vector<FTTreeSentence> Sentences = Repository.LoadTreeSentences(Sampled);

    REQUIRE(Sentences.size() == Sampled.size());

    size_t WithGap = 0;

    for (const FTTreeSentence& Sentence : Sentences)
    {
        if (Sentence.GapStart < 0)
        {
            continue;
        }

        ++WithGap;

        const std::string Word = GetGapWord(Sentence);
        const std::vector<std::string> Words = SplitSentenceWords(Word);

        INFO(Sentence.Text);
        CHECK(HasGap(Sentence));
        REQUIRE(Words.size() == 1);
        CHECK(Words.front() == Word);
        CHECK(MakeGapPrompt(Sentence).find("___") != std::string::npos);
    }

    CHECK(WithGap * 100 > Sentences.size() * 95);
}

TEST_CASE("Sentence tree statistics and query timing", "[.][sentencetree-benchmark]")
{
    using FTClock = std::chrono::steady_clock;

    const FTClock::time_point LoadStart = FTClock::now();
    FTRealTree Real = OpenRealTree();
    const double LoadMilliseconds =
        std::chrono::duration<double, std::milli>(FTClock::now() - LoadStart).count();

    if (!Real.Tree.has_value())
    {
        SKIP("The built dictionary has no sentence index");
    }

    std::printf("nodes: %zu\n", Real.Tree->GetNodeCount());
    std::printf("sentence references: %zu\n", Real.Tree->GetSentenceReferenceCount());
    std::printf("tree memory: %zu bytes\n", Real.Tree->GetMemoryBytes());
    std::printf("open and load: %.1f ms\n", LoadMilliseconds);

    constexpr int Repetitions = 200;

    for (const int VocabularySize : {500, 3000, 10000})
    {
        const FTWordIdSet Known = ToWordSet(LoadTopTaughtWords(*Real.Database, VocabularySize));

        std::vector<double> Timings;
        size_t Found = 0;

        for (int Repetition = 0; Repetition < Repetitions; ++Repetition)
        {
            const FTClock::time_point Start = FTClock::now();
            Found = Real.Tree->FindWritableSentences(Known).size();
            Timings.push_back(
                std::chrono::duration<double, std::micro>(FTClock::now() - Start).count());
        }

        std::sort(Timings.begin(), Timings.end());

        std::printf("vocabulary %5d: %7zu sentences, median %8.1f us, min %8.1f us, max %8.1f us\n",
                    VocabularySize, Found, Timings[Timings.size() / 2], Timings.front(),
                    Timings.back());
    }

    for (const int VocabularySize : {500, 3000, 10000})
    {
        const FTWordIdSet Known = ToWordSet(LoadTopTaughtWords(*Real.Database, VocabularySize));

        FTSentenceQuery Query;
        Query.MaxCount = 20;
        Query.MinLength = 2;
        Query.MaxLength = 6;

        std::vector<double> Timings;

        for (int Repetition = 0; Repetition < Repetitions; ++Repetition)
        {
            Query.Seed = static_cast<uint64_t>(Repetition);

            const FTClock::time_point Start = FTClock::now();
            Real.Tree->FindWritableSentences(Known, Query);
            Timings.push_back(
                std::chrono::duration<double, std::micro>(FTClock::now() - Start).count());
        }

        std::sort(Timings.begin(), Timings.end());

        std::printf("vocabulary %5d, 20 sentences of 2 to 6 words: median %8.1f us, "
                    "min %8.1f us, max %8.1f us\n",
                    VocabularySize, Timings[Timings.size() / 2], Timings.front(), Timings.back());
    }

    const FTDictionaryRepository Repository(*Real.Database);
    const FTWordIdSet Known = ToWordSet(LoadTopTaughtWords(*Real.Database, 3000));

    FTSentenceQuery Query;
    Query.MaxCount = 20;
    Query.MinLength = 2;
    Query.MaxLength = 6;
    Query.Seed = 2026;

    const std::vector<uint32_t> Found = Real.Tree->FindWritableSentences(Known, Query);

    const FTClock::time_point TextStart = FTClock::now();
    const std::vector<FTTreeSentence> Sentences = Repository.LoadTreeSentences(Found);
    const double TextMilliseconds =
        std::chrono::duration<double, std::milli>(FTClock::now() - TextStart).count();

    std::printf("load text of %zu sentences: %.2f ms\n", Sentences.size(), TextMilliseconds);

    for (const FTTreeSentence& Sentence : Sentences)
    {
        std::printf("  %s | %s\n", Sentence.Text.c_str(), Sentence.Translation.c_str());
    }

    SUCCEED();
}

TEST_CASE("A focused query keeps writable sentences holding a focus word and says which ones",
          "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    const std::vector<FTFocusedSentence> Found =
        Tree.FindFocusedSentences(MakeWordSet({10, 20, 30, 40}), {{20}, {10}});

    std::map<uint32_t, uint64_t> MaskBySentence;

    for (const FTFocusedSentence& Sentence : Found)
    {
        MaskBySentence[Sentence.SentenceId] = Sentence.FocusMask;
    }

    CHECK(MaskBySentence == std::map<uint32_t, uint64_t>{{1, 0b11}, {2, 0b01}, {4, 0b10}, {5, 0b10}});
}

TEST_CASE("A focused query with no focus words finds nothing", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    CHECK(Tree.FindFocusedSentences(MakeWordSet({10, 20, 30, 40}), {}).empty());
}

TEST_CASE("A search for words finds every sentence holding one, whatever else it needs", "[sentencetree]")
{
    const FTSentenceTree Tree = ParseOrFail(Kitchen);

    const std::vector<FTFocusedSentence> Found = Tree.FindSentencesWithWords({{25}, {40}});

    std::map<uint32_t, uint64_t> MaskBySentence;

    for (const FTFocusedSentence& Sentence : Found)
    {
        MaskBySentence[Sentence.SentenceId] = Sentence.FocusMask;
    }

    CHECK(MaskBySentence == std::map<uint32_t, uint64_t>{{3, 0b01}, {4, 0b10}});
}
