#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "SentenceTilesController.h"

#include "TestAppSpeech.h"

#include "core/FCardSelectorBase.h"
#include "core/FTSentenceCards.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace
{
    FTCardHandle MakeSentence(uint32_t Id, std::string Text, std::string Translation)
    {
        FTTreeSentence Sentence;
        Sentence.Id = Id;
        Sentence.Text = std::move(Text);
        Sentence.Translation = std::move(Translation);

        return MakeCardHandle(MakeSentenceCard(Sentence));
    }

    FTSessionStartParams MakeParams(std::vector<FTCardHandle> Cards)
    {
        FTSessionStartParams Params{};
        Params.CardState = std::make_shared<FCardState>(std::move(Cards));
        Params.Title = "Sentences";
        Params.TargetLanguage = "fr";
        Params.NativeLanguage = "en";
        Params.FixedLength = Params.CardState->GetCount();
        Params.Selector = std::make_shared<FEachOnceSelector>(Params.CardState->GetCount(), false);

        return Params;
    }

    FTSessionStartParams MakeApple()
    {
        return MakeParams({MakeSentence(1, "Je mange une pomme rouge.", "I eat a red apple.")});
    }

    void PlaceInOrder(SentenceTilesController& Controller, const QStringList& Order)
    {
        for (const QString& Word : Order)
        {
            const QStringList Tiles = Controller.GetTiles();
            const QVariantList Placed = Controller.GetPlaced();

            for (int Index = 0; Index < Tiles.size(); ++Index)
            {
                if (Tiles[Index] == Word && !Placed.contains(Index))
                {
                    Controller.placeTile(Index);

                    break;
                }
            }
        }
    }

    void PlaceAndSubmit(SentenceTilesController& Controller, const QStringList& Order)
    {
        PlaceInOrder(Controller, Order);
        Controller.submit();
    }

    const QStringList AppleWords{"Je", "mange", "une", "pomme", "rouge"};
}

TEST_CASE("The tiles are the sentence's words in another order", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    QStringList Sorted = Controller.GetTiles();
    QStringList Expected = AppleWords;
    Sorted.sort();
    Expected.sort();

    CHECK(Sorted == Expected);
    CHECK(Controller.GetTiles() != AppleWords);
    CHECK(Controller.GetPlaced().isEmpty());
}

TEST_CASE("The tiles exercise speaks the sentence and keeps its translation for later", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    CHECK(Controller.GetPrompt() == "I eat a red apple.");
    CHECK(Controller.GetAnswer() == "Je mange une pomme rouge.");
    CHECK_FALSE(Controller.ShowsTargetLanguage());
    CHECK(Controller.GetLastTranslation().isEmpty());
    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == "Je mange une pomme rouge.");
}

TEST_CASE("Replaying speaks the sentence again", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    Controller.replay();

    CHECK(SpeechGuard.Mock.SpeakCount == 2);
    CHECK(SpeechGuard.Mock.LastText == "Je mange une pomme rouge.");
}

TEST_CASE("The tiles exercise speaks at the sentence speech rate", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;
    SpeechGuard.Mock.SentenceSpeed = 0.75f;

    SentenceTilesController Controller;
    Controller.SetSpeaksSentences(true);
    Controller.Start(MakeApple());

    CHECK(SpeechGuard.Mock.LastRateScale == 0.75f);
}

TEST_CASE("Submitting every tile in order is a correct answer", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    PlaceAndSubmit(Controller, AppleWords);

    CHECK(Controller.IsAnswered());
    CHECK(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetLastRevealedAnswer() == "Je mange une pomme rouge.");
    CHECK(Controller.GetLastGivenAnswer() == "Je mange une pomme rouge");
    CHECK(Controller.GetLastTranslation() == "I eat a red apple.");
    CHECK(SpeechGuard.Mock.SpeakCount == 1);
}

TEST_CASE("Placing the last tile does not check the sentence", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    PlaceInOrder(Controller, {"Je", "mange", "une", "pomme"});

    CHECK_FALSE(Controller.IsAnswered());
    CHECK_FALSE(Controller.CanSubmit());
    CHECK(Controller.GetPlaced().size() == 4);

    PlaceInOrder(Controller, {"rouge"});

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.CanSubmit());
    CHECK(Controller.GetPlaced().size() == 5);
}

TEST_CASE("Submitting before every tile is placed does nothing", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    Controller.submit();
    CHECK_FALSE(Controller.IsAnswered());

    PlaceInOrder(Controller, {"Je", "mange", "une", "pomme"});
    Controller.submit();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetPlaced().size() == 4);
}

