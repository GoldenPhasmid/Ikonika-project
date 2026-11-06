#include <catch2/catch_test_macros.hpp>

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTSchedule.h"
#include "core/FTSchema.h"
#include "core/FTUuid.h"
#include "core/FTWordForms.h"
#include "core/FTSentenceSplitter.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace
{
    /** A database and repository that live together, with a clock the test controls. */
    struct Fixture
    {
        Fixture()
        {
            std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
            REQUIRE(Opened.has_value());

            Target = std::make_unique<FTDatabase>(std::move(*Opened));
            REQUIRE(MigrateSchema(*Target));

            Repository = std::make_unique<FTCardRepository>(*Target, [this]() { return FakeNow; });
        }

        std::unique_ptr<FTDatabase> Target;
        std::unique_ptr<FTCardRepository> Repository;

        int64_t FakeNow = 1000;
    };

    FTCard MakeCard(std::string TargetWord, std::string Translation)
    {
        FTCard Result;
        Result.TargetWord = std::move(TargetWord);
        Result.Translation = std::move(Translation);

        return Result;
    }

    FTStageProbe MakeProbe(std::string TargetWord, std::string Translation)
    {
        FTStageProbe Result;
        Result.TargetWord = std::move(TargetWord);
        Result.Translation = std::move(Translation);

        return Result;
    }

    bool AddCards(FTCardRepository& Repository, const std::vector<FTCard>& Cards)
    {
        for (const FTCard& Card : Cards)
        {
            const std::optional<std::string> CardId = Repository.AddCard(Card);

            if (!CardId.has_value())
            {
                return false;
            }

            if (Card.LeitnerStage == PoolStage && !Card.DueDate.has_value() && !Card.CompletedDate.has_value())
            {
                continue;
            }

            FTCard Placed = Card;
            Placed.Id = *CardId;

            if (!Repository.UpdateCardSchedule(Placed))
            {
                return false;
            }
        }

        return true;
    }

    std::vector<std::string> CardIdsOf(const std::vector<FTCardHandle>& Cards)
    {
        std::vector<std::string> Ids;

        for (const FTCardHandle& Card : Cards)
        {
            Ids.push_back(Card->Id);
        }

        return Ids;
    }

    std::vector<FTCard> MakeThreeCards()
    {
        return {
            MakeCard("le chien", "dog"),
            MakeCard("la maison", "house"),
            MakeCard("le café", "coffee")
        };
    }
}

TEST_CASE("A word already added is found, with the box it reached", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog")}));

    // In the pool, so stage 0 rather than nothing: it has been added, just not learned.
    REQUIRE(Test.Repository->FindWordStage("chien", "").has_value());
    CHECK(*Test.Repository->FindWordStage("chien", "") == 0);

    CHECK_FALSE(Test.Repository->FindWordStage("chat", "").has_value());

    // Matched however it was typed, since the user is warned about their own spelling.
    CHECK(Test.Repository->FindWordStage("CHIEN", "").has_value());

    FTCard Card = *Test.Repository->LoadCards().front();
    Card.LeitnerStage = 3;
    Card.DueDate = 20000;
    REQUIRE(Test.Repository->UpdateCardSchedule(Card));

    CHECK(*Test.Repository->FindWordStage("chien", "") == 3);
}

TEST_CASE("A word with two cards reports the box it is furthest along in", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog"), MakeCard("chien", "hound")}));

    FTCard Ahead = *Test.Repository->LoadCards().back();
    Ahead.LeitnerStage = 4;
    Ahead.DueDate = 20000;
    REQUIRE(Test.Repository->UpdateCardSchedule(Ahead));

    CHECK(*Test.Repository->FindWordStage("chien", "") == 4);
}

TEST_CASE("A deleted word is not reported as added", "[repository]")
{
    Fixture Test;

    const std::optional<std::string> CardId = Test.Repository->AddCard(MakeCard("chien", "dog"));
    REQUIRE(CardId.has_value());

    REQUIRE(Test.Repository->FindWordStage("chien", "").has_value());

    REQUIRE(Test.Repository->DeleteCards({*CardId}));

    CHECK_FALSE(Test.Repository->FindWordStage("chien", "").has_value());
}

TEST_CASE("A blank word is never reported as added", "[repository]")
{
    Fixture Test;

    CHECK_FALSE(Test.Repository->FindWordStage("", "").has_value());
    CHECK_FALSE(Test.Repository->FindWordStage("   ", "").has_value());
}

TEST_CASE("A batch finds the words paired with translations they already carry", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog")}));

    // The same word with the right meaning is found; with the wrong meaning, or for another word,
    // it is not. All in one call, in the same order they were asked in.
    const std::vector<std::optional<int>> Stages = Test.Repository->FindWordTranslationStages(
        {MakeProbe("chien", "dog"), MakeProbe("chien", "hound"), MakeProbe("chat", "dog")});

    REQUIRE(Stages.size() == 3);
    CHECK(Stages[0].has_value());
    CHECK_FALSE(Stages[1].has_value());
    CHECK_FALSE(Stages[2].has_value());
}

TEST_CASE("A batch matches however the word or translation was typed", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog")}));

    const std::vector<std::optional<int>> Stages = Test.Repository->FindWordTranslationStages(
        {MakeProbe("CHIEN", "DOG"), MakeProbe("Chien", " Dog ")});

    REQUIRE(Stages.size() == 2);
    CHECK(Stages[0].has_value());
    CHECK(Stages[1].has_value());
}

TEST_CASE("A batch finds a translation inside a card's comma-separated list", "[repository]")
{
    Fixture Test;


    // Picking several suggestions produces exactly this shape, so any one of them counts as
    // already covered rather than only the whole string matching.
    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog, hound, doggy")}));

    const std::vector<std::optional<int>> Stages = Test.Repository->FindWordTranslationStages(
        {MakeProbe("chien", "dog"), MakeProbe("chien", "hound"), MakeProbe("chien", "doggy"),
         MakeProbe("chien", " hound "), MakeProbe("chien", "dogg"), MakeProbe("chien", "og")});

    REQUIRE(Stages.size() == 6);

    // Any one of the stored parts counts, whitespace around it included.
    CHECK(Stages[0].has_value());
    CHECK(Stages[1].has_value());
    CHECK(Stages[2].has_value());
    CHECK(Stages[3].has_value());

    // A substring of a part is not a part.
    CHECK_FALSE(Stages[4].has_value());
    CHECK_FALSE(Stages[5].has_value());
}

TEST_CASE("A batch reports the furthest box each word sits in", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog"), MakeCard("chien", "dog")}));

    FTCard Ahead = *Test.Repository->LoadCards().back();
    Ahead.LeitnerStage = 5;
    Ahead.DueDate = 20000;
    REQUIRE(Test.Repository->UpdateCardSchedule(Ahead));

    const std::vector<std::optional<int>> Stages = Test.Repository->FindWordTranslationStages(
        {MakeProbe("chien", "dog")});

    REQUIRE(Stages.size() == 1);
    REQUIRE(Stages[0].has_value());
    CHECK(*Stages[0] == 5);
}

TEST_CASE("A batch matches the reading, not just the spelling", "[repository]")
{
    Fixture Test;

    // Two readings of the same spelling, sharing a translation only the noun's card actually
    // carries. A card for the noun must not make the verb read as already covered.
    FTCard Noun = MakeCard("livre", "book");
    Noun.PartOfSpeech = "noun";
    Noun.Gender = GenderMasculine;

    REQUIRE(AddCards(*Test.Repository, {Noun}));

    FTStageProbe NounProbe = MakeProbe("livre", "book");
    NounProbe.PartOfSpeech = "noun";
    NounProbe.Gender = GenderMasculine;

    FTStageProbe VerbProbe = MakeProbe("livre", "book");
    VerbProbe.PartOfSpeech = "verb";

    const std::vector<std::optional<int>> Stages = Test.Repository->FindWordTranslationStages(
        {NounProbe, VerbProbe});

    REQUIRE(Stages.size() == 2);
    CHECK(Stages[0].has_value());
    CHECK_FALSE(Stages[1].has_value());
}

TEST_CASE("A batch leaves blank words and translations unanswered", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog")}));

    const std::vector<std::optional<int>> Stages = Test.Repository->FindWordTranslationStages(
        {MakeProbe("", "dog"), MakeProbe("chien", ""), MakeProbe("chien", "   "),
         MakeProbe("chien", "dog")});

    REQUIRE(Stages.size() == 4);
    CHECK_FALSE(Stages[0].has_value());
    CHECK_FALSE(Stages[1].has_value());
    CHECK_FALSE(Stages[2].has_value());

    // The valid one in the same batch still answers.
    CHECK(Stages[3].has_value());
}

TEST_CASE("An empty batch returns no stages", "[repository]")
{
    Fixture Test;

    CHECK(Test.Repository->FindWordTranslationStages({}).empty());
}

TEST_CASE("A card for one reading of a spelling does not cover another reading's translation",
          "[repository]")
{
    Fixture Test;

    // Two readings of the same spelling, sharing a translation only the noun's card actually
    // carries. A card for the noun must not make the verb read as already covered.
    FTCard Noun = MakeCard("livre", "book");
    Noun.PartOfSpeech = "noun";
    Noun.Gender = GenderMasculine;

    REQUIRE(AddCards(*Test.Repository, {Noun}));

    CHECK(Test.Repository->FindWordTranslationStage("livre", "noun", GenderMasculine, "book")
              .has_value());

    CHECK_FALSE(Test.Repository->FindWordTranslationStage("livre", "verb", std::nullopt, "book")
                    .has_value());

    CHECK(Test.Repository->FindWordTranslationStageAnyReading("livre", "book").has_value());

    const std::optional<FTWordTranslationMatch> Match =
        Test.Repository->FindWordTranslationMatch("livre", "noun", "book");
    REQUIRE(Match.has_value());
    CHECK_FALSE(Match->CardId.empty());

    CHECK_FALSE(Test.Repository->FindWordTranslationMatch("livre", "verb", "book").has_value());
}

TEST_CASE("A card with no part of speech covers every reading of its spelling", "[repository]")
{
    Fixture Test;

    const FTCard Untyped = MakeCard("partido", "game");

    REQUIRE(AddCards(*Test.Repository, {Untyped}));

    CHECK(Test.Repository->FindWordTranslationMatch("partido", "noun", "game").has_value());
    CHECK(Test.Repository->FindWordTranslationMatch("partido", "adj", "game").has_value());
}

