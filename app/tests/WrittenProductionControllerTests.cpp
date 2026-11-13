#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "GapProductionController.h"
#include "WrittenProductionController.h"

#include "ExerciseFlowStages.h"
#include "SessionSentenceStage.h"
#include "SessionSettings.h"
#include "SessionSettingsGuard.h"
#include "TestAppSpeech.h"

#include "core/FTSentenceCards.h"
#include "core/FTSettings.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    FTCard MakeCard(std::string TargetWord, std::string Translation)
    {
        FTCard Result;
        Result.Id = TargetWord;
        Result.TargetWord = std::move(TargetWord);
        Result.Translation = std::move(Translation);

        return Result;
    }

    std::vector<FTCardHandle> MakeCards(int Count)
    {
        std::vector<FTCardHandle> Cards;

        for (int Index = 0; Index < Count; ++Index)
        {
            Cards.push_back(MakeCardHandle(
                MakeCard("word" + std::to_string(Index), "translation" + std::to_string(Index))));
        }

        return Cards;
    }

    FTSessionStartParams MakeParams(std::vector<FTCardHandle> Cards)
    {
        FTSessionStartParams Params{};
        Params.CardState = std::make_shared<FCardState>(std::move(Cards));
        Params.Title = "Test Session";
        Params.TargetLanguage = "fr";
        Params.FixedLength = Params.CardState->GetCount();
        Params.Selector = std::make_shared<FEachOnceSelector>(Params.CardState->GetCount(), true);

        return Params;
    }

    struct FMissingArticleGuard
    {
        explicit FMissingArticleGuard(bool bAllow)
            : bPrevious(FTSettings::Get().GetLeniencyRules().bAllowMissingArticle)
        {
            FTSettings::Get().SetAllowMissingArticle(bAllow);
        }

        ~FMissingArticleGuard()
        {
            FTSettings::Get().SetAllowMissingArticle(bPrevious);
        }

        bool bPrevious = false;
    };

    bool IsBareWordAccepted(FTCard Card, const std::string& Language, const QString& Typed)
    {
        std::vector<FTCardHandle> Cards;
        Cards.push_back(MakeCardHandle(std::move(Card)));

        FTSessionStartParams Params = MakeParams(std::move(Cards));
        Params.TargetLanguage = Language;

        WrittenProductionController Controller;
        Controller.SetAskWord(false);
        Controller.Start(Params);
        Controller.submitAnswer(Typed);

        return Controller.WasLastAnswerCorrect();
    }

    FTCard MakeGenderedCard(std::string TargetWord, int Gender)
    {
        FTCard Result = MakeCard(std::move(TargetWord), "meaning");
        Result.Gender = Gender;
        Result.PartOfSpeech = "noun";

        return Result;
    }
}

TEST_CASE("A fresh session defaults to asking for the word", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(Controller.GetDirectionLabel() == "FR → EN");
    CHECK(Controller.GetPrompt() == "word0");
    CHECK(Controller.GetAnswer() == "translation0");
}

TEST_CASE("SetAskWord(false) swaps the prompt and answer directions", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.SetAskWord(false);
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(Controller.GetDirectionLabel() == "EN → FR");
    CHECK(Controller.GetPrompt() == "translation0");
    CHECK(Controller.GetAnswer() == "word0");
}

TEST_CASE("Submitting the exact translation records a correct answer", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.submitAnswer(Controller.GetAnswer());

    CHECK(Controller.IsAnswered());
    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Submitting is case-insensitive", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.submitAnswer(Controller.GetAnswer().toUpper());

    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Submitting the wrong text records an incorrect answer", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.submitAnswer("nope");

    CHECK(Controller.IsAnswered());
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Submitting blank text is not an answer", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.submitAnswer("");
    Controller.submitAnswer("   ");

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetResultMarks().isEmpty());
}

TEST_CASE("Submitting again after already answered is a no-op", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    const QString CorrectAnswer = Controller.GetAnswer();

    Controller.submitAnswer(CorrectAnswer);
    Controller.submitAnswer("nope");

    CHECK(Controller.GetLastGivenAnswer() == CorrectAnswer);
    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Skipping records an incorrect answer with no given answer", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.skip();

    CHECK(Controller.IsAnswered());
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
    CHECK(Controller.GetLastGivenAnswer().isEmpty());
}

