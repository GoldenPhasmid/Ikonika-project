#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "SentenceTrainingController.h"

#include "AppDictionary.h"
#include "AppState.h"
#include "AudioTranscriptionController.h"
#include "FlashcardController.h"
#include "SentenceTilesController.h"
#include "TestAppDatabase.h"
#include "TestAppSpeech.h"
#include "WrittenProductionController.h"

#include "core/FTSentenceCards.h"
#include "core/FTSettings.h"
#include "core/FTWordForms.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <set>
#include <string>
#include <vector>

namespace
{
    struct FAppDictionaryGuard
    {
        ~FAppDictionaryGuard()
        {
            AppDictionary::Get().Close();
        }
    };

    struct FSentenceFixture
    {
        FAppDatabaseGuard DatabaseGuard;
        FAppSpeechGuard SpeechGuard;
        FAppDictionaryGuard DictionaryGuard;

        FSentenceFixture()
        {
            SpeechGuard.Mock.SentenceSpeed = 0.7f;

            FTSettings::Get().SetTargetLanguage("fr");
            FTSettings::Get().SetNativeLanguage("en");

            AppDatabase::Get().GetCardRepository().SetActiveLanguages("fr", "en");

            QSettings().remove("sentences/difficulty");
            QSettings().remove("sentences/count");

            AppState::Get().SentenceCheckpoints.clear();
        }

        ~FSentenceFixture()
        {
            AppState::Get().SentenceCheckpoints.clear();
        }

        bool HasSentenceIndex() const
        {
            return AppDictionary::Get().Initialise() && AppDictionary::Get().GetSentenceTree() != nullptr;
        }
    };

    void StudyWordsOnly(int Count)
    {
        FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

        std::vector<FTCard> Cards;

        for (const FTLearnableWord& Word : AppDictionary::Get().GetRepository().LoadLearnableWords(Count, 0))
        {
            FTCard Card;
            Card.TargetWord = Word.Headword;
            Card.Translation = Word.Translation;
            Card.PartOfSpeech = Word.PartOfSpeech;
            Cards.push_back(std::move(Card));
        }

        REQUIRE(AddTestCards(Cards));

        const std::vector<FTCardHandle> Loaded = CardRepository.LoadCards();

        std::vector<std::string> CardIds;

        for (const FTCardHandle& Card : Loaded)
        {
            CardIds.push_back(Card->Id);
        }

        REQUIRE(CardRepository.PromoteCards(CardIds));
    }

    void TrainVerbTenses(const std::vector<std::string>& Verbs,
                         const std::vector<std::string>& Tenses = ListFormsTenses("fr"))
    {
        FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

        std::vector<FTCard> Cards;
        std::vector<std::string> CardIds;

        for (const std::string& Verb : Verbs)
        {
            for (const std::string& Tense : Tenses)
            {
                FTCard Card;
                Card.Id = "conjugation-fr-" + Verb + "-" + Tense;
                Card.TargetWord = Verb;
                Card.Translation = Verb;

                CardIds.push_back(Card.Id);
                Cards.push_back(std::move(Card));
            }
        }

        REQUIRE(CardRepository.AddFormsCards(Cards));
        REQUIRE(CardRepository.MarkFormsCardsLearned(CardIds, GetTodayDayNumber()));
    }

    void StudyCommonWords(int Count)
    {
        StudyWordsOnly(Count);

        std::vector<std::string> Verbs;

        for (const FTStudiedHeadword& Studied : AppDatabase::Get().GetCardRepository().ListStudiedHeadwords())
        {
            if (Studied.PartOfSpeech == FormsKindVerb)
            {
                Verbs.push_back(Studied.Headword);
            }
        }

        TrainVerbTenses(Verbs);
    }

    void PlaceSentence(SentenceTilesController& Tiles)
    {
        for (const std::string& Word : SplitSentenceWords(Tiles.GetAnswer().toStdString()))
        {
            const QStringList Bank = Tiles.GetTiles();
            const QVariantList Placed = Tiles.GetPlaced();

            for (int Index = 0; Index < Bank.size(); ++Index)
            {
                if (Bank[Index].toStdString() == Word && !Placed.contains(Index))
                {
                    Tiles.placeTile(Index);

                    break;
                }
            }
        }

        Tiles.submit();
    }

