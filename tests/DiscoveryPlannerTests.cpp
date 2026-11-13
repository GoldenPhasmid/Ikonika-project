#include <catch2/catch_test_macros.hpp>

#include "core/FTDiscoveryPlanner.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct FTestWord
    {
        FTDiscoveryRow Row;

        std::vector<FTDictionarySense> Meanings;
    };

    FTDictionarySense MakeMeaning(std::string Gist, std::vector<std::string> Translations, std::string Example)
    {
        FTDictionarySense Meaning;
        Meaning.Gist = std::move(Gist);
        Meaning.Example = std::move(Example);
        Meaning.ExampleTranslation = "translated example";

        for (std::string& Text : Translations)
        {
            Meaning.Translations.push_back(FTDictionaryTranslation{std::move(Text), std::nullopt});
        }

        return Meaning;
    }

    FTestWord MakeWord(int64_t DeckId, std::string Headword, std::string PartOfSpeech, int Rank,
                       std::vector<FTDictionarySense> Meanings, std::string Gist = {})
    {
        FTestWord Word;
        Word.Row.DeckId = DeckId;
        Word.Row.Headword = std::move(Headword);
        Word.Row.PartOfSpeech = std::move(PartOfSpeech);
        Word.Row.Gist = std::move(Gist);
        Word.Row.TeachingRank = Rank;
        Word.Row.Translation = Meanings.front().Translations.front().Text;
        Word.Row.bAskable = true;
        Word.Meanings = std::move(Meanings);

        return Word;
    }

    std::vector<FTestWord> MakeDeck()
    {
        std::vector<FTestWord> Words;

        for (int Index = 0; Index < 40; ++Index)
        {
            const std::string Headword = "mot" + std::to_string(Index);

            Words.push_back(MakeWord(1, Headword, "noun", 100 + Index * 150,
                                     {MakeMeaning("sens", {"word" + std::to_string(Index)}, "Le " + Headword + " est ici.")}));
        }

        for (int Index = 0; Index < 20; ++Index)
        {
            const std::string Headword = "parler" + std::to_string(Index);

            Words.push_back(MakeWord(1, Headword, "verb", 120 + Index * 300,
                                     {MakeMeaning("dire", {"to say" + std::to_string(Index)}, "Il " + Headword + " souvent."),
                                      MakeMeaning("autre", {"to other" + std::to_string(Index)}, "Elle " + Headword + " ici.")}));
        }

        Words.push_back(MakeWord(1, "pêche", "noun", 900,
                                 {MakeMeaning("fruit", {"peach"}, "Je mange une pêche."),
                                  MakeMeaning("activité", {"fishing"}, "Il aime la pêche.")}, "fruit"));

        return Words;
    }

    std::vector<FTestWord> MakeBeginnerDeck()
    {
        std::vector<FTestWord> Words;

        for (int Index = 0; Index < 40; ++Index)
        {
            const std::string Headword = "mot" + std::to_string(Index);
            const std::string Translation = "word" + std::to_string(Index);

            FTDictionarySense Meaning = MakeMeaning("sens", {Translation}, "Le " + Headword + " est ici.");
            Meaning.ExampleTranslation = "The " + Translation + " is here.";

            Words.push_back(MakeWord(1, Headword, "noun", 100 + Index * 20, {Meaning}));
        }

        return Words;
    }

    FTDiscoveryPlanner MakePlanner(const std::vector<FTestWord>& Words, const FTDiscoveryLearner& Learner, uint32_t Seed,
                                   std::map<std::string, std::vector<std::string>> Forms = {},
                                   std::set<std::string> KnownExamples = {})
    {
        std::vector<FTDiscoveryRow> Rows;
        std::map<std::string, std::vector<FTDictionarySense>> Meanings;

        for (const FTestWord& Word : Words)
        {
            Rows.push_back(Word.Row);
            Meanings[Word.Row.Headword] = Word.Meanings;
        }

        FTDiscoverySources Sources;

        Sources.LoadMeanings = [Meanings](const FTDiscoveryRow& Row)
        {
            const auto Found = Meanings.find(Row.Headword);

            return Found != Meanings.end() ? Found->second : std::vector<FTDictionarySense>{};
        };

        Sources.LoadForms = [Forms](const FTDiscoveryRow& Row)
        {
            const auto Found = Forms.find(Row.Headword);

            return Found != Forms.end() ? Found->second : std::vector<std::string>{};
        };

        Sources.IsKnownExample = [KnownExamples](const FTDiscoveryRow&, const std::string& Example)
        {
            return KnownExamples.contains(Example);
        };

        return FTDiscoveryPlanner(std::move(Rows), std::move(Sources), Learner, FTDiscoveryPlanner::DefaultQuestionCount, Seed);
    }

    FTDiscoveryPlanner MakePlanner(const std::vector<FTestWord>& Words, int CutoffRank, uint32_t Seed)
    {
        return MakePlanner(Words, FTDiscoveryLearner{CutoffRank, FTDiscoveryPlanner::BeginnerWordCount}, Seed);
    }

    int CorrectIndex(const FTDiscoveryQuestion& Question)
    {
        for (std::size_t Index = 0; Index < Question.Options.size(); ++Index)
        {
            if (Question.Options[Index].bCorrect)
            {
                return static_cast<int>(Index);
            }
        }

        return -1;
    }

    int WrongIndex(const FTDiscoveryQuestion& Question)
    {
        for (std::size_t Index = 0; Index < Question.Options.size(); ++Index)
        {
            if (!Question.Options[Index].bCorrect && !Question.Struck[Index])
            {
                return static_cast<int>(Index);
            }
        }

        return -1;
    }

    void AnswerWrong(FTDiscoveryPlanner& Planner)
    {
        while (!Planner.GetCurrentQuestion()->Outcome)
        {
            const int Index = WrongIndex(*Planner.GetCurrentQuestion());

            REQUIRE(Index >= 0);

            Planner.Choose(Index);
        }
    }
}