TEST_CASE("Editing a card sets its gender and marks it as the user's", "[repository]")
{
    Fixture Test;


    FTCard Wrong = MakeCard("habitacion", "room");
    Wrong.Gender = GenderMasculine;

    const std::optional<std::string> CardId = Test.Repository->AddCard(Wrong);
    REQUIRE(CardId.has_value());

    // Moved up the ladder first, so the edit below has progress it could destroy.
    FTCard Studied = **Test.Repository->LoadCard(*CardId);
    Studied.LeitnerStage = FirstLadderStage + 1;
    Studied.DueDate = FTDayNumber{40};
    REQUIRE(Test.Repository->UpdateCardSchedule(Studied));

    REQUIRE(Test.Repository->UpdateCardContent(*CardId, "habitación", "room", GenderFeminine, "", ""));

    const std::optional<FTCardHandle> Corrected = Test.Repository->LoadCard(*CardId);
    REQUIRE(Corrected.has_value());

    CHECK((*Corrected)->TargetWord == "habitación");
    CHECK((*Corrected)->Gender == GenderFeminine);

    // Marked as the user's, so anything that later fills gender in from a dictionary leaves it be.
    CHECK(IsUserEdited(**Corrected, EFTCardField::Gender));

    // Correcting an article is not unlearning the word.
    CHECK((*Corrected)->LeitnerStage == FirstLadderStage + 1);
    CHECK((*Corrected)->DueDate == FTDayNumber{40});
}

TEST_CASE("A card can be edited to have no gender at all", "[repository]")
{
    Fixture Test;


    FTCard Gendered = MakeCard("manger", "to eat");
    Gendered.Gender = GenderMasculine;

    const std::optional<std::string> CardId = Test.Repository->AddCard(Gendered);
    REQUIRE(CardId.has_value());

    REQUIRE(Test.Repository->UpdateCardContent(*CardId, "manger", "to eat", std::nullopt, "", ""));

    const std::optional<FTCardHandle> Corrected = Test.Repository->LoadCard(*CardId);
    REQUIRE(Corrected.has_value());

    // A verb given a gender by mistake has to be able to lose it, and saying so is still the
    // user's own choice rather than an absence of one.
    CHECK_FALSE((*Corrected)->Gender.has_value());
    CHECK(IsUserEdited(**Corrected, EFTCardField::Gender));
}

TEST_CASE("Editing a card moves it to the chosen meaning", "[repository]")
{
    Fixture Test;


    FTCard Card = MakeCard("jeune", "young");
    Card.MeaningGist = "âge";

    const std::optional<std::string> CardId = Test.Repository->AddCard(Card);
    REQUIRE(CardId.has_value());

    REQUIRE(Test.Repository->UpdateCardContent(*CardId, "jeune", "little", std::nullopt, "adj", "cadet"));

    const std::optional<FTCardHandle> Moved = Test.Repository->LoadCard(*CardId);
    REQUIRE(Moved.has_value());
    CHECK((*Moved)->MeaningGist == "cadet");
    CHECK((*Moved)->Translation == "little");

    REQUIRE(Test.Repository->UpdateCardContent(*CardId, "jeune", "little", std::nullopt, "adj", ""));

    const std::optional<FTCardHandle> Cleared = Test.Repository->LoadCard(*CardId);
    REQUIRE(Cleared.has_value());
    CHECK((*Cleared)->MeaningGist.empty());
}

TEST_CASE("An added card lands in the pool after the others", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog")}));

    const std::optional<std::string> Added = Test.Repository->AddCard(MakeCard("chat", "cat"));

    REQUIRE(Added.has_value());

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 2);

    CHECK(Loaded[1]->Id == *Added);
    CHECK(Loaded[1]->TargetWord == "chat");
    CHECK(Loaded[1]->LeitnerStage == PoolStage);
    CHECK_FALSE(Loaded[1]->DueDate.has_value());
}

TEST_CASE("A card with a blank word is not added", "[repository]")
{
    Fixture Test;

    CHECK_FALSE(Test.Repository->AddCard(MakeCard("", "cat")).has_value());
    CHECK_FALSE(Test.Repository->AddCard(MakeCard("   ", "cat")).has_value());

    CHECK(Test.Repository->LoadCards().empty());
}

TEST_CASE("Counting cards by stage splits the pool, each box and completed cards", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog"), MakeCard("chat", "cat"),
                                                MakeCard("maison", "house"), MakeCard("pomme", "apple")}));

    std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 4);

    FTCard InBoxTwo = *Loaded[0];
    InBoxTwo.LeitnerStage = 2;
    InBoxTwo.DueDate = 20001;
    REQUIRE(Test.Repository->UpdateCardSchedule(InBoxTwo));

    FTCard Completed = *Loaded[1];
    Completed.LeitnerStage = GetLastStage();
    Completed.CompletedDate = 20000;
    REQUIRE(Test.Repository->UpdateCardSchedule(Completed));

    int Pool = 0;
    int BoxTwo = 0;
    int Done = 0;

    for (const FTStageCount& Entry : Test.Repository->CountCardsByStage())
    {
        if (Entry.bCompleted)
        {
            Done += Entry.Count;
        }
        else if (Entry.Stage == 0)
        {
            Pool += Entry.Count;
        }
        else if (Entry.Stage == 2)
        {
            BoxTwo += Entry.Count;
        }
    }

    CHECK(Pool == 2);
    CHECK(BoxTwo == 1);
    CHECK(Done == 1);
}

TEST_CASE("Recently studied cards come back most-recently-promoted first", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog"),
                                                MakeCard("chat", "cat"),
                                                MakeCard("oiseau", "bird")}));

    std::vector<FTCardHandle> Cards = Test.Repository->LoadCards();
    REQUIRE(Cards.size() == 3);

    FTCard First = *Cards[0];
    Promote(First, 1000);
    REQUIRE(Test.Repository->UpdateCardSchedule(First));

    FTCard Second = *Cards[1];
    Promote(Second, 3000);
    REQUIRE(Test.Repository->UpdateCardSchedule(Second));

    FTCard Third = *Cards[2];
    Promote(Third, 2000);
    REQUIRE(Test.Repository->UpdateCardSchedule(Third));

    const std::vector<FTCard> Recent = Test.Repository->LoadRecentlyStudiedCards(10);

    REQUIRE(Recent.size() == 3);
    CHECK(Recent[0].TargetWord == "chat");
    CHECK(Recent[1].TargetWord == "oiseau");
    CHECK(Recent[2].TargetWord == "chien");
}

TEST_CASE("Recently studied cards include a word promoted without being completed",
          "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog"),
                                                MakeCard("chat", "cat")}));

    std::vector<FTCardHandle> Cards = Test.Repository->LoadCards();
    REQUIRE(Cards.size() == 2);

    FTCard StillInPool = *Cards[0];
    REQUIRE(Test.Repository->UpdateCardSchedule(StillInPool));

    FTCard Promoted = *Cards[1];
    Promote(Promoted, 1000);
    REQUIRE(Test.Repository->UpdateCardSchedule(Promoted));

    const std::vector<FTCard> Recent = Test.Repository->LoadRecentlyStudiedCards(10);

    REQUIRE(Recent.size() == 1);
    CHECK(Recent.front().TargetWord == "chat");
}

TEST_CASE("Recently studied cards include a fully completed word", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog")}));

    FTCard Completed = *Test.Repository->LoadCards().front();
    MarkCompleted(Completed, 1000);
    REQUIRE(Test.Repository->UpdateCardSchedule(Completed));

    const std::vector<FTCard> Recent = Test.Repository->LoadRecentlyStudiedCards(10);

    REQUIRE(Recent.size() == 1);
    CHECK(Recent.front().TargetWord == "chien");
    CHECK(Recent.front().CompletedDate.has_value());
}

TEST_CASE("Recently studied cards leaves out a word sent back to the pool", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog")}));

    FTCard Returned = *Test.Repository->LoadCards().front();
    Promote(Returned, 1000);
    ReturnToPool(Returned, 2000);
    REQUIRE(Test.Repository->UpdateCardSchedule(Returned));

    CHECK(Test.Repository->LoadRecentlyStudiedCards(10).empty());
}

TEST_CASE("Recently studied cards respect the limit", "[repository]")
{
    Fixture Test;


    std::vector<FTCard> Words;
    for (int Index = 0; Index < 5; ++Index)
    {
        Words.push_back(MakeCard("word" + std::to_string(Index), "meaning"));
    }
    REQUIRE(AddCards(*Test.Repository, Words));

    std::vector<FTCardHandle> Cards = Test.Repository->LoadCards();
    REQUIRE(Cards.size() == 5);

    for (size_t Index = 0; Index < Cards.size(); ++Index)
    {
        FTCard Card = *Cards[Index];
        Promote(Card, static_cast<FTDayNumber>(1000 + Index));
        REQUIRE(Test.Repository->UpdateCardSchedule(Card));
    }

    CHECK(Test.Repository->LoadRecentlyStudiedCards(2).size() == 2);
    CHECK(Test.Repository->LoadRecentlyStudiedCards(0).empty());
    CHECK(Test.Repository->LoadRecentlyStudiedCards(-1).empty());
}

TEST_CASE("A completed word is reported as completed", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("chien", "dog")}));

    CHECK_FALSE(Test.Repository->IsWordCompleted("chien", ""));

    FTCard Card = *Test.Repository->LoadCards().front();
    Card.LeitnerStage = GetLastStage();
    Card.CompletedDate = 1000;
    REQUIRE(Test.Repository->UpdateCardSchedule(Card));

    CHECK(Test.Repository->IsWordCompleted("chien", ""));
    CHECK(Test.Repository->IsWordCompleted("CHIEN", ""));
    CHECK_FALSE(Test.Repository->IsWordCompleted("chat", ""));
}

TEST_CASE("Counting due and pool cards agrees with loading them", "[repository]")
{
    Fixture Test;


    std::vector<FTCard> Cards;
    Cards.push_back(MakeCard("chien", "dog"));
    Cards.push_back(MakeCard("chat", "cat"));
    Cards.push_back(MakeCard("maison", "house"));

    REQUIRE(AddCards(*Test.Repository, Cards));

    const FTDayNumber Today = 20000;

    // Everything starts in the pool, so nothing is due yet.
    CHECK(Test.Repository->CountUnlearnedCards() == 3);
    CHECK(Test.Repository->CountDueCards(Today) == 0);

    // Put one card on the ladder, due today.
    std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 3);

    FTCard Promoted = *Loaded[0];
    Promote(Promoted, Today - 1);
    Promoted.DueDate = Today;
    REQUIRE(Test.Repository->UpdateCardSchedule(Promoted));

    CHECK(Test.Repository->CountUnlearnedCards() == 2);
    CHECK(Test.Repository->CountDueCards(Today) == 1);
    CHECK(Test.Repository->CountCards() == 3);

    // The counts have to match what the load queries actually return, since the screen promises
    // one number and the session then works the other.
    CHECK(Test.Repository->CountUnlearnedCards()
          == static_cast<int>(Test.Repository->LoadUnlearnedCards().size()));
    CHECK(Test.Repository->CountDueCards(Today)
          == static_cast<int>(Test.Repository->LoadDueCards(Today).size()));
}