TEST_CASE("A submitted answer cannot be submitted again", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    PlaceAndSubmit(Controller, AppleWords);
    REQUIRE(Controller.IsAnswered());
    CHECK_FALSE(Controller.CanSubmit());

    const int Answered = Controller.GetCurrentSessionCount();

    Controller.submit();

    CHECK(Controller.GetCurrentSessionCount() == Answered);
}

TEST_CASE("Placing the tiles in the wrong order is a wrong answer", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    PlaceAndSubmit(Controller, {"mange", "Je", "une", "pomme", "rouge"});

    CHECK(Controller.IsAnswered());
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetLastRevealedAnswer() == "Je mange une pomme rouge.");
}

TEST_CASE("A placed tile can be taken back and placed again", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    PlaceInOrder(Controller, {"mange"});
    REQUIRE(Controller.GetPlaced().size() == 1);

    const int Misplaced = Controller.GetPlaced().front().toInt();

    Controller.placeTile(Misplaced);
    CHECK(Controller.GetPlaced().size() == 1);

    Controller.removeTile(Misplaced);
    CHECK(Controller.GetPlaced().isEmpty());

    Controller.removeTile(Misplaced);
    CHECK(Controller.GetPlaced().isEmpty());

    PlaceAndSubmit(Controller, AppleWords);

    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("A tile index outside the bank is ignored", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    Controller.placeTile(-1);
    Controller.placeTile(5);
    Controller.placeTile(100);

    CHECK(Controller.GetPlaced().isEmpty());
    CHECK_FALSE(Controller.IsAnswered());
}

TEST_CASE("Two tiles with the same word stand in for each other", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeParams({MakeSentence(1, "Le chat voit le chien.", "The cat sees the dog.")}));

    const QStringList Tiles = Controller.GetTiles();

    for (const QString& Word : QStringList{"Le", "chat", "voit"})
    {
        Controller.placeTile(static_cast<int>(Tiles.indexOf(Word)));
    }

    Controller.placeTile(static_cast<int>(Tiles.lastIndexOf("le")));
    Controller.placeTile(static_cast<int>(Tiles.indexOf("chien")));
    Controller.submit();

    CHECK(Controller.IsAnswered());
    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Two tiles that differ only in case stand in for each other", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeParams({MakeSentence(1, "Le chat voit le chien.", "The cat sees the dog.")}));

    const QStringList Tiles = Controller.GetTiles();

    Controller.placeTile(static_cast<int>(Tiles.indexOf("le")));

    for (const QString& Word : QStringList{"chat", "voit", "Le", "chien"})
    {
        Controller.placeTile(static_cast<int>(Tiles.indexOf(Word)));
    }

    Controller.submit();

    CHECK(Controller.IsAnswered());
    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("A typed sentence is correct whatever its case", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    Controller.submitTyped("JE MANGE une pomme rouge");

    CHECK(Controller.IsAnswered());
    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Skipping a sentence is a wrong answer that reveals it", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    Controller.skip();

    CHECK(Controller.IsAnswered());
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetLastRevealedAnswer() == "Je mange une pomme rouge.");
}

TEST_CASE("Tiles are locked once the sentence is answered", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    PlaceAndSubmit(Controller, AppleWords);
    REQUIRE(Controller.IsAnswered());

    const QVariantList Placed = Controller.GetPlaced();

    Controller.removeTile(Placed.front().toInt());
    Controller.placeTile(0);
    Controller.skip();

    CHECK(Controller.GetPlaced() == Placed);
    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Advancing deals the next sentence's tiles", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeParams({
        MakeSentence(1, "Je mange une pomme rouge.", "I eat a red apple."),
        MakeSentence(2, "Il fait très froid aujourd'hui.", "It is very cold today."),
    }));

    const QStringList ColdWords{"Il", "fait", "très", "froid", "aujourd'hui"};

    const auto WordsOfCurrent = [&]()
    {
        return Controller.GetPrompt() == "I eat a red apple." ? AppleWords : ColdWords;
    };

    const QString FirstPrompt = Controller.GetPrompt();

    PlaceAndSubmit(Controller, WordsOfCurrent());
    REQUIRE(Controller.IsAnswered());
    CHECK(Controller.WasLastAnswerCorrect());

    Controller.advance();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetPlaced().isEmpty());
    CHECK(Controller.GetPrompt() != FirstPrompt);
    CHECK(SpeechGuard.Mock.SpeakCount == 2);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetAnswer());

    QStringList Sorted = Controller.GetTiles();
    QStringList Expected = WordsOfCurrent();
    Sorted.sort();
    Expected.sort();

    CHECK(Sorted == Expected);

    PlaceAndSubmit(Controller, WordsOfCurrent());
    CHECK(Controller.WasLastAnswerCorrect());

    Controller.advance();

    CHECK(Controller.IsFinished());
    CHECK(Controller.GetTiles().isEmpty());
}