TEST_CASE("Advancing before answering is a no-op", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int CountBefore = Controller.GetCurrentSessionCount();

    Controller.advance();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == CountBefore);
}

TEST_CASE("Advancing after answering moves to the next card", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int CountBefore = Controller.GetCurrentSessionCount();
    const QString FirstPrompt = Controller.GetPrompt();

    Controller.submitAnswer(Controller.GetAnswer());
    Controller.advance();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == CountBefore + 1);
    CHECK(Controller.GetPrompt() != FirstPrompt);
}

TEST_CASE("A wrong answer holds the card until the answer is typed right, and the retype is not scored",
          "[writtenproduction][retype]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const int CountBefore = Controller.GetCurrentSessionCount();
    const QString Answer = Controller.GetAnswer();

    Controller.submitAnswer("nope");

    CHECK(Controller.IsRetypePending());
    CHECK(Controller.GetResultMarks().size() == 1);

    Controller.advance();

    CHECK(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == CountBefore);

    Controller.submitAnswer("still wrong");

    CHECK(Controller.IsRetypePending());
    CHECK(Controller.GetResultMarks().size() == 1);

    Controller.submitAnswer(Answer);

    CHECK_FALSE(Controller.IsRetypePending());
    CHECK_FALSE(Controller.WasLastAnswerCorrect());
    REQUIRE(Controller.GetResultMarks().size() == 1);
    CHECK(Controller.GetResultMarks().front().toInt() == 0);

    Controller.advance();

    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == CountBefore + 1);
}

TEST_CASE("Don't know also asks for the answer to be typed", "[writtenproduction][retype]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    const QString Answer = Controller.GetAnswer();

    Controller.skip();

    CHECK(Controller.IsRetypePending());

    Controller.advance();

    CHECK(Controller.IsAnswered());

    Controller.submitAnswer(Answer);
    Controller.advance();

    CHECK_FALSE(Controller.IsAnswered());
}

TEST_CASE("A right answer needs no retype", "[writtenproduction][retype]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.submitAnswer(Controller.GetAnswer());

    CHECK_FALSE(Controller.IsRetypePending());
}

TEST_CASE("A pending retype survives a checkpoint", "[writtenproduction][retype]")
{
    FAppSpeechGuard SpeechGuard;

    const FTSessionStartParams Params = MakeParams(MakeCards(5));

    WrittenProductionController Controller;
    Controller.Start(Params);
    Controller.submitAnswer("nope");

    const std::optional<FExerciseCheckpoint> Checkpoint =
        ExerciseControllerBase::CheckpointFromJson(ExerciseControllerBase::CheckpointToJson(*Controller.TakeSession()));

    WrittenProductionController Restored;
    REQUIRE(Restored.RestoreSession(Params, Checkpoint));

    CHECK(Restored.IsRetypePending());

    Restored.advance();

    CHECK(Restored.IsAnswered());
}

TEST_CASE("Speaks the target word up front when asking for the word", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetPrompt());
}

TEST_CASE("Does not speak again when submitting while asking for the word", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(1)));

    REQUIRE(SpeechGuard.Mock.SpeakCount == 1);

    Controller.submitAnswer(Controller.GetAnswer());

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
}

TEST_CASE("Speaks the target word only after submitting when asking for the translation", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.SetAskWord(false);
    Controller.Start(MakeParams(MakeCards(1)));

    CHECK(SpeechGuard.Mock.SpeakCount == 0);

    Controller.submitAnswer(Controller.GetAnswer());

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetAnswer());
}

TEST_CASE("Continuing to the next card speaks its prompt when asking for the word", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.Start(MakeParams(MakeCards(5)));

    Controller.submitAnswer(Controller.GetAnswer());
    const int SpeakCountAfterAnswer = SpeechGuard.Mock.SpeakCount;

    Controller.advance();

    CHECK(SpeechGuard.Mock.SpeakCount == SpeakCountAfterAnswer + 1);
    CHECK(SpeechGuard.Mock.LastText == Controller.GetPrompt());
}