TEST_CASE("Recent searches come back newest first", "[repository]")
{
    Fixture Test;

    Test.FakeNow = 1000;
    CHECK(Test.Repository->RecordSearch("marche", "verb", 0));

    Test.FakeNow = 2000;
    CHECK(Test.Repository->RecordSearch("chien", "noun", 0));

    Test.FakeNow = 3000;
    CHECK(Test.Repository->RecordSearch("maison", "noun", 0));

    const std::vector<FTSearchHistoryEntry> Recent = Test.Repository->LoadRecentSearches(10);

    REQUIRE(Recent.size() == 3);
    CHECK(Recent[0].Term == "maison");
    CHECK(Recent[1].Term == "chien");
    CHECK(Recent[2].Term == "marche");
}

TEST_CASE("Searching the same word again moves it up rather than repeating it", "[repository]")
{
    Fixture Test;

    Test.FakeNow = 1000;
    Test.Repository->RecordSearch("marche", "verb", 0);

    Test.FakeNow = 2000;
    Test.Repository->RecordSearch("chien", "noun", 0);

    Test.FakeNow = 3000;
    Test.Repository->RecordSearch("marche", "verb", 0);

    const std::vector<FTSearchHistoryEntry> Recent = Test.Repository->LoadRecentSearches(10);

    REQUIRE(Recent.size() == 2);
    CHECK(Recent[0].Term == "marche");
    CHECK(Recent[1].Term == "chien");
}

TEST_CASE("Recent searches respect the limit", "[repository]")
{
    Fixture Test;

    for (int Index = 0; Index < 10; ++Index)
    {
        Test.FakeNow = 1000 + Index;
        Test.Repository->RecordSearch("word" + std::to_string(Index), "noun", 0);
    }

    const std::vector<FTSearchHistoryEntry> Recent = Test.Repository->LoadRecentSearches(4);

    REQUIRE(Recent.size() == 4);
    CHECK(Recent.front().Term == "word9");

    CHECK(Test.Repository->LoadRecentSearches(0).empty());
    CHECK(Test.Repository->LoadRecentSearches(-1).empty());
}

TEST_CASE("A blank search is not recorded", "[repository]")
{
    Fixture Test;

    CHECK_FALSE(Test.Repository->RecordSearch("", "", 0));
    CHECK_FALSE(Test.Repository->RecordSearch("   ", "", 0));

    CHECK(Test.Repository->LoadRecentSearches(10).empty());
}

TEST_CASE("Wiping the database clears search history too", "[repository]")
{
    Fixture Test;

    Test.Repository->RecordSearch("marche", "verb", 0);
    REQUIRE(Test.Repository->LoadRecentSearches(10).size() == 1);

    REQUIRE(Test.Repository->DeleteEverything());

    CHECK(Test.Repository->LoadRecentSearches(10).empty());
}

TEST_CASE("Promoting cards moves every one from the pool to box 1", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, MakeThreeCards()));

    CHECK(Test.Repository->PromoteCards(CardIdsOf(Test.Repository->LoadCards())));

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();

    REQUIRE(Loaded.size() == 3);

    for (const FTCardHandle& Word : Loaded)
    {
        CHECK(Word->LeitnerStage == 1);
    }
}

TEST_CASE("Deleting a card is soft, so the row survives for a later sync", "[repository]")
{
    Fixture Test;

    const std::optional<std::string> CardId = Test.Repository->AddCard(MakeCard("chien", "dog"));
    REQUIRE(CardId.has_value());

    Test.FakeNow = 4000;
    REQUIRE(Test.Repository->DeleteCards({*CardId}));

    FTStatement Query = Test.Target->Prepare("SELECT Deleted, LastUpdatedDate FROM Cards WHERE Id = ?1");
    REQUIRE(Query.IsValid());
    Query.BindText(1, *CardId);
    REQUIRE(Query.Step());

    CHECK(Query.GetInt(0) == 1);
    CHECK(Query.GetInt64(1) == 4000);
    CHECK_FALSE(Test.Repository->LoadCard(*CardId).has_value());
    CHECK(Test.Repository->LoadCards().empty());
}

TEST_CASE("Words are stored and loaded back in order", "[repository]")
{
    Fixture Test;


    REQUIRE(AddCards(*Test.Repository, MakeThreeCards()));

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 3);

    CHECK(Loaded[0]->TargetWord == "le chien");
    CHECK(Loaded[1]->TargetWord == "la maison");
    CHECK(Loaded[2]->TargetWord == "le café");

    CHECK(Loaded[0]->Translation == "dog");
    CHECK(Loaded[2]->Translation == "coffee");
}

TEST_CASE("Every card field round trips", "[repository]")
{
    Fixture Test;


    Test.FakeNow = 7000;

    FTCard Full;
    Full.TargetWord = "l'école";
    Full.Translation = "school";
    Full.Definition = "établissement où l'on enseigne";
    Full.Gender = GenderFeminine;
    Full.LeitnerStage = 3;
    Full.DueDate = 20500;
    Full.BoxChangedDate = 20495;
    SetUserEdited(Full, EFTCardField::Definition, true);

    REQUIRE(AddCards(*Test.Repository, {Full}));

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 1);

    const FTCard& Round = *Loaded[0];
    CHECK(Round.TargetWord == "l'école");
    CHECK(Round.Translation == "school");
    CHECK(Round.Definition == "établissement où l'on enseigne");
    CHECK(Round.Gender == std::optional<int>(GenderFeminine));
    CHECK(Round.LeitnerStage == 3);
    CHECK(Round.DueDate == 20500);
    CHECK(Round.BoxChangedDate == 20495);
    CHECK_FALSE(Round.CompletedDate.has_value());
    CHECK(IsUserEdited(Round, EFTCardField::Definition));
    CHECK_FALSE(IsUserEdited(Round, EFTCardField::Translation));
    CHECK(Round.CreatedDate == 7000);
    CHECK(Round.LastUpdatedDate == 7000);
    CHECK(IsValidUuid(Round.Id));
}

TEST_CASE("A card's gender round trips through the repository", "[repository]")
{
    Fixture Test;


    FTCard Masculine = MakeCard("chien", "dog");
    Masculine.Gender = GenderMasculine;

    FTCard Feminine = MakeCard("maison", "house");
    Feminine.Gender = GenderFeminine;

    FTCard NoGender = MakeCard("manger", "to eat");

    REQUIRE(AddCards(*Test.Repository, {Masculine, Feminine, NoGender}));

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 3);

    CHECK(Loaded[0]->Gender == std::optional<int>(GenderMasculine));
    CHECK(Loaded[1]->Gender == std::optional<int>(GenderFeminine));
    CHECK_FALSE(Loaded[2]->Gender.has_value());
}

TEST_CASE("A pool card stores no dates at all", "[repository]")
{
    Fixture Test;


    // The default card, which is what the editor produces. NULL rather than a zero day, so that
    // "no due date" cannot be confused with "due on 1970-01-01".
    REQUIRE(AddCards(*Test.Repository, {MakeCard("le chien", "dog")}));

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 1);

    const FTCard& Round = *Loaded[0];
    CHECK(Round.LeitnerStage == PoolStage);
    CHECK(IsInPool(Round));
    CHECK_FALSE(Round.DueDate.has_value());
    CHECK_FALSE(Round.BoxChangedDate.has_value());
    CHECK_FALSE(Round.CompletedDate.has_value());
}

TEST_CASE("Saving assigns ids to new cards", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, MakeThreeCards()));

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();

    for (const FTCardHandle& Stored : Loaded)
    {
        CHECK(IsValidUuid(Stored->Id));
    }
}

TEST_CASE("A deleted card goes away and the others stay", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, MakeThreeCards()));

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 3);

    REQUIRE(Test.Repository->DeleteCards({Loaded[1]->Id}));

    const std::vector<FTCardHandle> After = Test.Repository->LoadCards();
    REQUIRE(After.size() == 2);

    CHECK(After[0]->TargetWord == "le chien");
    CHECK(After[1]->TargetWord == "le café");
}

TEST_CASE("Cards survive reopening the same file", "[repository]")
{
    const std::string Path = std::string(SAVED_TEST_DATA_DIRECTORY) + "/IkonikaRepositoryTest.db";
    std::remove(Path.c_str());

    {
        std::optional<FTDatabase> Opened = FTDatabase::Open(Path);
        REQUIRE(Opened.has_value());
        REQUIRE(MigrateSchema(*Opened));

        FTCardRepository Repository(*Opened);
        Repository.SetActiveLanguages("fr", "en");

        REQUIRE(AddCards(Repository, MakeThreeCards()));
    }

    {
        std::optional<FTDatabase> Reopened = FTDatabase::Open(Path);
        REQUIRE(Reopened.has_value());
        REQUIRE(MigrateSchema(*Reopened));

        FTCardRepository Repository(*Reopened);
        Repository.SetActiveLanguages("fr", "en");

        const std::vector<FTCardHandle> Loaded = Repository.LoadCards();
        REQUIRE(Loaded.size() == 3);
        CHECK(Loaded[2]->TargetWord == "le café");
    }

    std::remove(Path.c_str());
}

TEST_CASE("Deleting everything leaves nothing, tombstones included", "[repository]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, MakeThreeCards()));

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Test.Repository->DeleteCards({Loaded.front()->Id}));
    REQUIRE(Test.Repository->PromoteCards({Loaded.back()->Id}));

    REQUIRE(Test.Repository->DeleteEverything());

    CHECK(Test.Repository->LoadCards().empty());

    for (const char* Table : {"Cards", "Journal"})
    {
        FTStatement Count = Test.Target->Prepare(std::string("SELECT COUNT(*) FROM ") + Table);
        REQUIRE(Count.IsValid());
        REQUIRE(Count.Step());

        CHECK(Count.GetInt(0) == 0);
    }
}

TEST_CASE("Deleting everything on an empty database succeeds", "[repository]")
{
    Fixture Test;

    CHECK(Test.Repository->DeleteEverything());
}