TEST_CASE("Every Discover question has four distinct options and one right answer", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeDeck();

    for (uint32_t Seed = 1; Seed <= 40; ++Seed)
    {
        FTDiscoveryPlanner Planner = MakePlanner(Words, static_cast<int>(Seed) * 300, Seed);

        while (!Planner.IsFinished())
        {
            const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

            REQUIRE(Question.Options.size() == 4);

            std::set<std::string> Texts;
            int CorrectCount = 0;

            for (const FTDiscoveryOption& Option : Question.Options)
            {
                Texts.insert(NormalizeDiscoveryTranslation(Option.Text));
                CorrectCount += Option.bCorrect ? 1 : 0;
            }

            CHECK(Texts.size() == 4);
            CHECK(CorrectCount == 1);

            if (Seed % 2 == 0)
            {
                Planner.Choose(CorrectIndex(Question));
            }
            else
            {
                AnswerWrong(Planner);
            }

            Planner.Advance();
        }

        CHECK(Planner.GetAskedCount() <= Planner.GetQuestionCount());
    }
}

TEST_CASE("A Discover Word question never offers another meaning of the asked word", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeDeck();

    for (uint32_t Seed = 1; Seed <= 60; ++Seed)
    {
        FTDiscoveryPlanner Planner = MakePlanner(Words, 900, Seed);

        while (!Planner.IsFinished())
        {
            const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

            if (Planner.GetRows()[static_cast<std::size_t>(Question.RowIndex)].Headword == "pêche"
                && Question.Prompt == EFTDiscoveryPrompt::Word)
            {
                for (const FTDiscoveryOption& Option : Question.Options)
                {
                    CHECK(Option.Text != "fishing");
                }
            }

            Planner.Choose(CorrectIndex(Question));
            Planner.Advance();
        }
    }
}

TEST_CASE("A Discover Sentence question offers the word's other meanings as traps", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeDeck();

    bool bSawTrap = false;

    for (uint32_t Seed = 1; Seed <= 20 && !bSawTrap; ++Seed)
    {
        FTDiscoveryPlanner Planner = MakePlanner(Words, 900, Seed);

        while (!Planner.IsFinished())
        {
            const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

            if (Question.Prompt == EFTDiscoveryPrompt::Sentence)
            {
                CHECK(Question.ExampleSpanStart >= 0);

                for (const FTDiscoveryOption& Option : Question.Options)
                {
                    bSawTrap = bSawTrap || Option.bOtherMeaning;
                    CHECK(!(Option.bOtherMeaning && Option.bCorrect));
                }
            }

            Planner.Choose(CorrectIndex(Question));
            Planner.Advance();
        }
    }

    CHECK(bSawTrap);
}

TEST_CASE("An Almost answer leaves the Discover level estimate unchanged", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeDeck();

    FTDiscoveryPlanner Planner = MakePlanner(Words, 1200, 7);

    const int Before = Planner.GetLevelCutoffRank();

    Planner.AskClue();
    Planner.Choose(CorrectIndex(*Planner.GetCurrentQuestion()));

    REQUIRE(Planner.GetCurrentQuestion()->Outcome == EFTDiscoveryOutcome::Almost);
    CHECK(Planner.GetLevelCutoffRank() == Before);
    CHECK(Planner.GetResults().back().bLearn);
}