TEST_CASE("Advancing before an answer does nothing", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.Start(MakeApple());

    const QStringList Tiles = Controller.GetTiles();

    Controller.advance();

    CHECK(Controller.GetTiles() == Tiles);
    CHECK_FALSE(Controller.IsFinished());
}

namespace
{
    FTSessionStartParams MakeAppleAmongOthers()
    {
        return MakeParams({MakeSentence(1, "Je mange une pomme rouge.", "I eat a red apple."),
                           MakeSentence(2, "Le chat dort.", "The cat sleeps."),
                           MakeSentence(3, "Il pleut beaucoup.", "It rains a lot.")});
    }

    QStringList TranslationTiles(const SentenceTilesController& Controller, const QStringList& Answer)
    {
        QStringList Rest = Controller.GetTiles();

        for (const QString& Word : Answer)
        {
            Rest.removeOne(Word);
        }

        return Rest;
    }

    QStringList WordsOf(const QString& Translation)
    {
        QStringList Words;

        for (const std::string& Word : SplitSentenceWords(Translation.toStdString()))
        {
            Words.push_back(QString::fromStdString(Word));
        }

        return Words;
    }
}

TEST_CASE("Translation tiles hold the translation's words plus distractors from other sentences", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.SetBuildsTranslation(true);
    Controller.Start(MakeAppleAmongOthers());

    const QStringList Answer = WordsOf(Controller.GetPrompt());
    const QStringList Distractors = TranslationTiles(Controller, Answer);

    CHECK(Controller.GetTiles().size() == Answer.size() + SentenceTilesController::DistractorCount);
    CHECK(Distractors.size() == SentenceTilesController::DistractorCount);

    for (const QString& Distractor : Distractors)
    {
        CHECK_FALSE(Answer.contains(Distractor, Qt::CaseInsensitive));
    }

    CHECK(SpeechGuard.Mock.LastText == Controller.GetAnswer());
}

TEST_CASE("The translation's words in any order are a correct answer", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.SetBuildsTranslation(true);
    Controller.Start(MakeAppleAmongOthers());

    QStringList Answer = WordsOf(Controller.GetPrompt());
    std::reverse(Answer.begin(), Answer.end());

    PlaceAndSubmit(Controller, Answer);

    CHECK(Controller.IsAnswered());
    CHECK(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetLastRevealedAnswer() == Controller.GetAnswer());
}

TEST_CASE("Placing a distractor among the translation's words is a wrong answer", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.SetBuildsTranslation(true);
    Controller.Start(MakeAppleAmongOthers());

    QStringList Answer = WordsOf(Controller.GetPrompt());
    const QStringList Distractors = TranslationTiles(Controller, Answer);
    REQUIRE_FALSE(Distractors.isEmpty());

    Answer.removeLast();
    PlaceInOrder(Controller, Answer);
    CHECK_FALSE(Controller.IsAnswered());

    PlaceInOrder(Controller, {Distractors.first()});
    CHECK_FALSE(Controller.IsAnswered());

    Controller.submit();

    CHECK(Controller.IsAnswered());
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
}

TEST_CASE("A full answer takes no more tiles until one is taken back", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.SetBuildsTranslation(true);
    Controller.Start(MakeAppleAmongOthers());

    const QStringList Answer = WordsOf(Controller.GetPrompt());
    const QStringList Distractors = TranslationTiles(Controller, Answer);
    REQUIRE_FALSE(Distractors.isEmpty());

    PlaceInOrder(Controller, Answer);
    REQUIRE(Controller.CanSubmit());

    PlaceInOrder(Controller, {Distractors.first()});

    CHECK(Controller.GetPlaced().size() == Answer.size());

    Controller.removeTile(Controller.GetPlaced().last().toInt());

    CHECK_FALSE(Controller.CanSubmit());

    PlaceInOrder(Controller, {Distractors.first()});
    Controller.submit();

    CHECK(Controller.IsAnswered());
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
}

TEST_CASE("A lone sentence gets translation tiles with no distractors", "[sentencetiles]")
{
    FAppSpeechGuard SpeechGuard;

    SentenceTilesController Controller;
    Controller.SetBuildsTranslation(true);
    Controller.Start(MakeApple());

    QStringList Sorted = Controller.GetTiles();
    QStringList Expected = WordsOf("I eat a red apple.");
    Sorted.sort();
    Expected.sort();

    CHECK(Sorted == Expected);
}