TEST_CASE("A card can be added again after deleting everything", "[repository]")
{
    Fixture Test;

    REQUIRE(Test.Repository->AddCard(MakeCard("chien", "dog")).has_value());
    REQUIRE(Test.Repository->DeleteEverything());

    REQUIRE(Test.Repository->AddCard(MakeCard("chat", "cat")).has_value());
    CHECK(Test.Repository->LoadCards().size() == 1);
}

TEST_CASE("Due cards come back soonest first", "[repository][schedule]")
{
    Fixture Test;


    constexpr FTDayNumber Today = 20000;

    FTCard Overdue = MakeCard("le café", "coffee");
    Overdue.LeitnerStage = 2;
    Overdue.DueDate = Today - 3;

    FTCard DueToday = MakeCard("le chien", "dog");
    DueToday.LeitnerStage = 1;
    DueToday.DueDate = Today;

    FTCard Later = MakeCard("la maison", "house");
    Later.LeitnerStage = 3;
    Later.DueDate = Today + 5;

    FTCard Pooled = MakeCard("la clé", "key");

    REQUIRE(AddCards(*Test.Repository, {Overdue, Later, Pooled}));
    REQUIRE(AddCards(*Test.Repository, {DueToday}));

    const std::vector<FTCardHandle> Due = Test.Repository->LoadDueCards(Today);

    REQUIRE(Due.size() == 2);
    CHECK(Due[0]->TargetWord == "le café");
    CHECK(Due[1]->TargetWord == "le chien");
}

TEST_CASE("A completed card is never due", "[repository][schedule]")
{
    Fixture Test;


    constexpr FTDayNumber Today = 20000;

    FTCard Done = MakeCard("le chien", "dog");
    Done.LeitnerStage = 5;
    Done.CompletedDate = Today - 1;

    REQUIRE(AddCards(*Test.Repository, {Done}));

    CHECK(Test.Repository->LoadDueCards(Today).empty());
    CHECK(Test.Repository->LoadUnlearnedCards().empty());
}

TEST_CASE("Pool cards come back oldest first", "[repository][schedule]")
{
    Fixture Test;

    Test.FakeNow = 9000;
    REQUIRE(Test.Repository->AddCard(MakeCard("la maison", "house")).has_value());

    Test.FakeNow = 1000;
    REQUIRE(Test.Repository->AddCard(MakeCard("le chien", "dog")).has_value());

    const std::vector<FTCardHandle> Pool = Test.Repository->LoadUnlearnedCards();

    REQUIRE(Pool.size() == 2);
    CHECK(Pool[0]->TargetWord == "le chien");
    CHECK(Pool[1]->TargetWord == "la maison");
}

TEST_CASE("A deleted card is not offered by either query", "[repository][schedule]")
{
    Fixture Test;

    const std::optional<std::string> CardId = Test.Repository->AddCard(MakeCard("le chien", "dog"));
    REQUIRE(CardId.has_value());
    REQUIRE(Test.Repository->LoadUnlearnedCards().size() == 1);

    REQUIRE(Test.Repository->DeleteCards({*CardId}));

    CHECK(Test.Repository->LoadUnlearnedCards().empty());
    CHECK(Test.Repository->LoadDueCards(20000).empty());
}

TEST_CASE("Writing a schedule leaves the card's text and last-updated time alone", "[repository][schedule]")
{
    Fixture Test;


    Test.FakeNow = 1000;
    REQUIRE(AddCards(*Test.Repository, {MakeCard("le chien", "dog")}));

    std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 1);

    FTCard Reviewed = *Loaded[0];
    Reviewed.LeitnerStage = 2;
    Reviewed.DueDate = 20002;
    Reviewed.BoxChangedDate = 20000;

    // Deliberately wrong, to prove the text is not written.
    Reviewed.TargetWord = "should not be saved";
    Reviewed.Translation = "nor this";

    Test.FakeNow = 7000;
    REQUIRE(Test.Repository->UpdateCardSchedule(Reviewed));

    Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 1);

    const FTCard& Stored = *Loaded[0];
    CHECK(Stored.TargetWord == "le chien");
    CHECK(Stored.Translation == "dog");
    CHECK(Stored.LeitnerStage == 2);
    CHECK(Stored.DueDate == 20002);
    CHECK(Stored.BoxChangedDate == 20000);
    CHECK(Stored.LastUpdatedDate == 1000);
}

TEST_CASE("Writing a schedule for an unknown or deleted card fails", "[repository][schedule]")
{
    Fixture Test;

    FTCard Unknown = MakeCard("le chien", "dog");
    Unknown.Id = MakeUuid();

    CHECK_FALSE(Test.Repository->UpdateCardSchedule(Unknown));

    Unknown.Id.clear();
    CHECK_FALSE(Test.Repository->UpdateCardSchedule(Unknown));
}

TEST_CASE("A schedule written by the repository round trips through the ladder", "[repository][schedule]")
{
    Fixture Test;


    REQUIRE(AddCards(*Test.Repository, {MakeCard("le chien", "dog")}));

    constexpr FTDayNumber Today = 20000;

    std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 1);

    FTCard Card = *Loaded[0];
    REQUIRE(IsInPool(Card));

    // Learned today, so due tomorrow and not before.
    Promote(Card, Today);
    REQUIRE(Test.Repository->UpdateCardSchedule(Card));

    CHECK(Test.Repository->LoadDueCards(Today).empty());
    CHECK(Test.Repository->LoadDueCards(Today + 1).size() == 1);
    CHECK(Test.Repository->LoadUnlearnedCards().empty());

    // Missed for four days, which sends a stage-1 card back to the pool.
    Card = *Test.Repository->LoadDueCards(Today + 4).at(0);
    REQUIRE(ApplyExpiry(Card, Today + 4));
    REQUIRE(Test.Repository->UpdateCardSchedule(Card));

    CHECK(Test.Repository->LoadDueCards(Today + 4).empty());
    CHECK(Test.Repository->LoadUnlearnedCards().size() == 1);
}

TEST_CASE("The system clock is used when none is given", "[repository]")
{
    std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
    REQUIRE(Opened.has_value());
    REQUIRE(MigrateSchema(*Opened));

    FTCardRepository Repository(*Opened);

    REQUIRE(Repository.AddCard(MakeCard("chien", "dog")).has_value());

    const std::vector<FTCardHandle> Loaded = Repository.LoadCards();
    REQUIRE(Loaded.size() == 1);
    CHECK(Loaded[0]->LastUpdatedDate > 1700000000000LL);
}

TEST_CASE("Deleting cards removes each of them", "[repository][pool]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("le pain", "bread"), MakeCard("le beurre", "butter")}));

    const std::vector<std::string> CardIds = CardIdsOf(Test.Repository->LoadCards());
    REQUIRE(CardIds.size() == 2);

    REQUIRE(Test.Repository->DeleteCards(CardIds));

    CHECK_FALSE(Test.Repository->LoadCard(CardIds.front()).has_value());
    CHECK_FALSE(Test.Repository->LoadCard(CardIds.back()).has_value());
    CHECK(Test.Repository->CountCards() == 0);
}

TEST_CASE("Deleting no cards succeeds", "[repository][pool]")
{
    Fixture Test;

    REQUIRE(Test.Repository->DeleteCards({}));
}

TEST_CASE("A card promoted out of box 0 leaves the pool and stays a card", "[repository][pool]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("le chien", "dog"), MakeCard("la maison", "house")}));

    REQUIRE(Test.Repository->CountUnlearnedCards() == 2);

    std::vector<FTCardHandle> Pool = Test.Repository->LoadUnlearnedCards();
    REQUIRE(Pool.size() == 2);

    FTCard Word = *Pool.front();
    Word.LeitnerStage = 1;
    Word.DueDate = 3;
    REQUIRE(Test.Repository->UpdateCardSchedule(Word));

    CHECK(Test.Repository->CountUnlearnedCards() == 1);
    CHECK(Test.Repository->LoadCards().size() == 2);
}

TEST_CASE("Pool cards added at the same time come back in the order they were added", "[repository][pool]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("zebre", "zebra"), MakeCard("abeille", "bee"),
                                        MakeCard("maison", "house")}));

    const std::vector<FTCardHandle> Pool = Test.Repository->LoadUnlearnedCards();
    REQUIRE(Pool.size() == 3);

    CHECK(Pool[0]->TargetWord == "zebre");
    CHECK(Pool[1]->TargetWord == "abeille");
    CHECK(Pool[2]->TargetWord == "maison");
}

TEST_CASE("A card sent back to the pool by hand loses its box and due date",
         "[repository][pool]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("le chien", "dog")}));

    std::vector<FTCardHandle> Held = Test.Repository->LoadUnlearnedCards();
    REQUIRE(Held.size() == 1);

    FTCard Word = *Held.front();
    Word.LeitnerStage = 3;
    Word.DueDate = 42;
    Word.BoxChangedDate = 10;
    REQUIRE(Test.Repository->UpdateCardSchedule(Word));

    REQUIRE(Test.Repository->ReturnCardToPool(Word.Id));

    const std::optional<FTCardHandle> Reloaded = Test.Repository->LoadCard(Word.Id);
    REQUIRE(Reloaded.has_value());

    CHECK(IsInPool(**Reloaded));
    CHECK_FALSE((*Reloaded)->DueDate.has_value());
    CHECK_FALSE((*Reloaded)->CompletedDate.has_value());

    CHECK(Test.Repository->LoadCards().size() == 1);
}

TEST_CASE("Returning a Completed card to the pool clears its completion", "[repository][pool]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("le chien", "dog")}));

    std::vector<FTCardHandle> Held = Test.Repository->LoadUnlearnedCards();
    REQUIRE(Held.size() == 1);

    FTCard Word = *Held.front();
    Word.LeitnerStage = GetLastStage();
    Word.CompletedDate = 5;
    REQUIRE(Test.Repository->UpdateCardSchedule(Word));

    REQUIRE(Test.Repository->ReturnCardToPool(Word.Id));

    const std::optional<FTCardHandle> Reloaded = Test.Repository->LoadCard(Word.Id);
    REQUIRE(Reloaded.has_value());

    CHECK(IsInPool(**Reloaded));
    CHECK_FALSE(IsCompleted(**Reloaded));
}

TEST_CASE("Returning an unknown card to the pool fails", "[repository][pool]")
{
    Fixture Test;

    CHECK_FALSE(Test.Repository->ReturnCardToPool("not-a-real-id"));
}