    QString FindMissingWord(const QString& Prompt, const QString& Sentence)
    {
        const qsizetype GapStart = Prompt.indexOf("___");
        const qsizetype TailLength = Prompt.size() - GapStart - 3;

        return Sentence.mid(GapStart, Sentence.size() - GapStart - TailLength);
    }
}

TEST_CASE("Sentence training starts on essential words before any word is studied", "[sentencetraining]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    SentenceTrainingController Controller;
    Controller.refresh();

    CHECK(Controller.IsAvailable());
    CHECK(Controller.GetReadyCount() > 0);
    CHECK(Controller.CanStart());

    CHECK(Controller.start("training"));
    CHECK(Controller.HasActiveSession());
    CHECK(Controller.GetActiveController() != nullptr);
}

TEST_CASE("Every difficulty unlocks once it has ten sentences", "[sentencetraining][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    SentenceTrainingController Controller;
    Controller.refresh();

    CHECK(Controller.IsUnlocked());
    CHECK(Controller.GetUnlockedDifficultyMask() == SentenceTrainingController::EveryDifficulty);
}

TEST_CASE("Essential words alone unlock every sentence difficulty", "[sentencetraining][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    SentenceTrainingController Controller;
    Controller.SetDifficultyMask(SentenceTrainingController::HardDifficulty);
    Controller.refresh();

    REQUIRE(Controller.IsUnlocked());
    CHECK(Controller.GetUnlockedDifficultyMask() == SentenceTrainingController::EveryDifficulty);
    CHECK(Controller.GetDifficultyMask() == SentenceTrainingController::HardDifficulty);
    CHECK(Controller.CanStart());
}

TEST_CASE("Studied words make ten balanced sentences ready", "[sentencetraining][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    SentenceTrainingController Controller;
    Controller.SetDifficultyMask(SentenceTrainingController::EasyDifficulty);
    Controller.refresh();

    CHECK(Controller.GetReadyCount() == 10);
    CHECK(Controller.CanStart());

    REQUIRE(Controller.start("training"));

    CHECK(Controller.HasActiveSession());
    CHECK(Controller.GetStage() == 1);
    CHECK(Controller.GetSessionSentenceCount() == 10);

    auto* Flashcard = qobject_cast<FlashcardController*>(Controller.GetActiveController());
    REQUIRE(Flashcard != nullptr);

    const FCardState* Cards = Flashcard->GetSessionCardState();
    REQUIRE(Cards != nullptr);
    REQUIRE(Cards->GetCount() == 10);

    std::set<std::string> CardIds;
    size_t Shortest = 1000;
    size_t Longest = 0;

    for (const FTCardHandle& Card : Cards->GetCards())
    {
        const size_t WordCount = SplitSentenceWords(Card->TargetWord).size();

        CardIds.insert(Card->Id);
        Shortest = std::min(Shortest, WordCount);
        Longest = std::max(Longest, WordCount);

        CHECK(IsSentenceCardId(Card->Id));
        CHECK_FALSE(Card->Translation.empty());
        CHECK(WordCount >= SentenceTrainingController::MinimumSentenceWords);
        CHECK(QString::fromStdString(Card->TargetWord).size()
              <= SentenceTrainingController::MaximumSentenceCharacters);
    }

    CHECK(CardIds.size() == 10);
    CHECK(Longest - Shortest <= 3);

    std::vector<uint32_t> SentenceIds;

    for (const std::string& CardId : CardIds)
    {
        SentenceIds.push_back(static_cast<uint32_t>(std::stoul(CardId.substr(CardId.find(':') + 1))));
    }

    const std::vector<FTTreeSentence> Sentences =
        AppDictionary::Get().GetRepository().LoadTreeSentences(SentenceIds);

    REQUIRE(Sentences.size() == 10);

    for (const FTTreeSentence& Sentence : Sentences)
    {
        INFO(Sentence.Text);
        CHECK(Sentence.Level == 1);
    }
}