TEST_CASE("Slow speech says nothing while a translation prompt is hidden", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    Controller.SetAskWord(false);
    Controller.Start(MakeParams(MakeCards(1)));

    Controller.speakSlowly();
    Controller.replay();

    CHECK(SpeechGuard.Mock.SpeakCount == 0);
    CHECK_FALSE(Controller.CanReplay());

    Controller.submitAnswer(Controller.GetAnswer());
    Controller.speakSlowly();

    CHECK(SpeechGuard.Mock.SpeakCount == 2);
    CHECK(SpeechGuard.Mock.LastText == "word0");
    CHECK(SpeechGuard.Mock.LastRateScale == AppSpeech::SlowRateScale);
}

TEST_CASE("The sentence gap speaks the whole sentence while hidden and nothing once revealed", "[writtenproduction][gap]")
{
    FAppSpeechGuard SpeechGuard;
    SpeechGuard.Mock.SentenceSpeed = 0.8f;

    FTTreeSentence Sentence;
    Sentence.Text = "Je mange une pomme.";
    Sentence.Translation = "I eat an apple.";
    Sentence.GapStart = 3;
    Sentence.GapLength = 5;

    const auto Sentences = std::make_shared<const FGapSentences>(FGapSentences{{"phrase", Sentence}});

    SentenceGapController Controller;
    Controller.SetSentenceLookup(MakeGapSentenceLookup(Sentences));
    Controller.Start(MakeParams({MakeCardHandle(MakeCard("phrase", "I eat an apple."))}));

    CHECK(Controller.GetPrompt() == "Je ___ une pomme.");
    CHECK(Controller.CanReplay());
    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == "Je mange une pomme.");
    CHECK(SpeechGuard.Mock.LastRateScale == 0.8f);

    Controller.replay();

    CHECK(SpeechGuard.Mock.SpeakCount == 2);
    CHECK(SpeechGuard.Mock.LastText == "Je mange une pomme.");

    Controller.speakSlowly();

    CHECK(SpeechGuard.Mock.SpeakCount == 3);
    CHECK(SpeechGuard.Mock.LastText == "Je mange une pomme.");
    CHECK(SpeechGuard.Mock.LastRateScale == 0.8f * AppSpeech::SlowRateScale);

    Controller.submitAnswer("Mange");

    CHECK(Controller.WasLastAnswerCorrect());
    CHECK_FALSE(Controller.CanReplay());

    Controller.replay();
    Controller.speakSlowly();

    CHECK(SpeechGuard.Mock.SpeakCount == 3);
}

TEST_CASE("The forms gap stays silent while hidden and speaks the whole sentence once revealed",
          "[writtenproduction][gap]")
{
    FAppSpeechGuard SpeechGuard;
    SpeechGuard.Mock.SentenceSpeed = 0.8f;

    FTTreeSentence Sentence;
    Sentence.Text = "Tu veux du pain.";
    Sentence.Translation = "You want bread.";
    Sentence.GapStart = 3;
    Sentence.GapLength = 4;

    const auto Sentences = std::make_shared<const FGapSentences>(FGapSentences{{"veux", Sentence}});

    FormGapController Controller;
    Controller.SetSentenceLookup(MakeGapSentenceLookup(Sentences));
    Controller.Start(MakeParams({MakeCardHandle(MakeCard("veux", "want"))}));

    CHECK(Controller.GetPrompt() == "Tu ___ du pain.");
    CHECK_FALSE(Controller.CanReplay());

    Controller.replay();
    Controller.speakSlowly();

    CHECK(SpeechGuard.Mock.SpeakCount == 0);

    Controller.submitAnswer("veux");

    CHECK(Controller.WasLastAnswerCorrect());
    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == "Tu veux du pain.");
    CHECK(SpeechGuard.Mock.LastRateScale == 0.8f);

    Controller.replay();

    CHECK(SpeechGuard.Mock.SpeakCount == 2);
    CHECK(SpeechGuard.Mock.LastText == "Tu veux du pain.");
}

TEST_CASE("Typing the translation forgives a one-letter typo and the opening words", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    FTCard Card = MakeCard("regarder", "to look at");
    std::vector<FTCardHandle> Cards;
    Cards.push_back(MakeCardHandle(std::move(Card)));

    WrittenProductionController Controller;
    Controller.Start(MakeParams(std::move(Cards)));

    Controller.submitAnswer("look");

    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Typing a typo in the translation counts as correct", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    FTCard Card = MakeCard("regarder", "look");
    std::vector<FTCardHandle> Cards;
    Cards.push_back(MakeCardHandle(std::move(Card)));

    WrittenProductionController Controller;
    Controller.Start(MakeParams(std::move(Cards)));

    Controller.submitAnswer("louk");

    CHECK(Controller.WasLastAnswerCorrect());
}