TEST_CASE("A pool card marked complete by hand skips straight to the last stage",
         "[repository][pool]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("le chien", "dog")}));

    std::vector<FTCardHandle> Held = Test.Repository->LoadUnlearnedCards();
    REQUIRE(Held.size() == 1);

    REQUIRE(Test.Repository->MarkCardCompleted(Held.front()->Id));

    const std::optional<FTCardHandle> Reloaded = Test.Repository->LoadCard(Held.front()->Id);
    REQUIRE(Reloaded.has_value());

    CHECK(IsCompleted(**Reloaded));
    CHECK((*Reloaded)->LeitnerStage == GetLastStage());
    CHECK_FALSE((*Reloaded)->DueDate.has_value());
}

TEST_CASE("Marking an unknown card complete fails", "[repository][pool]")
{
    Fixture Test;

    CHECK_FALSE(Test.Repository->MarkCardCompleted("not-a-real-id"));
}

TEST_CASE("A word the user knows is remembered without becoming a card", "[repository][triage]")
{
    Fixture Test;

    CHECK_FALSE(Test.Repository->IsWordTriaged("chien", "noun", 0));

    REQUIRE(Test.Repository->MarkWordTriaged("chien", "noun", 0));

    CHECK(Test.Repository->IsWordTriaged("chien", "noun", 0));
    CHECK(Test.Repository->CountTriagedWords() == 1);

    // The point of the table: knowing five thousand words should not leave five thousand things to
    // study.
    CHECK(Test.Repository->CountCards() == 0);
}

TEST_CASE("A spelling that is two words is answered twice", "[repository][triage]")
{
    Fixture Test;

    // "moule" is a mould as a masculine noun and a mussel as a feminine one. Knowing one says
    // nothing about the other, so answering one must not silence both.
    REQUIRE(Test.Repository->MarkWordTriaged("moule", "noun", 0));

    CHECK(Test.Repository->IsWordTriaged("moule", "noun", 0));
    CHECK_FALSE(Test.Repository->IsWordTriaged("moule", "noun", 1));

    REQUIRE(Test.Repository->MarkWordTriaged("moule", "noun", 1));
    CHECK(Test.Repository->CountTriagedWords() == 2);

    // The same for a spelling split by part of speech rather than gender.
    REQUIRE(Test.Repository->MarkWordTriaged("pouvoir", "verb", std::nullopt));
    CHECK(Test.Repository->IsWordTriaged("pouvoir", "verb", std::nullopt));
    CHECK_FALSE(Test.Repository->IsWordTriaged("pouvoir", "noun", 0));
}

TEST_CASE("Known words list each reading once", "[repository][triage][decks]")
{
    Fixture Test;

    REQUIRE(Test.Repository->MarkWordTriaged("moule", "noun", 0));
    REQUIRE(Test.Repository->MarkWordTriaged("moule", "noun", 1));
    REQUIRE(Test.Repository->MarkWordTriaged("pouvoir", "verb", std::nullopt));

    const std::vector<FTKnownWord> Known = Test.Repository->ListKnownWords();

    REQUIRE(Known.size() == 2);
    CHECK(std::any_of(Known.begin(), Known.end(), [](const FTKnownWord& Word)
    {
        return Word.TargetWord == "moule" && Word.PartOfSpeech == "noun";
    }));
    CHECK(std::any_of(Known.begin(), Known.end(), [](const FTKnownWord& Word)
    {
        return Word.TargetWord == "pouvoir" && Word.PartOfSpeech == "verb";
    }));
}

TEST_CASE("A word whose card was deleted after triage is not known", "[repository][triage][decks]")
{
    Fixture Test;

    FTCard Card = MakeCard("chien", "dog");
    Card.PartOfSpeech = "noun";

    const std::optional<std::string> CardId = Test.Repository->AddCard(Card);
    REQUIRE(CardId.has_value());
    REQUIRE(Test.Repository->MarkWordTriaged("chien", "noun", 0));

    Test.FakeNow = 2000;
    REQUIRE(Test.Repository->DeleteCards({*CardId}));

    CHECK(Test.Repository->ListKnownWords().empty());

    Test.FakeNow = 3000;
    REQUIRE(Test.Repository->MarkWordTriaged("chien", "noun", 0));

    REQUIRE(Test.Repository->ListKnownWords().size() == 1);
}

TEST_CASE("Triage matches a word whatever its case", "[repository][triage]")
{
    Fixture Test;

    REQUIRE(Test.Repository->MarkWordTriaged("Chien", "noun", 0));

    CHECK(Test.Repository->IsWordTriaged("chien", "noun", 0));
    CHECK(Test.Repository->IsWordTriaged("CHIEN", "noun", 0));

    // Answered twice is not an error, and must not become two rows.
    REQUIRE(Test.Repository->MarkWordTriaged("chien", "noun", 0));
    CHECK(Test.Repository->CountTriagedWords() == 1);
}

TEST_CASE("Triage skips readings already answered or already cards", "[repository][triage]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("le pain", "bread")}));

    REQUIRE(Test.Repository->MarkWordTriaged("chien", "noun", 0));

    // Answered, so not offered again.
    CHECK_FALSE(Test.Repository->IsWordUnseen("chien", "noun", 0, "dog"));

    // Already a card. Matched on the reading too, not just spelling and translation: this card
    // was made by hand, so it carries no part of speech or gender, the same as this check.
    CHECK_FALSE(Test.Repository->IsWordUnseen("le pain", "", std::nullopt, "bread"));

    // Neither, so both are offerable.
    CHECK(Test.Repository->IsWordUnseen("maison", "noun", 1, "house"));
    CHECK(Test.Repository->IsWordUnseen("chien", "noun", 1, "bitch"));
}

TEST_CASE("Triage leaves out a headword and part of speech the user already has",
          "[repository][triage]")
{
    Fixture Test;


    FTCard Owned = MakeCard("payer", "pay");
    Owned.PartOfSpeech = "verb";

    FTCard Ungendered = MakeCard("bonjour", "hello");
    Ungendered.PartOfSpeech = "noun";

    REQUIRE(AddCards(*Test.Repository, {Owned, Ungendered}));
    REQUIRE(Test.Repository->MarkWordTriaged("moule", "noun", 0));

    CHECK(Test.Repository->IsWordOwnedOrTriaged("payer", "verb"));
    CHECK(Test.Repository->IsWordOwnedOrTriaged("Payer", "verb"));
    CHECK(Test.Repository->IsWordOwnedOrTriaged("bonjour", "noun"));
    CHECK(Test.Repository->IsWordOwnedOrTriaged("moule", "noun"));

    CHECK_FALSE(Test.Repository->IsWordOwnedOrTriaged("payer", "noun"));
    CHECK_FALSE(Test.Repository->IsWordOwnedOrTriaged("manger", "verb"));
}

TEST_CASE("Known headwords are the studied cards and the words answered as known",
          "[repository][triage]")
{
    Fixture Test;


    FTCard Studied = MakeCard("chien", "dog");
    Studied.PartOfSpeech = "noun";
    Studied.Gender = 0;

    FTCard Waiting = MakeCard("maison", "house");
    Waiting.PartOfSpeech = "noun";
    Waiting.Gender = 1;

    REQUIRE(AddCards(*Test.Repository, {Studied, Waiting}));

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();

    std::vector<std::string> StudiedIds;

    for (const FTCardHandle& Card : Loaded)
    {
        if (Card->TargetWord == "chien")
        {
            StudiedIds.push_back(Card->Id);
        }
    }

    REQUIRE(StudiedIds.size() == 1);
    REQUIRE(Test.Repository->PromoteCards(StudiedIds));

    REQUIRE(Test.Repository->MarkWordTriaged("maison", "noun", 1));
    REQUIRE(Test.Repository->MarkWordTriaged("je", "pron", std::nullopt));

    std::vector<std::string> Known = Test.Repository->ListKnownHeadwords();
    std::sort(Known.begin(), Known.end());

    CHECK(Known == std::vector<std::string>{"chien", "je"});
}

TEST_CASE("Wiping everything clears what the user said they knew", "[repository][triage]")
{
    Fixture Test;

    REQUIRE(Test.Repository->MarkWordTriaged("chien", "noun", 0));
    REQUIRE(Test.Repository->CountTriagedWords() == 1);

    REQUIRE(Test.Repository->DeleteEverything());

    // Otherwise a fresh start would never offer those words again.
    CHECK(Test.Repository->CountTriagedWords() == 0);
}

// ------------------------------------------------------------------- language scoping

TEST_CASE("Cards belong to the language pair they were added under", "[repository][languages]")
{
    Fixture Test;

    Test.Repository->SetActiveLanguages("fr", "en");
    REQUIRE(Test.Repository->AddCard(MakeCard("chien", "dog")).has_value());

    Test.Repository->SetActiveLanguages("es", "en");
    REQUIRE(Test.Repository->AddCard(MakeCard("perro", "dog")).has_value());

    REQUIRE(Test.Repository->LoadCards().size() == 1);
    CHECK(Test.Repository->LoadCards()[0]->TargetWord == "perro");

    Test.Repository->SetActiveLanguages("fr", "en");
    REQUIRE(Test.Repository->LoadCards().size() == 1);
    CHECK(Test.Repository->LoadCards()[0]->TargetWord == "chien");

    FTStatement Stored = Test.Target->Prepare(
        "SELECT TargetLanguage, NativeLanguage FROM Cards WHERE TargetWord = 'perro'");
    REQUIRE(Stored.Step());
    CHECK(Stored.GetText(0) == "es");
    CHECK(Stored.GetText(1) == "en");
}

TEST_CASE("The native language is part of what scopes a card", "[repository][languages]")
{
    Fixture Test;

    Test.Repository->SetActiveLanguages("fr", "en");
    REQUIRE(Test.Repository->AddCard(MakeCard("chien", "dog")).has_value());

    Test.Repository->SetActiveLanguages("fr", "de");
    CHECK(Test.Repository->LoadCards().empty());
    CHECK(Test.Repository->CountCards() == 0);
}

TEST_CASE("Cards of another language pair are invisible to a session", "[repository][languages]")
{
    Fixture Test;

    Test.Repository->SetActiveLanguages("fr", "en");
    REQUIRE(AddCards(*Test.Repository, {MakeCard("le chien", "dog")}));
    REQUIRE(Test.Repository->PromoteCards(CardIdsOf(Test.Repository->LoadCards())));

    Test.Repository->SetActiveLanguages("es", "en");

    // The Spanish learner has nothing due, nothing waiting, and has not already learned "chien".
    CHECK(Test.Repository->CountDueCards(999999) == 0);
    CHECK(Test.Repository->LoadDueCards(999999).empty());
    CHECK(Test.Repository->CountUnlearnedCards() == 0);
    CHECK_FALSE(Test.Repository->FindWordStage("le chien", "").has_value());

    Test.Repository->SetActiveLanguages("fr", "en");
    CHECK(Test.Repository->CountDueCards(999999) == 1);
    CHECK(Test.Repository->FindWordStage("le chien", "").has_value());
}