TEST_CASE("An unknown sentence exercise does not start", "[sentencetraining][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    SentenceTrainingController Controller;

    CHECK_FALSE(Controller.start("crossword"));
    CHECK_FALSE(Controller.start(""));
    CHECK_FALSE(Controller.HasActiveSession());
}

TEST_CASE("Sentence training opens with a flashcard for every sentence", "[sentencetraining][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    SentenceTrainingController Controller;
    REQUIRE(Controller.start("training"));

    auto* Flashcard = qobject_cast<FlashcardController*>(Controller.GetActiveController());
    REQUIRE(Flashcard != nullptr);

    CHECK(Flashcard->GetMaxSessionCount() == 10);
    CHECK_FALSE(Flashcard->CanGoPrevious());

    std::set<QString> Shown;

    for (int Card = 0; Card < 10; ++Card)
    {
        REQUIRE(Controller.GetStage() == 1);

        const FTCard* Current = Flashcard->GetCurrentCard();
        REQUIRE(Current != nullptr);

        CHECK(IsSentenceCardId(Current->Id));
        CHECK(Flashcard->GetPrompt() == QString::fromStdString(Current->TargetWord));
        CHECK(Flashcard->GetAnswer() == QString::fromStdString(Current->Translation));
        CHECK(Flashcard->GetCurrentSessionCount() == Card + 1);
        CHECK(Fixture.SpeechGuard.Mock.LastText == Flashcard->GetPrompt());
        CHECK(Fixture.SpeechGuard.Mock.LastRateScale == Fixture.SpeechGuard.Mock.SentenceSpeed);

        Shown.insert(Flashcard->GetPrompt());

        Flashcard->next();
    }

    CHECK(Shown.size() == 10);
    CHECK(Controller.GetStage() == 2);
    CHECK(qobject_cast<SentenceTilesController*>(Controller.GetActiveController()) != nullptr);

    Controller.endSession();

    REQUIRE(Controller.start("tiles"));

    CHECK(qobject_cast<SentenceTilesController*>(Controller.GetActiveController()) != nullptr);
}

TEST_CASE("A sentence session runs flashcards, tiles, missing word, then dictation", "[sentencetraining][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    SentenceTrainingController Controller;
    REQUIRE(Controller.start("training"));

    REQUIRE(qobject_cast<FlashcardController*>(Controller.GetActiveController()) != nullptr);

    Controller.completeStage();

    REQUIRE(Controller.GetStage() == 2);
    REQUIRE(qobject_cast<SentenceTilesController*>(Controller.GetActiveController()) != nullptr);

    Controller.completeStage();

    REQUIRE(Controller.GetStage() == 3);

    auto* Gap = qobject_cast<WrittenProductionController*>(Controller.GetActiveController());
    REQUIRE(Gap != nullptr);

    const QString Sentence = Gap->GetAnswer();
    const QString Prompt = Gap->GetPrompt();

    CHECK(Prompt.contains("___"));
    CHECK_FALSE(Gap->ShowsTargetLanguage());
    CHECK_FALSE(Gap->GetAnswerHint().isEmpty());
    CHECK(Gap->CanReplay());
    CHECK(Fixture.SpeechGuard.Mock.LastText == Sentence);
    CHECK(Fixture.SpeechGuard.Mock.LastRateScale == Fixture.SpeechGuard.Mock.SentenceSpeed);

    const int SpokenBeforeReplay = Fixture.SpeechGuard.Mock.SpeakCount;

    Gap->replay();

    CHECK(Fixture.SpeechGuard.Mock.SpeakCount == SpokenBeforeReplay + 1);
    CHECK(Fixture.SpeechGuard.Mock.LastText == Sentence);

    const QString Missing = FindMissingWord(Prompt, Sentence);
    REQUIRE(Missing.size() >= 2);
    CHECK(QString(Prompt).replace("___", Missing) == Sentence);

    Gap->submitAnswer(Missing.toUpper());

    CHECK(Gap->WasLastAnswerCorrect());
    CHECK(Gap->GetLastRevealedAnswer() == Sentence);
    CHECK_FALSE(Gap->GetLastCardTranslation().isEmpty());
    CHECK(Gap->GetLastCardTranslation() != Sentence);

    Gap->advance();
    Gap->submitAnswer("zzzz");

    CHECK_FALSE(Gap->WasLastAnswerCorrect());

    Controller.completeStage();

    REQUIRE(Controller.GetStage() == 4);

    auto* Dictation = qobject_cast<AudioTranscriptionController*>(Controller.GetActiveController());
    REQUIRE(Dictation != nullptr);

    const QString Spoken = Dictation->GetPrompt();
    CHECK(Fixture.SpeechGuard.Mock.LastText == Spoken);
    CHECK(Fixture.SpeechGuard.Mock.LastRateScale == Fixture.SpeechGuard.Mock.SentenceSpeed);

    QStringList Typed;

    for (const std::string& Word : SplitSentenceWords(Spoken.toStdString()))
    {
        Typed.push_back(QString::fromStdString(Word).toLower());
    }

    Dictation->submitAnswer(Typed.join(' '));

    CHECK(Dictation->WasLastAnswerCorrect());
    CHECK(Dictation->GetLastExpectedAnswer() == Spoken);

    Dictation->advance();
    Dictation->submitAnswer(Typed.front());

    CHECK_FALSE(Dictation->WasLastAnswerCorrect());

    Controller.completeStage();

    CHECK(Controller.IsFinished());
    CHECK_FALSE(Controller.HasActiveSession());
    CHECK(Controller.GetSessionSentenceCount() == 10);

    Controller.endSession();

    CHECK(Controller.GetSessionSentenceCount() == 0);
    CHECK(Controller.GetActiveController() == nullptr);
}