TEST_CASE("Typing the target word stays exact", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;

    FTCard Card = MakeCard("regarder", "look");
    std::vector<FTCardHandle> Cards;
    Cards.push_back(MakeCardHandle(std::move(Card)));

    WrittenProductionController Controller;
    Controller.SetAskWord(false);
    Controller.Start(MakeParams(std::move(Cards)));

    Controller.submitAnswer("regardre");

    CHECK_FALSE(Controller.WasLastAnswerCorrect());
}

TEST_CASE("The mastery stage gap exercise speaks the sentence without its word, then the word",
          "[writtenproduction][gap]")
{
    FAppSpeechGuard SpeechGuard;
    SpeechGuard.Mock.SentenceSpeed = 0.8f;
    FSessionSettingsGuard SettingsGuard;
    SessionSettings::Review().SetGapExercise(true);

    FTCard Card = MakeCard("train", "train");
    Card.PartOfSpeech = "noun";

    FTTreeSentence Sentence;
    Sentence.Text = "Il descend du train.";
    Sentence.Translation = "He gets off the train.";
    REQUIRE(PlaceGap(Sentence, {"train"}, true));

    FTSessionStartParams Params = MakeParams({MakeCardHandle(Card)});
    Params.NativeLanguage = "en";

    QObject Parent;
    const std::unique_ptr<FMasteryStage> Stage =
        FMasteryStage::Create(&Parent, Params, SessionSettings::Review(), FGapSentences{{Card.Id, Sentence}});

    CHECK(Stage->GetGapSentences().size() == 1);

    auto* Gap = qobject_cast<WordGapController*>(Stage->GetControllers()[2]);
    REQUIRE(Gap != nullptr);

    Gap->Start(Params);

    CHECK(Gap->GetPrompt() == "Il descend du ___.");
    CHECK(Gap->GetPromptHeading() == "He gets off the train.");
    CHECK(Gap->IsSentencePrompt());
    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == "Il descend du ….");
    CHECK(SpeechGuard.Mock.LastRateScale == 0.8f);

    Gap->speakSlowly();

    CHECK(SpeechGuard.Mock.SpeakCount == 2);
    CHECK(SpeechGuard.Mock.LastText == "Il descend du ….");
    CHECK(SpeechGuard.Mock.LastRateScale == 0.8f * AppSpeech::SlowRateScale);

    Gap->submitAnswer("train");

    CHECK(Gap->WasLastAnswerCorrect());
    CHECK(Gap->GetLastRevealedAnswer() == "Il descend du train.");
    CHECK(SpeechGuard.Mock.SpeakCount == 3);
    CHECK(SpeechGuard.Mock.LastText == "train");
    CHECK(SpeechGuard.Mock.LastRateScale == 1.0f);

    Gap->replay();

    CHECK(SpeechGuard.Mock.SpeakCount == 4);
    CHECK(SpeechGuard.Mock.LastText == "train");
}

TEST_CASE("The mastery stage gap exercise speaks only the missing word when the answer is skipped",
          "[writtenproduction][gap]")
{
    FAppSpeechGuard SpeechGuard;
    FSessionSettingsGuard SettingsGuard;
    SessionSettings::Review().SetGapExercise(true);

    FTCard Card = MakeCard("train", "train");
    Card.PartOfSpeech = "noun";

    FTTreeSentence Sentence;
    Sentence.Text = "Il descend du train.";
    Sentence.Translation = "He gets off the train.";
    REQUIRE(PlaceGap(Sentence, {"train"}, true));

    FTSessionStartParams Params = MakeParams({MakeCardHandle(Card)});
    Params.NativeLanguage = "en";

    QObject Parent;
    const std::unique_ptr<FMasteryStage> Stage =
        FMasteryStage::Create(&Parent, Params, SessionSettings::Review(), FGapSentences{{Card.Id, Sentence}});

    auto* Gap = qobject_cast<WordGapController*>(Stage->GetControllers()[2]);
    REQUIRE(Gap != nullptr);

    Gap->Start(Params);
    Gap->skip();

    CHECK(SpeechGuard.Mock.SpeakCount == 2);
    CHECK(SpeechGuard.Mock.LastText == "train");
}