TEST_CASE("A word answered in one language is still offered in another",
          "[repository][languages][triage]")
{
    Fixture Test;

    Test.Repository->SetActiveLanguages("fr", "en");
    REQUIRE(Test.Repository->MarkWordTriaged("pan", "noun", 0));
    CHECK(Test.Repository->IsWordTriaged("pan", "noun", 0));

    // Spellings collide across languages. French "pan" says nothing about the Spanish word.
    Test.Repository->SetActiveLanguages("es", "en");
    CHECK_FALSE(Test.Repository->IsWordTriaged("pan", "noun", 0));
    CHECK(Test.Repository->CountTriagedWords() == 0);

    // Reading it in another native language does not make it a new question, though.
    Test.Repository->SetActiveLanguages("fr", "de");
    CHECK(Test.Repository->IsWordTriaged("pan", "noun", 0));
}

TEST_CASE("Search history is per language being learned", "[repository][languages]")
{
    Fixture Test;

    Test.Repository->SetActiveLanguages("fr", "en");
    REQUIRE(Test.Repository->RecordSearch("chien", "noun", 0));

    Test.Repository->SetActiveLanguages("es", "en");
    CHECK(Test.Repository->LoadRecentSearches(10).empty());

    REQUIRE(Test.Repository->RecordSearch("chien", "noun", 0));
    CHECK(Test.Repository->LoadRecentSearches(10).size() == 1);

    Test.Repository->SetActiveLanguages("fr", "en");
    CHECK(Test.Repository->LoadRecentSearches(10).size() == 1);
}

// ------------------------------------------------------------------- training sessions

TEST_CASE("A session promotes only the words it taught", "[repository][training]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, MakeThreeCards()));

    const std::vector<std::string> CardIds = CardIdsOf(Test.Repository->LoadCards());
    REQUIRE(CardIds.size() == 3);

    REQUIRE(Test.Repository->PromoteCards({CardIds[0], CardIds[1]}));

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();

    int Promoted = 0;
    int StillWaiting = 0;

    for (const FTCardHandle& Card : Loaded)
    {
        if (Card->LeitnerStage > 0)
        {
            ++Promoted;

            continue;
        }

        ++StillWaiting;
    }

    CHECK(Promoted == 2);
    CHECK(StillWaiting == 1);
}

// ------------------------------------------------------------------- daily stats

TEST_CASE("A day's progress adds up rather than replacing", "[repository][stats]")
{
    Fixture Test;

    Test.Repository->SetActiveLanguages("fr", "en");

    // A day can hold several sessions, and each reports only its own work.
    REQUIRE(Test.Repository->RecordDailyProgress(100, 25, 0, 15));
    REQUIRE(Test.Repository->RecordDailyProgress(100, 0, 40, 15));
    REQUIRE(Test.Repository->RecordDailyProgress(100, 10, 5, 15));

    const std::vector<FTDailyStats> Stats = Test.Repository->LoadDailyStats(100, 100);

    REQUIRE(Stats.size() == 1);
    CHECK(Stats[0].Day == 100);
    CHECK(Stats[0].LearnedCount == 35);
    CHECK(Stats[0].ReviewedCount == 45);
    CHECK(Stats[0].DueCount == 0);
    CHECK(Stats[0].GoalCount == 15);
}

TEST_CASE("A day nothing happened on is not stored", "[repository][stats]")
{
    Fixture Test;

    Test.Repository->SetActiveLanguages("fr", "en");

    // Otherwise the calendar could not tell "nothing done" from "not recorded", which is the whole
    // difference between a day you skipped and a day before any of this existed.
    CHECK_FALSE(Test.Repository->RecordDailyProgress(100, 0, 0, 15));
    CHECK(Test.Repository->LoadDailyStats(0, 1000).empty());
}

TEST_CASE("Progress is kept per language pair", "[repository][stats][languages]")
{
    Fixture Test;

    Test.Repository->SetActiveLanguages("fr", "en");
    REQUIRE(Test.Repository->RecordDailyProgress(100, 25, 0, 15));

    // A French count has no business showing while Spanish is being studied.
    Test.Repository->SetActiveLanguages("es", "en");
    CHECK(Test.Repository->LoadDailyStats(100, 100).empty());

    REQUIRE(Test.Repository->RecordDailyProgress(100, 7, 0, 15));

    const std::vector<FTDailyStats> Spanish = Test.Repository->LoadDailyStats(100, 100);
    REQUIRE(Spanish.size() == 1);
    CHECK(Spanish[0].LearnedCount == 7);

    Test.Repository->SetActiveLanguages("fr", "en");

    const std::vector<FTDailyStats> French = Test.Repository->LoadDailyStats(100, 100);
    REQUIRE(French.size() == 1);
    CHECK(French[0].LearnedCount == 25);
}

TEST_CASE("Only days inside the range come back, oldest first", "[repository][stats]")
{
    Fixture Test;

    Test.Repository->SetActiveLanguages("fr", "en");

    REQUIRE(Test.Repository->RecordDailyProgress(120, 3, 0, 15));
    REQUIRE(Test.Repository->RecordDailyProgress(100, 1, 0, 15));
    REQUIRE(Test.Repository->RecordDailyProgress(110, 2, 0, 15));

    const std::vector<FTDailyStats> Stats = Test.Repository->LoadDailyStats(100, 110);

    REQUIRE(Stats.size() == 2);
    CHECK(Stats[0].Day == 100);
    CHECK(Stats[1].Day == 110);
}

TEST_CASE("Wiping everything clears the calendar too", "[repository][stats]")
{
    Fixture Test;

    Test.Repository->SetActiveLanguages("fr", "en");
    REQUIRE(Test.Repository->RecordDailyProgress(100, 25, 0, 15));

    REQUIRE(Test.Repository->DeleteEverything());

    CHECK(Test.Repository->LoadDailyStats(0, 1000).empty());
}

TEST_CASE("Due cards are counted per day for the days ahead", "[repository][stats]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, MakeThreeCards()));

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();

    std::vector<FTCard> Editable;

    for (const FTCardHandle& Held : Loaded)
    {
        Editable.push_back(*Held);
    }

    REQUIRE(Editable.size() == 3);

    // Two on one day, one on another, so the grouping is doing something.
    Editable[0].LeitnerStage = 1;
    Editable[0].DueDate = 200;
    Editable[1].LeitnerStage = 1;
    Editable[1].DueDate = 200;
    Editable[2].LeitnerStage = 1;
    Editable[2].DueDate = 205;

    for (const FTCard& Card : Editable)
    {
        REQUIRE(Test.Repository->UpdateCardSchedule(Card));
    }

    const std::vector<FTDueCountForDay> Counts = Test.Repository->CountDueCardsByDay(200, 210);

    REQUIRE(Counts.size() == 2);
    CHECK(Counts[0].Day == 200);
    CHECK(Counts[0].Count == 2);
    CHECK(Counts[1].Day == 205);
    CHECK(Counts[1].Count == 1);
}

TEST_CASE("Nothing on the ladder means no due days ahead", "[repository][stats]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, MakeThreeCards()));

    // Every card is unlearned, so nothing is due on any day.
    CHECK(Test.Repository->CountDueCardsByDay(0, 100000).empty());
}

TEST_CASE("A word or translation past the field limit is cut to it", "[repository]")
{
    Fixture Test;


    const std::string LongWord(MaximumCardFieldLength + 30, 'a');
    const std::string LongTranslation(MaximumCardFieldLength + 5, 'b');

    const std::optional<std::string> CardId =
        Test.Repository->AddCard(MakeCard(LongWord, LongTranslation));

    REQUIRE(CardId.has_value());

    const std::optional<FTCardHandle> Stored = Test.Repository->LoadCard(*CardId);
    REQUIRE(Stored.has_value());

    CHECK(static_cast<int>((*Stored)->TargetWord.size()) == MaximumCardFieldLength);
    CHECK(static_cast<int>((*Stored)->Translation.size()) == MaximumCardFieldLength);
}

TEST_CASE("An accented word is measured in characters, not bytes", "[repository]")
{
    Fixture Test;


    // Every character is two bytes in UTF-8, so a byte-counting limit would cut this in half.
    std::string Accented;

    for (int Index = 0; Index < MaximumCardFieldLength; ++Index)
    {
        Accented += "é";
    }

    const std::optional<std::string> CardId =
        Test.Repository->AddCard(MakeCard(Accented, "dog"));

    REQUIRE(CardId.has_value());

    const std::optional<FTCardHandle> Stored = Test.Repository->LoadCard(*CardId);
    REQUIRE(Stored.has_value());

    CHECK((*Stored)->TargetWord == Accented);
}

TEST_CASE("A word inside the field limit is stored whole", "[repository]")
{
    Fixture Test;


    const std::string Word(MaximumCardFieldLength, 'a');

    const std::optional<std::string> CardId =
        Test.Repository->AddCard(MakeCard(Word, "dog"));

    REQUIRE(CardId.has_value());

    const std::optional<FTCardHandle> Stored = Test.Repository->LoadCard(*CardId);
    REQUIRE(Stored.has_value());

    CHECK((*Stored)->TargetWord == Word);
}

TEST_CASE("A card already handed out sees a later edit", "[repository][cardstore]")
{
    // The bug this covers: a lesson takes its words when it starts, so editing a translation
    // mid-lesson left the old one on screen until the lesson was rebuilt.
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("le chien", "dog")}));

    const std::vector<FTCardHandle> Running = Test.Repository->LoadUnlearnedCards();
    REQUIRE(Running.size() == 1);
    REQUIRE(Running.front()->Translation == "dog");

    const std::string CardId = Running.front()->Id;

    REQUIRE(Test.Repository->UpdateCardContent(CardId, "le chien", "hound", std::nullopt, "", ""));

    CHECK(Running.front()->Translation == "hound");
}

TEST_CASE("A card handed out twice is the same object", "[repository][cardstore]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("le chien", "dog")}));

    const std::vector<FTCardHandle> First = Test.Repository->LoadUnlearnedCards();
    const std::vector<FTCardHandle> Second = Test.Repository->LoadUnlearnedCards();

    REQUIRE(First.size() == 1);
    REQUIRE(Second.size() == 1);
    CHECK(First.front().get() == Second.front().get());
}