TEST_CASE("A missed sentence comes back before the stage ends", "[sentencetraining][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    SentenceTrainingController Controller;
    REQUIRE(Controller.start("training"));

    Controller.completeStage();

    auto* Tiles = qobject_cast<SentenceTilesController*>(Controller.GetActiveController());
    REQUIRE(Tiles != nullptr);

    const QString Missed = Tiles->GetPrompt();

    Tiles->skip();
    Tiles->advance();

    for (int Answer = 0; Answer < 9; ++Answer)
    {
        REQUIRE(Controller.GetStage() == 2);

        Tiles = qobject_cast<SentenceTilesController*>(Controller.GetActiveController());
        REQUIRE(Tiles != nullptr);
        CHECK(Tiles->GetPrompt() != Missed);

        PlaceSentence(*Tiles);
        REQUIRE(Tiles->WasLastAnswerCorrect());

        Tiles->advance();
    }

    REQUIRE(Controller.GetStage() == 2);

    Tiles = qobject_cast<SentenceTilesController*>(Controller.GetActiveController());
    REQUIRE(Tiles != nullptr);
    CHECK(Tiles->GetPrompt() == Missed);

    PlaceSentence(*Tiles);
    REQUIRE(Tiles->WasLastAnswerCorrect());

    Tiles->advance();

    CHECK(Controller.GetStage() == 3);
}

TEST_CASE("A single sentence exercise is a session with one stage", "[sentencetraining][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    SentenceTrainingController Controller;

    REQUIRE(Controller.start("dictation"));
    CHECK(qobject_cast<AudioTranscriptionController*>(Controller.GetActiveController()) != nullptr);

    Controller.completeStage();
    CHECK(Controller.IsFinished());

    REQUIRE(Controller.start("gap"));
    CHECK_FALSE(Controller.IsFinished());
    CHECK(qobject_cast<WrittenProductionController*>(Controller.GetActiveController()) != nullptr);

    REQUIRE(Controller.start("tiles"));
    CHECK(qobject_cast<SentenceTilesController*>(Controller.GetActiveController()) != nullptr);

    Controller.endSession();

    CHECK_FALSE(Controller.HasActiveSession());
    CHECK(Controller.GetStage() == 0);
}