TEST_CASE("Auto-speak off silences the gap in both states but replay still speaks", "[writtenproduction][gap]")
{
    FAppSpeechGuard SpeechGuard;

    const bool bPreviousAutoSpeak = FTSettings::Get().GetSpeechRules().bAutoSpeak;
    FTSettings::Get().SetAutoSpeak(false);

    FTTreeSentence Sentence;
    Sentence.Text = "Il descend du train.";
    REQUIRE(PlaceGap(Sentence, {"train"}, true));

    const auto Sentences = std::make_shared<const FGapSentences>(FGapSentences{{"train", Sentence}});

    WordGapController Controller;
    Controller.SetSentenceLookup(MakeGapSentenceLookup(Sentences));
    Controller.Start(MakeParams({MakeCardHandle(MakeCard("train", "train"))}));

    CHECK(SpeechGuard.Mock.SpeakCount == 0);

    Controller.replay();

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == "Il descend du ….");

    Controller.submitAnswer("train");

    CHECK(SpeechGuard.Mock.SpeakCount == 1);

    Controller.replay();

    CHECK(SpeechGuard.Mock.SpeakCount == 2);
    CHECK(SpeechGuard.Mock.LastText == "train");

    FTSettings::Get().SetAutoSpeak(bPreviousAutoSpeak);
}

TEST_CASE("Gap sentences survive a round trip through the checkpoint JSON", "[writtenproduction][gap][checkpoint]")
{
    FTTreeSentence Sentence;
    Sentence.Id = 7;
    Sentence.Text = "Il descend du train.";
    Sentence.Translation = "He gets off the train.";
    Sentence.GapStart = 14;
    Sentence.GapLength = 5;
    Sentence.Level = 2;

    const FGapSentences Restored = GapSentencesFromJson(GapSentencesToJson(FGapSentences{{"card", Sentence}}));

    REQUIRE(Restored.size() == 1);

    const FTTreeSentence& Saved = Restored.at("card");
    CHECK(Saved.Id == 7);
    CHECK(Saved.Text == Sentence.Text);
    CHECK(Saved.Translation == Sentence.Translation);
    CHECK(Saved.GapStart == 14);
    CHECK(Saved.GapLength == 5);
    CHECK(Saved.Level == 2);
}

TEST_CASE("A gendered word typed without its article is accepted in every language when allowed",
          "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;
    FMissingArticleGuard ArticleGuard(true);

    CHECK(IsBareWordAccepted(MakeGenderedCard("chien", GenderMasculine), "fr", "chien"));
    CHECK(IsBareWordAccepted(MakeGenderedCard("perro", GenderMasculine), "es", "perro"));
    CHECK(IsBareWordAccepted(MakeGenderedCard("gato", GenderMasculine), "pt", "gato"));
    CHECK(IsBareWordAccepted(MakeGenderedCard("casa", GenderFeminine), "pt", "casa"));
    CHECK(IsBareWordAccepted(MakeGenderedCard("libro", GenderMasculine), "it", "libro"));
    CHECK(IsBareWordAccepted(MakeGenderedCard("studente", GenderMasculine), "it", "studente"));
}

TEST_CASE("A gendered word typed without its article is wrong when not allowed", "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;
    FMissingArticleGuard ArticleGuard(false);

    CHECK_FALSE(IsBareWordAccepted(MakeGenderedCard("perro", GenderMasculine), "es", "perro"));
    CHECK(IsBareWordAccepted(MakeGenderedCard("perro", GenderMasculine), "es", "el perro"));
}

TEST_CASE("A phrase keeps its leading article word even when a missing article is allowed",
          "[writtenproduction]")
{
    FAppSpeechGuard SpeechGuard;
    FMissingArticleGuard ArticleGuard(true);

    CHECK_FALSE(IsBareWordAccepted(MakeCard("lo siento", "I'm sorry"), "es", "siento"));
    CHECK_FALSE(IsBareWordAccepted(MakeCard("o que", "what"), "pt", "que"));
    CHECK_FALSE(IsBareWordAccepted(MakeCard("la plupart", "most"), "fr", "plupart"));
}