TEST_CASE("Show me settles a Discover question as New after the last clue", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeDeck();

    FTDiscoveryPlanner Planner = MakePlanner(Words, 1200, 3);

    const int ClueCount = GetDiscoveryClueCount(Planner.GetCurrentQuestion()->Prompt);

    for (int Step = 0; Step < ClueCount; ++Step)
    {
        Planner.AskClue();
        CHECK(!Planner.GetCurrentQuestion()->Outcome);
    }

    int Struck = 0;

    for (const bool bStruck : Planner.GetCurrentQuestion()->Struck)
    {
        Struck += bStruck ? 1 : 0;
    }

    CHECK(Struck == 2);

    Planner.AskClue();

    CHECK(Planner.GetCurrentQuestion()->Outcome == EFTDiscoveryOutcome::New);
}

TEST_CASE("A wrong Discover answer before the sentence shows the sentence", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeDeck();
    int Checked = 0;

    for (uint32_t Seed = 1; Seed <= 20; ++Seed)
    {
        FTDiscoveryPlanner Planner = MakePlanner(Words, 1200, Seed);

        const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

        if (Question.Prompt == EFTDiscoveryPrompt::Sentence)
        {
            continue;
        }

        const int Wrong = WrongIndex(Question);

        Planner.Choose(Wrong);

        REQUIRE(!Question.Outcome);
        CHECK(ShowsDiscoveryExample(Question));
        CHECK(Question.Struck[static_cast<std::size_t>(Wrong)]);
        CHECK(std::count(Question.Struck.begin(), Question.Struck.end(), true) == 1);

        Planner.AskClue();

        REQUIRE(!Question.Outcome);
        CHECK(std::count(Question.Struck.begin(), Question.Struck.end(), true) == 2);

        Planner.Choose(CorrectIndex(Question));

        CHECK(Question.Outcome == EFTDiscoveryOutcome::Almost);
        ++Checked;
    }

    CHECK(Checked > 0);
}

TEST_CASE("A wrong Discover answer with the sentence shown settles as New", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeDeck();

    for (uint32_t Seed = 1; Seed <= 20; ++Seed)
    {
        FTDiscoveryPlanner Planner = MakePlanner(Words, 1200, Seed);

        const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();
        const bool bSentence = Question.Prompt == EFTDiscoveryPrompt::Sentence;

        AnswerWrong(Planner);

        CHECK(Question.Outcome == EFTDiscoveryOutcome::New);
        CHECK(Question.ClueStep == (bSentence ? 0 : 1));
        CHECK(std::count(Question.Struck.begin(), Question.Struck.end(), true) == (bSentence ? 0 : 1));
    }
}

TEST_CASE("A Beginner question swaps the native word for the French one", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeBeginnerDeck();

    for (uint32_t Seed = 1; Seed <= 10; ++Seed)
    {
        FTDiscoveryPlanner Planner = MakePlanner(Words, FTDiscoveryLearner{300, 300}, Seed);

        REQUIRE(Planner.IsBeginner());

        const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();
        const std::string& Headword = Planner.GetRows()[static_cast<std::size_t>(Question.RowIndex)].Headword;

        REQUIRE(Question.Prompt == EFTDiscoveryPrompt::Beginner);
        CHECK(Question.BeginnerSentence == "The " + Headword + " is here.");
        CHECK(Question.BeginnerSpanStart == 4);
        CHECK(Question.BeginnerSpanLength == static_cast<int>(Headword.size()));
        CHECK(Question.Options.size() == 4);
    }
}

TEST_CASE("A Beginner verb question swaps the whole verb group", "[discovery]")
{
    FTDictionarySense Won = MakeMeaning("victoire", {"to win"}, "Notre équipe a gagné le match hier soir.");
    Won.ExampleTranslation = "Our team won the match last night.";

    std::vector<FTestWord> Words{MakeWord(1, "gagner", "verb", 400, {Won})};

    const std::vector<std::pair<std::string, std::string>> Others{
        {"perdre", "to lose"}, {"jouer", "to play"}, {"courir", "to run"}, {"chanter", "to sing"}};

    for (const auto& [Headword, Translation] : Others)
    {
        FTestWord Word = MakeWord(1, Headword, "verb", 420, {MakeMeaning("sens", {Translation}, "Il va " + Headword + ".")});
        Word.Row.bAskable = false;

        Words.push_back(std::move(Word));
    }

    FTDiscoveryPlanner Planner = MakePlanner(Words, FTDiscoveryLearner{300, 300}, 1, {{"gagner", {"gagne", "gagné", "gagnons"}}});

    const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

    REQUIRE(Question.Prompt == EFTDiscoveryPrompt::Beginner);
    CHECK(Question.BeginnerSentence == "Our team a gagné the match last night.");
    CHECK(Question.BeginnerSpanStart == 9);
    CHECK(Question.BeginnerSpanLength == 7);
}