namespace
{
    std::set<int> LevelsOfSession(SentenceTrainingController& Controller)
    {
        const FCardState* Cards = Controller.GetActiveController()->GetSessionCardState();

        std::vector<uint32_t> SentenceIds;

        for (const FTCardHandle& Card : Cards->GetCards())
        {
            SentenceIds.push_back(static_cast<uint32_t>(std::stoul(Card->Id.substr(Card->Id.find(':') + 1))));
        }

        std::set<int> Levels;

        for (const FTTreeSentence& Sentence :
             AppDictionary::Get().GetRepository().LoadTreeSentences(SentenceIds))
        {
            Levels.insert(Sentence.Level);
        }

        return Levels;
    }
}

TEST_CASE("Difficulty maps onto grammar levels", "[sentencetraining]")
{
    using FController = SentenceTrainingController;

    CHECK(FController::MakeGrammarLevelMask(FController::EasyDifficulty) == MakeLevelMask(1));
    CHECK(FController::MakeGrammarLevelMask(FController::NormalDifficulty) == MakeLevelMask(2));
    CHECK(FController::MakeGrammarLevelMask(FController::HardDifficulty)
          == (MakeLevelMask(3) | MakeLevelMask(4)));
    CHECK(FController::MakeGrammarLevelMask(FController::EasyDifficulty | FController::HardDifficulty)
          == (MakeLevelMask(1) | MakeLevelMask(3) | MakeLevelMask(4)));
    CHECK(FController::MakeGrammarLevelMask(0) == 0u);
}

TEST_CASE("A session draws only from the chosen difficulties", "[sentencetraining][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    SentenceTrainingController Controller;

    Controller.SetDifficultyMask(SentenceTrainingController::NormalDifficulty);
    REQUIRE(Controller.CanStart());
    REQUIRE(Controller.start("tiles"));
    CHECK(LevelsOfSession(Controller) == std::set<int>{2});

    Controller.endSession();
    Controller.SetDifficultyMask(SentenceTrainingController::HardDifficulty);
    REQUIRE(Controller.start("tiles"));

    for (const int Level : LevelsOfSession(Controller))
    {
        CHECK(Level >= 3);
    }

    Controller.endSession();
    Controller.SetDifficultyMask(SentenceTrainingController::EasyDifficulty
                                 | SentenceTrainingController::NormalDifficulty);
    REQUIRE(Controller.start("tiles"));

    for (const int Level : LevelsOfSession(Controller))
    {
        CHECK(Level <= 2);
    }
}

TEST_CASE("The difficulty choice is kept and never left empty", "[sentencetraining]")
{
    FSentenceFixture Fixture;

    {
        SentenceTrainingController Controller;
        Controller.SetDifficultyMask(SentenceTrainingController::EasyDifficulty
                                     | SentenceTrainingController::HardDifficulty);

        CHECK(Controller.GetDifficultyMask() == 5);

        Controller.SetDifficultyMask(0);
        CHECK(Controller.GetDifficultyMask() == SentenceTrainingController::EasyDifficulty);

        Controller.SetDifficultyMask(64 | SentenceTrainingController::NormalDifficulty);
        CHECK(Controller.GetDifficultyMask() == SentenceTrainingController::NormalDifficulty);
    }

    SentenceTrainingController Reopened;

    CHECK(Reopened.GetDifficultyMask() == SentenceTrainingController::NormalDifficulty);

    Reopened.SetDifficultyMask(SentenceTrainingController::EasyDifficulty);
}

namespace
{
    std::set<std::string> SessionCardIds(SentenceTrainingController& Controller)
    {
        std::set<std::string> CardIds;

        for (const FTCardHandle& Card : Controller.GetActiveController()->GetSessionCardState()->GetCards())
        {
            CardIds.insert(Card->Id);
        }

        return CardIds;
    }
}