TEST_CASE("A card already handed out sees a schedule change", "[repository][cardstore]")
{
    // Promotion writes the row from a copy, so the copy has to reach whoever is holding the card.
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("le chien", "dog")}));

    const std::vector<FTCardHandle> Running = Test.Repository->LoadUnlearnedCards();
    REQUIRE(Running.size() == 1);
    REQUIRE(Running.front()->LeitnerStage == PoolStage);

    FTCard Moved = *Running.front();
    Moved.LeitnerStage = FirstLadderStage;
    Moved.DueDate = 20000;

    REQUIRE(Test.Repository->UpdateCardSchedule(Moved));

    CHECK(Running.front()->LeitnerStage == FirstLadderStage);
    CHECK(Running.front()->DueDate == 20000);
}

TEST_CASE("Every card a load returns has been through the annotator, one batch per load",
          "[repository][annotator]")
{
    Fixture Test;

    REQUIRE(AddCards(*Test.Repository, {MakeCard("homme", "man"), MakeCard("chien", "dog"),
                                                    MakeCard("chat", "cat")}));
    REQUIRE(Test.Repository->PromoteCards(CardIdsOf(Test.Repository->LoadCards())));

    const std::vector<FTCardHandle> Plain = Test.Repository->LoadCards();
    REQUIRE(Plain.size() == 3);
    CHECK_FALSE(Plain.front()->bElidesArticle);

    int Batches = 0;
    size_t Annotated = 0;

    Test.Repository->SetCardAnnotator([&](std::vector<FTCard>& Cards)
    {
        ++Batches;
        Annotated += Cards.size();

        for (FTCard& Card : Cards)
        {
            Card.bElidesArticle = Card.TargetWord.front() == 'h';
        }
    });

    const auto IsMarked = [](const FTCard& Card)
    {
        return Card.bElidesArticle == (Card.TargetWord == "homme");
    };

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    REQUIRE(Loaded.size() == 3);

    CHECK(Batches == 1);
    CHECK(Annotated == 3);

    for (const FTCardHandle& Card : Loaded)
    {
        CHECK(IsMarked(*Card));
    }

    CHECK(Plain.front()->bElidesArticle);

    const std::optional<FTCardHandle> Single = Test.Repository->LoadCard(Loaded.front()->Id);
    REQUIRE(Single.has_value());
    CHECK((*Single)->TargetWord == "homme");
    CHECK((*Single)->bElidesArticle);
    CHECK(Batches == 2);

    const std::vector<FTCard> Recent = Test.Repository->LoadRecentlyStudiedCards(10);
    REQUIRE(Recent.size() == 3);
    CHECK(Batches == 3);

    for (const FTCard& Card : Recent)
    {
        CHECK(IsMarked(Card));
    }

    const std::vector<FTCard> Found = Test.Repository->FindCardsByHeadword("ho", 10);
    REQUIRE(Found.size() == 1);
    CHECK(Found.front().bElidesArticle);
    CHECK(Batches == 4);

    CHECK(Test.Repository->FindCardsByHeadword("zz", 10).empty());
    CHECK(Batches == 4);
    CHECK_FALSE(Test.Repository->LoadCard("no-such-card").has_value());
    CHECK(Batches == 4);
}

static FTCard MakeFormsCardOf(const std::string& Id, const std::string& Word, const std::string& Translation)
{
    FTCard Card = MakeCard(Word, Translation);
    Card.Id = Id;

    return Card;
}

TEST_CASE("A forms card waits until it is marked learned, and keeps its own table", "[repository][conjugation]")
{
    Fixture Test;
    Test.Repository->SetActiveLanguages("fr", "en");


    const FTCard Vouloir = MakeFormsCardOf("conjugation-fr-vouloir-present", "vouloir", "want");
    const FTCard Dire = MakeFormsCardOf("conjugation-fr-dire-present", "dire", "say");

    REQUIRE(Test.Repository->AddFormsCards({Vouloir}));

    ++Test.FakeNow;

    REQUIRE(Test.Repository->AddFormsCards({Dire}));

    const std::vector<FTCardHandle> Waiting = Test.Repository->LoadFormsCards();

    REQUIRE(Waiting.size() == 2);
    CHECK(Waiting[0]->Id == Vouloir.Id);
    CHECK(Waiting[0]->TargetWord == "vouloir");
    CHECK(Waiting[0]->Translation == "want");
    CHECK_FALSE(IsCompleted(*Waiting[0]));
    CHECK(Waiting[1]->Id == Dire.Id);

    REQUIRE(Test.Repository->MarkFormsCardsLearned({Vouloir.Id}, 20000));

    const std::optional<FTCardHandle> Learned = Test.Repository->LoadFormsCard(Vouloir.Id);
    REQUIRE(Learned.has_value());
    CHECK((*Learned)->CompletedDate == 20000);

    const std::optional<FTCardHandle> StillWaiting = Test.Repository->LoadFormsCard(Dire.Id);
    REQUIRE(StillWaiting.has_value());
    CHECK_FALSE(IsCompleted(**StillWaiting));

    CHECK_FALSE(Test.Repository->LoadCard(Vouloir.Id).has_value());
    CHECK(Test.Repository->CountUnlearnedCards() == 0);
    CHECK(Test.Repository->CountDueCards(30000) == 0);
    CHECK(Test.Repository->ListKnownHeadwords().empty());

    const std::vector<FTCardHandle> Loaded = Test.Repository->LoadCards();
    CHECK(Loaded.empty());
}

TEST_CASE("Adding a forms card again leaves the stored one as it was", "[repository][conjugation]")
{
    Fixture Test;
    Test.Repository->SetActiveLanguages("fr", "en");

    const FTCard Vouloir = MakeFormsCardOf("conjugation-fr-vouloir-present", "vouloir", "want");

    REQUIRE(Test.Repository->AddFormsCards({Vouloir}));
    REQUIRE(Test.Repository->MarkFormsCardsLearned({Vouloir.Id}, 20000));

    REQUIRE(Test.Repository->AddFormsCards({MakeFormsCardOf(Vouloir.Id, "vouloir", "wish")}));

    const std::vector<FTCardHandle> Stored = Test.Repository->LoadFormsCards();

    REQUIRE(Stored.size() == 1);
    CHECK(Stored[0]->Translation == "want");
    CHECK(Stored[0]->CompletedDate == 20000);

    REQUIRE(Test.Repository->MarkFormsCardsLearned({Vouloir.Id}, 20005));
    CHECK((*Test.Repository->LoadFormsCard(Vouloir.Id))->CompletedDate == 20000);
}

TEST_CASE("A forms card without an id or a language pair is refused and nothing is written",
          "[repository][conjugation]")
{
    Fixture Test;

    const FTCard Vouloir = MakeFormsCardOf("conjugation-fr-vouloir-present", "vouloir", "want");

    CHECK_FALSE(Test.Repository->AddFormsCards({Vouloir}));

    Test.Repository->SetActiveLanguages("fr", "en");

    CHECK_FALSE(Test.Repository->AddFormsCards({Vouloir, MakeCard("dire", "say")}));
    CHECK(Test.Repository->LoadFormsCards().empty());
}

TEST_CASE("Each language pair has its own forms cards", "[repository][conjugation]")
{
    Fixture Test;
    Test.Repository->SetActiveLanguages("fr", "en");

    const FTCard Vouloir = MakeFormsCardOf("conjugation-fr-vouloir-present", "vouloir", "want");

    REQUIRE(Test.Repository->AddFormsCards({Vouloir}));

    Test.Repository->SetActiveLanguages("es", "en");

    CHECK(Test.Repository->LoadFormsCards().empty());
    CHECK_FALSE(Test.Repository->LoadFormsCard(Vouloir.Id).has_value());

    REQUIRE(Test.Repository->AddFormsCards({MakeFormsCardOf("conjugation-es-querer-present", "querer", "want")}));
    CHECK(Test.Repository->LoadFormsCards().size() == 1);

    Test.Repository->SetActiveLanguages("fr", "en");

    REQUIRE(Test.Repository->LoadFormsCards().size() == 1);
    CHECK(Test.Repository->LoadFormsCards()[0]->Id == Vouloir.Id);
}

TEST_CASE("Known verb tenses are the conjugation cards that were learned", "[repository][conjugation]")
{
    Fixture Test;
    Test.Repository->SetActiveLanguages("fr", "en");

    REQUIRE(Test.Repository->AddFormsCards({
        MakeFormsCardOf("conjugation-fr-vouloir-present", "vouloir", "want"),
        MakeFormsCardOf("conjugation-fr-vouloir-future", "vouloir", "want"),
        MakeFormsCardOf("conjugation-fr-dire-present", "dire", "say"),
        MakeFormsCardOf("forms-adj-fr-beau", "beau", "beautiful")}));

    CHECK(Test.Repository->ListKnownVerbTenses().empty());

    REQUIRE(Test.Repository->MarkFormsCardsLearned(
        {"conjugation-fr-vouloir-present", "conjugation-fr-vouloir-future", "forms-adj-fr-beau"}, 20000));

    const std::vector<FTKnownVerbTense> Known = Test.Repository->ListKnownVerbTenses();

    REQUIRE(Known.size() == 2);
    CHECK(Known[0].Headword == "vouloir");
    CHECK(Known[0].Tense == "present");
    CHECK(Known[1].Headword == "vouloir");
    CHECK(Known[1].Tense == "future");
}

TEST_CASE("Studied headwords are the cards on the ladder or completed, oldest first",
          "[repository][conjugation]")
{
    Fixture Test;
    Test.Repository->SetActiveLanguages("fr", "en");

    std::vector<std::string> CardIds;

    for (const char* Verb : {"parler", "manger", "finir"})
    {
        FTCard Card = MakeCard(Verb, "translation");
        Card.PartOfSpeech = "verb";

        ++Test.FakeNow;

        const std::optional<std::string> CardId = Test.Repository->AddCard(Card);
        REQUIRE(CardId.has_value());

        CardIds.push_back(*CardId);
    }

    FTCard Noun = MakeCard("chien", "dog");
    Noun.PartOfSpeech = "noun";

    ++Test.FakeNow;

    const std::optional<std::string> NounId = Test.Repository->AddCard(Noun);
    REQUIRE(NounId.has_value());

    FTCard Elsewhere = MakeCard("venir", "come");
    Elsewhere.PartOfSpeech = "verb";

    ++Test.FakeNow;

    const std::optional<std::string> ElsewhereId = Test.Repository->AddCard(Elsewhere);
    REQUIRE(ElsewhereId.has_value());
    REQUIRE(Test.Repository->PromoteCards({*ElsewhereId}));

    REQUIRE(Test.Repository->ListStudiedHeadwords().size() == 1);
    CHECK(Test.Repository->ListStudiedHeadwords()[0].Headword == "venir");

    REQUIRE(Test.Repository->PromoteCards({CardIds[0], *NounId}));
    REQUIRE(Test.Repository->MarkCardCompleted(CardIds[2]));

    const std::vector<FTStudiedHeadword> Studied = Test.Repository->ListStudiedHeadwords();

    REQUIRE(Studied.size() == 4);
    CHECK(Studied[0].Headword == "parler");
    CHECK(Studied[0].PartOfSpeech == "verb");
    CHECK(Studied[1].Headword == "finir");
    CHECK(Studied[2].Headword == "chien");
    CHECK(Studied[2].PartOfSpeech == "noun");
    CHECK(Studied[3].Headword == "venir");
}