TEST_CASE("A right Beginner answer is Known, defaults to Learn and leaves the estimate unchanged", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeBeginnerDeck();

    FTDiscoveryPlanner Planner = MakePlanner(Words, FTDiscoveryLearner{300, 300}, 5);

    const int Before = Planner.GetLevelCutoffRank();

    REQUIRE(Planner.GetCurrentQuestion()->Prompt == EFTDiscoveryPrompt::Beginner);

    Planner.Choose(CorrectIndex(*Planner.GetCurrentQuestion()));

    REQUIRE(Planner.GetCurrentQuestion()->Outcome == EFTDiscoveryOutcome::Known);
    CHECK(Planner.GetLevelCutoffRank() == Before);
    CHECK(Planner.GetResults().back().bLearn);
}

TEST_CASE("A Beginner question shows the French example when its other words are known", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeBeginnerDeck();

    std::set<std::string> KnownExamples;

    for (const FTestWord& Word : Words)
    {
        KnownExamples.insert(Word.Meanings.front().Example);
    }

    FTDiscoveryPlanner Planner = MakePlanner(Words, FTDiscoveryLearner{300, 300}, 8, {}, KnownExamples);

    const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

    REQUIRE(Planner.IsBeginner());
    CHECK(Question.Prompt == EFTDiscoveryPrompt::Sentence);
    CHECK(Question.BeginnerSentence.empty());
    CHECK(Question.ExampleSpanStart == 3);
}

TEST_CASE("A wrong first Beginner answer strikes two options instead of settling", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeBeginnerDeck();

    FTDiscoveryPlanner Planner = MakePlanner(Words, FTDiscoveryLearner{300, 300}, 4);

    const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

    REQUIRE(Question.Prompt == EFTDiscoveryPrompt::Beginner);

    Planner.Choose(WrongIndex(Question));

    REQUIRE(!Question.Outcome);
    CHECK(std::count(Question.Struck.begin(), Question.Struck.end(), true) == 2);

    Planner.Choose(WrongIndex(Question));

    CHECK(Question.Outcome == EFTDiscoveryOutcome::New);
}

TEST_CASE("A Beginner question asks the translation its sentence uses", "[discovery]")
{
    std::vector<FTestWord> Words = MakeBeginnerDeck();

    FTDictionarySense Hall = MakeMeaning("passage", {"hall", "hallway", "corridor"}, "Le couloir est sombre.");
    Hall.ExampleTranslation = "The hallway is dark.";

    for (FTestWord& Word : Words)
    {
        Word.Row.bAskable = false;
    }

    Words.push_back(MakeWord(1, "couloir", "noun", 400, {Hall}));

    FTDiscoveryPlanner Planner = MakePlanner(Words, FTDiscoveryLearner{300, 300}, 2);

    const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

    REQUIRE(Question.Prompt == EFTDiscoveryPrompt::Beginner);
    CHECK(Question.BeginnerSentence == "The couloir is dark.");
    CHECK(Question.Options[static_cast<std::size_t>(CorrectIndex(Question))].Text == "hallway");

    Planner.Choose(CorrectIndex(Question));

    CHECK(Planner.GetResults().back().Translation == "hallway");
}

TEST_CASE("A standard Discover question asks the translation its example uses", "[discovery]")
{
    std::vector<FTestWord> Words = MakeBeginnerDeck();

    FTDictionarySense Hall = MakeMeaning("passage", {"hall", "hallway", "corridor"}, "Le couloir est sombre.");
    Hall.ExampleTranslation = "The hallway is dark.";

    for (FTestWord& Word : Words)
    {
        Word.Row.bAskable = false;
    }

    Words.push_back(MakeWord(1, "couloir", "noun", 400, {Hall}));

    for (uint32_t Seed = 1; Seed <= 10; ++Seed)
    {
        FTDiscoveryPlanner Planner = MakePlanner(Words, 300, Seed);

        const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

        REQUIRE(Question.Prompt != EFTDiscoveryPrompt::Beginner);
        CHECK(Question.Options[static_cast<std::size_t>(CorrectIndex(Question))].Text == "hallway");

        Planner.Choose(CorrectIndex(Question));

        CHECK(Planner.GetResults().back().Translation == "hallway");
    }
}