TEST_CASE("A sentence session left halfway resumes where it was", "[sentencetraining][checkpoint][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    QString Prompt;
    QStringList TileBank;
    std::set<std::string> CardIds;

    {
        SentenceTrainingController Controller;
        REQUIRE(Controller.start("training"));

        Controller.completeStage();

        auto* Tiles = qobject_cast<SentenceTilesController*>(Controller.GetActiveController());
        REQUIRE(Tiles != nullptr);

        PlaceSentence(*Tiles);
        REQUIRE(Tiles->WasLastAnswerCorrect());
        Tiles->advance();

        Tiles = qobject_cast<SentenceTilesController*>(Controller.GetActiveController());
        REQUIRE(Tiles != nullptr);

        Tiles->placeTile(0);

        Prompt = Tiles->GetPrompt();
        TileBank = Tiles->GetTiles();
        CardIds = SessionCardIds(Controller);

        CHECK(Controller.GetSavedExercises() == QStringList{"training"});
    }

    SentenceTrainingController Reopened;

    CHECK_FALSE(Reopened.HasActiveSession());
    CHECK(Reopened.GetSavedExercises() == QStringList{"training"});

    const int SpokenBeforeResume = Fixture.SpeechGuard.Mock.SpeakCount;

    REQUIRE(Reopened.start("training"));

    CHECK(Reopened.GetStage() == 2);
    CHECK(Reopened.GetSessionSentenceCount() == 10);
    CHECK(SessionCardIds(Reopened) == CardIds);
    CHECK(Fixture.SpeechGuard.Mock.SpeakCount == SpokenBeforeResume);

    auto* Tiles = qobject_cast<SentenceTilesController*>(Reopened.GetActiveController());
    REQUIRE(Tiles != nullptr);

    CHECK(Tiles->GetPrompt() == Prompt);
    CHECK(Tiles->GetTiles() == TileBank);
    CHECK(Tiles->GetPlaced() == QVariantList{0});

    Tiles->removeTile(0);
    PlaceSentence(*Tiles);

    CHECK(Tiles->WasLastAnswerCorrect());
}

TEST_CASE("A resumed sentence session keeps its stage and the revealed answer",
          "[sentencetraining][checkpoint][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    QString Revealed;
    QString Translation;
    QString Missing;

    {
        SentenceTrainingController Controller;
        REQUIRE(Controller.start("training"));

        Controller.completeStage();
        Controller.completeStage();
        REQUIRE(Controller.GetStage() == 3);

        auto* Gap = qobject_cast<WrittenProductionController*>(Controller.GetActiveController());
        REQUIRE(Gap != nullptr);

        Missing = FindMissingWord(Gap->GetPrompt(), Gap->GetAnswer());

        Gap->submitAnswer("zzzz");

        Revealed = Gap->GetLastRevealedAnswer();
        Translation = Gap->GetLastCardTranslation();

        REQUIRE_FALSE(Translation.isEmpty());
    }

    SentenceTrainingController Reopened;
    REQUIRE(Reopened.start("training"));

    CHECK(Reopened.GetStage() == 3);

    auto* Gap = qobject_cast<WrittenProductionController*>(Reopened.GetActiveController());
    REQUIRE(Gap != nullptr);

    CHECK(Gap->IsAnswered());
    CHECK_FALSE(Gap->WasLastAnswerCorrect());
    CHECK(Gap->GetLastRevealedAnswer() == Revealed);
    CHECK(Gap->GetLastCardTranslation() == Translation);
    CHECK(Gap->IsRetypePending());

    Gap->advance();

    CHECK(Gap->IsAnswered());

    Gap->submitAnswer(Missing);
    Gap->advance();

    CHECK_FALSE(Gap->IsAnswered());
    CHECK(Gap->GetPrompt().contains("___"));
}