TEST_CASE("A placement cutoff is kept per language being learned", "[repository][placement]")
{
    Fixture Test;

    Test.Repository->SetActiveLanguages("fr", "en");

    CHECK_FALSE(Test.Repository->FindPlacementCutoff("fr").has_value());
    CHECK(Test.Repository->GetPlacementCutoff() == 0);

    REQUIRE(Test.Repository->SetPlacementCutoff("fr", 1200));
    REQUIRE(Test.Repository->SetPlacementCutoff("es", 0));

    CHECK(Test.Repository->FindPlacementCutoff("fr") == 1200);
    CHECK(Test.Repository->FindPlacementCutoff("es") == 0);
    CHECK_FALSE(Test.Repository->FindPlacementCutoff("pt").has_value());
    CHECK(Test.Repository->GetPlacementCutoff() == 1200);

    REQUIRE(Test.Repository->SetPlacementCutoff("fr", 800));
    CHECK(Test.Repository->GetPlacementCutoff() == 800);

    Test.Repository->SetActiveLanguages("es", "en");
    CHECK(Test.Repository->GetPlacementCutoff() == 0);

    CHECK_FALSE(Test.Repository->SetPlacementCutoff("", 500));

    REQUIRE(Test.Repository->DeleteEverything());
    CHECK_FALSE(Test.Repository->FindPlacementCutoff("fr").has_value());
}

TEST_CASE("Deleting a language erases every row of that language and leaves other languages alone",
          "[repository]")
{
    Fixture Test;

    for (const std::string& Language : {std::string("es"), std::string("fr")})
    {
        Test.Repository->SetActiveLanguages(Language, "en");
        REQUIRE(Test.Repository->AddCard(MakeCard(Language + "-word", "word")).has_value());
        REQUIRE(Test.Repository->AddFormsCards(
            {MakeFormsCardOf("conjugation-" + Language + "-verb-present", Language + "-verb", "do")}));
        REQUIRE(Test.Repository->MarkWordTriaged(Language + "-known", "noun", 1));
        REQUIRE(Test.Repository->RecordSearch(Language + "-searched", "noun", 2));
        REQUIRE(Test.Repository->SetPlacementCutoff(Language, 300));
        REQUIRE(Test.Repository->RecordDailyProgress(20000, 5, 10, 25));
    }

    Test.FakeNow = 5000;
    REQUIRE(Test.Repository->DeleteLanguage("es"));

    for (const char* Table : {"Cards", "WordForms", "TriagedWords", "SearchHistory", "Placements", "DailyStats"})
    {
        INFO(Table);

        FTStatement Rows{Test.Target->Prepare(std::string("SELECT TargetLanguage, COUNT(*) FROM ") + Table
                                              + " GROUP BY TargetLanguage ORDER BY TargetLanguage")};
        REQUIRE(Rows.Step());
        CHECK(Rows.GetText(0) == "fr");
        CHECK(Rows.GetInt(1) == 1);
        CHECK_FALSE(Rows.Step());
    }

    FTStatement Erased{Test.Target->Prepare("SELECT Scope, ErasedDate FROM Erasures")};
    REQUIRE(Erased.Step());
    CHECK(Erased.GetText(0) == "language:es");
    CHECK(Erased.GetInt64(1) == 5000);
    CHECK_FALSE(Erased.Step());
}

TEST_CASE("Deleting a language forgets its triaged words and keeps other languages' ones", "[repository]")
{
    Fixture Test;
    Test.Repository->SetActiveLanguages("es", "en");
    REQUIRE(Test.Repository->MarkWordTriaged("perro", "noun", 1));

    Test.Repository->SetActiveLanguages("fr", "en");
    REQUIRE(Test.Repository->MarkWordTriaged("chien", "noun", 1));

    Test.FakeNow = 5000;
    REQUIRE(Test.Repository->DeleteLanguage("es"));

    CHECK(Test.Repository->IsWordTriaged("chien", "noun", 1));

    Test.Repository->SetActiveLanguages("es", "en");
    CHECK_FALSE(Test.Repository->IsWordTriaged("perro", "noun", 1));
}

TEST_CASE("Deleting a language with no words succeeds and changes nothing", "[repository]")
{
    Fixture Test;
    Test.Repository->SetActiveLanguages("fr", "en");
    REQUIRE(Test.Repository->AddCard(MakeCard("chien", "dog")).has_value());

    CHECK(Test.Repository->DeleteLanguage("es"));
    CHECK_FALSE(Test.Repository->DeleteLanguage(""));
    CHECK(Test.Repository->CountLanguageCards("fr") == 1);
}

TEST_CASE("Card hint survives loading editing and clearing", "[repository][hint]")
{
    Fixture State;
    FTCard Card = MakeCard("chat", "cat");
    Card.Hint = "Le chat dort sur le canapé. Une phrase conservée sans limite de longueur pour le contexte original.";
    const auto Id = State.Repository->AddCard(Card);
    REQUIRE(Id.has_value());
    const auto Loaded = State.Repository->LoadCard(*Id);
    REQUIRE(Loaded.has_value());
    CHECK((*Loaded)->Hint == Card.Hint);
    REQUIRE(State.Repository->UpdateCardContent(*Id, "chat", "cat", {}, "noun", "", "Mon chat dort."));
    CHECK((*Loaded)->Hint == "Mon chat dort.");
    REQUIRE(State.Repository->UpdateCardContent(*Id, "chat", "cat", {}, "noun", "", ""));
    CHECK((*Loaded)->Hint.empty());
}

TEST_CASE("Sentence context uses the selected occurrence and preserves punctuation", "[hint]")
{
    const QString Text{QString::fromUtf8("Le chat dort. Le chat joue ! Ensuite il mange.")};
    CHECK(FindSentenceAt(Text, 3, "fr") == QString::fromUtf8("Le chat dort."));
    CHECK(FindSentenceAt(Text, 16, "fr") == QString::fromUtf8("Le chat joue !"));
    CHECK(FindSentenceAt(Text, 34, "fr") == QString::fromUtf8("Ensuite il mange."));
    CHECK(FindSentenceAt(QString::fromUtf8("Le chat\ndort sans bruit"), 3, "fr") == QString::fromUtf8("Le chat dort sans bruit"));
    CHECK(FindSentenceAt(Text, -1, "fr").isEmpty());
    CHECK(FindSentenceAt(QString(), 0, "fr").isEmpty());
}

static QStringList SplitSentenceTexts(const char* Text, const std::string& LanguageCode)
{
    const QString Source{QString::fromUtf8(Text)};
    QStringList Sentences;

    for (const FTSentenceRange& Range : SplitSentences(Source, LanguageCode))
    {
        Sentences.append(Source.mid(Range.Start, Range.End - Range.Start).simplified());
    }

    return Sentences;
}

TEST_CASE("Sentence splitting keeps French abbreviations and initials inside the sentence", "[hint]")
{
    CHECK(SplitSentenceTexts("M. Dupont entra dans la pièce. Il était tard.", "fr")
          == QStringList{QString::fromUtf8("M. Dupont entra dans la pièce."), QString::fromUtf8("Il était tard.")});
    CHECK(SplitSentenceTexts("Mme Leroux et M. Bernard parlaient. C'était en 1832. M. Lebrun le savait.", "fr")
          == QStringList{QString::fromUtf8("Mme Leroux et M. Bernard parlaient."), QString::fromUtf8("C'était en 1832."),
                         QString::fromUtf8("M. Lebrun le savait.")});
    CHECK(SplitSentenceTexts("Il habitait av. Foch, près de St. Michel. Voir p. 12.", "fr")
          == QStringList{QString::fromUtf8("Il habitait av. Foch, près de St. Michel."), QString::fromUtf8("Voir p. 12.")});
    CHECK(SplitSentenceTexts("Il lisait J. R. R. Tolkien. Puis il dormit.", "fr")
          == QStringList{QString::fromUtf8("Il lisait J. R. R. Tolkien."), QString::fromUtf8("Puis il dormit.")});
}

TEST_CASE("Sentence splitting continues dialogue that goes on in lowercase", "[hint]")
{
    CHECK(SplitSentenceTexts("— Vraiment ? dit-il. Je ne le crois pas.", "fr")
          == QStringList{QString::fromUtf8("— Vraiment ? dit-il."), QString::fromUtf8("Je ne le crois pas.")});
    CHECK(SplitSentenceTexts("« Vraiment ! » s'écria-t-il. — Oui. — Bon.", "fr")
          == QStringList{QString::fromUtf8("« Vraiment ! » s'écria-t-il."), QString::fromUtf8("— Oui."),
                         QString::fromUtf8("— Bon.")});
    CHECK(SplitSentenceTexts("¿Vienes? —preguntó la Sra. García. No lo sé.", "es")
          == QStringList{QString::fromUtf8("¿Vienes? —preguntó la Sra. García."), QString::fromUtf8("No lo sé.")});
}

TEST_CASE("Sentence splitting uses the abbreviations of the given language only", "[hint]")
{
    CHECK(SplitSentenceTexts("Mr. Holmes smiled. He knew.", "en")
          == QStringList{QString::fromUtf8("Mr. Holmes smiled."), QString::fromUtf8("He knew.")});
    CHECK(SplitSentenceTexts("Ver pág. 4 del Sr. Ruiz. Fin.", "es")
          == QStringList{QString::fromUtf8("Ver pág. 4 del Sr. Ruiz."), QString::fromUtf8("Fin.")});
    CHECK(SplitSentenceTexts("Il dit etc. Puis il partit.", "fr")
          == QStringList{QString::fromUtf8("Il dit etc."), QString::fromUtf8("Puis il partit.")});
    CHECK(SplitSentenceTexts("Il parla au Sr. Puis il partit.", "fr")
          == QStringList{QString::fromUtf8("Il parla au Sr."), QString::fromUtf8("Puis il partit.")});
}