TEST_CASE("Beginner mode defaults every answer to Learn", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeBeginnerDeck();

    FTDiscoveryPlanner Planner = MakePlanner(Words, FTDiscoveryLearner{300, 300}, 6);

    int Checked = 0;

    while (!Planner.IsFinished())
    {
        Planner.Choose(CorrectIndex(*Planner.GetCurrentQuestion()));

        if (Planner.IsBeginner())
        {
            CHECK(Planner.GetResults().back().bLearn);
            ++Checked;
        }

        Planner.Advance();
    }

    CHECK(Checked >= 4);
}

TEST_CASE("Beginner mode asks every question as a Beginner question", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeBeginnerDeck();

    FTDiscoveryPlanner Planner = MakePlanner(Words, FTDiscoveryLearner{300, 300}, 9);

    std::vector<EFTDiscoveryPrompt> Prompts;

    while (!Planner.IsFinished())
    {
        const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

        Prompts.push_back(Question.Prompt);

        if (Question.Prompt == EFTDiscoveryPrompt::Beginner)
        {
            Planner.Choose(CorrectIndex(Question));
        }
        else
        {
            AnswerWrong(Planner);
        }

        Planner.Advance();
    }

    REQUIRE(Prompts.size() == static_cast<std::size_t>(FTDiscoveryPlanner::DefaultQuestionCount));

    for (const EFTDiscoveryPrompt Prompt : Prompts)
    {
        CHECK(Prompt == EFTDiscoveryPrompt::Beginner);
    }
}

TEST_CASE("A learner who knows no word starts Discover in beginner mode", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeBeginnerDeck();

    FTDiscoveryPlanner Planner = MakePlanner(Words, FTDiscoveryLearner{}, 3);

    CHECK(Planner.IsBeginner());
    CHECK(Planner.GetCurrentQuestion()->Prompt == EFTDiscoveryPrompt::Beginner);
}

TEST_CASE("A Discover headword held by two decks is asked once per round", "[discovery]")
{
    std::vector<FTestWord> Words = MakeDeck();

    for (FTestWord Word : MakeDeck())
    {
        Word.Row.DeckId = 2;
        Words.push_back(std::move(Word));
    }

    for (uint32_t Seed = 1; Seed <= 20; ++Seed)
    {
        FTDiscoveryPlanner Planner = MakePlanner(Words, 900, Seed);

        std::map<std::string, int> Fresh;

        while (!Planner.IsFinished())
        {
            const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

            ++Fresh[Planner.GetRows()[static_cast<std::size_t>(Question.RowIndex)].Headword];

            Planner.Choose(CorrectIndex(Question));
            Planner.Advance();
        }

        for (const auto& [Headword, Count] : Fresh)
        {
            CHECK(Count == 1);
        }
    }
}

TEST_CASE("Discover results hold one row per question, Known defaulting to Know", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeDeck();

    FTDiscoveryPlanner Planner = MakePlanner(Words, 0, 11);

    while (!Planner.IsFinished())
    {
        Planner.Choose(CorrectIndex(*Planner.GetCurrentQuestion()));
        Planner.Advance();
    }

    CHECK(Planner.GetAskedCount() == FTDiscoveryPlanner::DefaultQuestionCount);
    REQUIRE(static_cast<int>(Planner.GetResults().size()) == Planner.GetAskedCount());

    for (const FTDiscoveryResult& Result : Planner.GetResults())
    {
        CHECK(Result.Outcome == EFTDiscoveryOutcome::Known);
        CHECK(!Result.bLearn);
    }
}

TEST_CASE("Discover never asks a word at or below the placement cutoff", "[discovery]")
{
    const std::vector<FTestWord> Words = MakeDeck();

    for (uint32_t Seed = 1; Seed <= 20; ++Seed)
    {
        FTDiscoveryPlanner Planner = MakePlanner(Words, 900, Seed);

        while (!Planner.IsFinished())
        {
            const FTDiscoveryQuestion& Question = *Planner.GetCurrentQuestion();

            CHECK(Planner.GetRows()[static_cast<std::size_t>(Question.RowIndex)].TeachingRank > 900);

            AnswerWrong(Planner);
            Planner.Advance();
        }
    }
}