TEST_CASE("The sentence count sets the session length and is kept", "[sentencetraining][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    {
        SentenceTrainingController Controller;

        CHECK(Controller.GetSentenceCount() == SentenceTrainingController::DefaultSentenceCount);

        Controller.SetSentenceCount(5);
        Controller.refresh();

        CHECK(Controller.GetSentenceCount() == 5);
        CHECK(Controller.GetReadyCount() == 5);
        CHECK(Controller.CanStart());

        REQUIRE(Controller.start("training"));

        CHECK(Controller.GetSessionSentenceCount() == 5);

        auto* Flashcard = qobject_cast<FlashcardController*>(Controller.GetActiveController());
        REQUIRE(Flashcard != nullptr);
        CHECK(Flashcard->GetMaxSessionCount() == 5);

        for (int Card = 0; Card < 5; ++Card)
        {
            Flashcard->next();
        }

        REQUIRE(Controller.GetStage() == 2);

        auto* Tiles = qobject_cast<SentenceTilesController*>(Controller.GetActiveController());
        REQUIRE(Tiles != nullptr);
        CHECK(Tiles->GetMaxSessionCount() == 5);

        Controller.endSession();

        Controller.SetSentenceCount(20);

        CHECK(Controller.GetReadyCount() == 20);

        REQUIRE(Controller.start("tiles"));

        CHECK(Controller.GetSessionSentenceCount() == 20);
        CHECK(SessionCardIds(Controller).size() == 20);

        Controller.endSession();
    }

    SentenceTrainingController Reopened;

    CHECK(Reopened.GetSentenceCount() == 20);

    Reopened.SetSentenceCount(100);
    CHECK(Reopened.GetSentenceCount() == SentenceTrainingController::MaximumSentenceCount);

    Reopened.SetSentenceCount(1);
    CHECK(Reopened.GetSentenceCount() == SentenceTrainingController::MinimumSentenceCount);

    Reopened.SetSentenceCount(12);
    CHECK(Reopened.GetSentenceCount() == 10);

    Reopened.SetSentenceCount(13);
    CHECK(Reopened.GetSentenceCount() == 15);
}

TEST_CASE("A saved session keeps its own length when the count changes", "[sentencetraining][checkpoint][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    std::set<std::string> CardIds;

    {
        SentenceTrainingController Controller;
        REQUIRE(Controller.start("tiles"));

        CardIds = SessionCardIds(Controller);
        REQUIRE(CardIds.size() == 10);
    }

    SentenceTrainingController Reopened;
    Reopened.SetSentenceCount(5);

    REQUIRE(Reopened.start("tiles"));

    CHECK(Reopened.GetSessionSentenceCount() == 10);
    CHECK(SessionCardIds(Reopened) == CardIds);
}

TEST_CASE("Sentence flashcards left halfway resume on the same card", "[sentencetraining][checkpoint][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    QStringList Seen;

    {
        SentenceTrainingController Controller;
        REQUIRE(Controller.start("training"));

        auto* Flashcard = qobject_cast<FlashcardController*>(Controller.GetActiveController());
        REQUIRE(Flashcard != nullptr);

        Seen.push_back(Flashcard->GetPrompt());

        for (int Step = 0; Step < 3; ++Step)
        {
            Flashcard->next();
            Seen.push_back(Flashcard->GetPrompt());
        }
    }

    SentenceTrainingController Reopened;

    const int SpokenBeforeResume = Fixture.SpeechGuard.Mock.SpeakCount;

    REQUIRE(Reopened.start("training"));

    CHECK(Reopened.GetStage() == 1);
    CHECK(Fixture.SpeechGuard.Mock.SpeakCount == SpokenBeforeResume);

    auto* Flashcard = qobject_cast<FlashcardController*>(Reopened.GetActiveController());
    REQUIRE(Flashcard != nullptr);

    CHECK(Flashcard->GetPrompt() == Seen[3]);
    CHECK(Flashcard->GetCurrentSessionCount() == 4);
    CHECK(Flashcard->CanGoPrevious());

    Flashcard->previous();

    CHECK(Flashcard->GetPrompt() == Seen[2]);

    Flashcard->next();

    for (int Step = 0; Step < 6; ++Step)
    {
        Flashcard->next();

        CHECK_FALSE(Seen.contains(Flashcard->GetPrompt()));

        Seen.push_back(Flashcard->GetPrompt());
    }

    CHECK(Reopened.GetStage() == 1);

    Flashcard->next();

    CHECK(Reopened.GetStage() == 2);
}

TEST_CASE("Each sentence exercise keeps its own saved session", "[sentencetraining][checkpoint][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    SentenceTrainingController Controller;
    REQUIRE(Controller.start("tiles"));

    auto* Tiles = qobject_cast<SentenceTilesController*>(Controller.GetActiveController());
    REQUIRE(Tiles != nullptr);

    Tiles->skip();
    Tiles->advance();

    const QString TilesPrompt = Tiles->GetPrompt();
    const std::set<std::string> TilesCards = SessionCardIds(Controller);

    REQUIRE(Controller.start("gap"));
    REQUIRE(qobject_cast<WrittenProductionController*>(Controller.GetActiveController()) != nullptr);

    CHECK(Controller.GetSavedExercises() == QStringList{"gap", "tiles"});

    REQUIRE(Controller.start("tiles"));

    Tiles = qobject_cast<SentenceTilesController*>(Controller.GetActiveController());
    REQUIRE(Tiles != nullptr);

    CHECK(Tiles->GetPrompt() == TilesPrompt);
    CHECK(SessionCardIds(Controller) == TilesCards);

    Controller.endSession();

    CHECK(Controller.GetSavedExercises() == QStringList{"gap"});

    Controller.forgetSessions();

    CHECK(Controller.GetSavedExercises().isEmpty());
    CHECK_FALSE(Controller.HasActiveSession());
}

TEST_CASE("Finishing a sentence session clears its save", "[sentencetraining][checkpoint][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    SentenceTrainingController Controller;
    REQUIRE(Controller.start("dictation"));

    CHECK(Controller.GetSavedExercises() == QStringList{"dictation"});

    Controller.completeStage();

    CHECK(Controller.IsFinished());
    CHECK(Controller.GetSavedExercises().isEmpty());

    REQUIRE(Controller.start("dictation"));

    CHECK_FALSE(Controller.IsFinished());
    CHECK(Controller.GetStage() == 1);
}

TEST_CASE("A saved session whose sentences changed is dropped", "[sentencetraining][checkpoint][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    {
        SentenceTrainingController Controller;
        REQUIRE(Controller.start("tiles"));
    }

    QJsonObject Saved = QJsonDocument::fromJson(AppState::Get().SentenceCheckpoints["tiles"]).object();
    QJsonArray SavedSentences = Saved["sentences"].toArray();
    REQUIRE(SavedSentences.size() == 10);

    QJsonObject First = SavedSentences[0].toObject();
    First["text"] = "Cette phrase a changé.";
    SavedSentences[0] = First;
    Saved["sentences"] = SavedSentences;

    AppState::Get().SentenceCheckpoints["tiles"] = QJsonDocument(Saved).toJson(QJsonDocument::Compact);

    SentenceTrainingController Reopened;
    REQUIRE(Reopened.start("tiles"));

    CHECK(Reopened.GetStage() == 1);
    CHECK_FALSE(AppState::Get().SentenceCheckpoints["tiles"].contains("Cette phrase a chang"));

    AppState::Get().SentenceCheckpoints["tiles"] = QByteArray("not json");

    SentenceTrainingController Broken;

    CHECK(Broken.start("tiles"));
    CHECK(Broken.HasActiveSession());
}

TEST_CASE("A saved session with another number of stages is dropped", "[sentencetraining][checkpoint][slow]")
{
    FSentenceFixture Fixture;

    if (!Fixture.HasSentenceIndex())
    {
        SKIP("The built dictionary has no sentence index");
    }

    StudyCommonWords(2000);

    {
        SentenceTrainingController Controller;
        REQUIRE(Controller.start("training"));

        Controller.completeStage();

        auto* Tiles = qobject_cast<SentenceTilesController*>(Controller.GetActiveController());
        REQUIRE(Tiles != nullptr);

        Tiles->placeTile(0);
    }

    QJsonObject Saved = QJsonDocument::fromJson(AppState::Get().SentenceCheckpoints["training"]).object();

    REQUIRE(Saved["stageCount"].toInt() == 4);
    REQUIRE(Saved["stageIndex"].toInt() == 1);

    Saved["stageCount"] = 3;

    AppState::Get().SentenceCheckpoints["training"] = QJsonDocument(Saved).toJson(QJsonDocument::Compact);

    SentenceTrainingController Reopened;
    REQUIRE(Reopened.start("training"));

    CHECK(Reopened.GetStage() == 1);
    CHECK(qobject_cast<FlashcardController*>(Reopened.GetActiveController()) != nullptr);

    const QJsonObject Fresh =
        QJsonDocument::fromJson(AppState::Get().SentenceCheckpoints["training"]).object();

    CHECK(Fresh["stageCount"].toInt() == 4);
}
