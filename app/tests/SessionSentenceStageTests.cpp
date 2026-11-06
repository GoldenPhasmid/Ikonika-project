#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "SessionSentenceStage.h"

#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppState.h"
#include "ExerciseFlowStages.h"
#include "IntensiveLearningController.h"
#include "RepetitionController.h"
#include "SentenceTilesController.h"
#include "SessionSettings.h"
#include "SessionSettingsGuard.h"
#include "TestAppDatabase.h"
#include "TestAppSpeech.h"

#include "core/FTCardMeaning.h"
#include "core/FTCardStore.h"
#include "core/FTSchema.h"
#include "core/FTSentenceCards.h"
#include "core/FTSettings.h"
#include "core/sessions/FCardState.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSettings>
#include <QVariantList>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace
{
    constexpr int AnswerLimit = 200;

    struct FSentenceStageTestBed
    {
        FAppDatabaseGuard DatabaseGuard;
        FAppSpeechGuard SpeechGuard;
        FSessionSettingsGuard SettingsGuard;

        FSentenceStageTestBed()
        {
            FTSettings::Get().SetTargetLanguage("fr");
            FTSettings::Get().SetNativeLanguage("en");

            AppDatabase::Get().GetCardRepository().SetActiveLanguages("fr", "en");

            REQUIRE(AppDictionary::Get().Initialise());
        }

        ~FSentenceStageTestBed()
        {
            AppDictionary::Get().Close();
        }
    };

    FTCard MakeCard(std::string Id, std::string Word, std::string PartOfSpeech, std::string Translation)
    {
        FTCard Card;
        Card.Id = std::move(Id);
        Card.TargetWord = std::move(Word);
        Card.PartOfSpeech = std::move(PartOfSpeech);
        Card.Translation = std::move(Translation);

        return Card;
    }

    FTSessionStartParams MakeParams(std::vector<FTCard> Cards)
    {
        std::vector<FTCardHandle> Handles;

        for (FTCard& Card : Cards)
        {
            Handles.push_back(MakeCardHandle(std::move(Card)));
        }

        FTSessionStartParams Params{};
        Params.CardState = std::make_shared<FCardState>(std::move(Handles));
        Params.TargetLanguage = "fr";
        Params.NativeLanguage = "en";

        return Params;
    }

    FTCard MakeMangerWithMeaning()
    {
        FTCard Manger = MakeCard("manger", "manger", "verb", "to eat");
        Manger.MeaningGist = MatchMeaningGist(Manger, AppDictionary::Get().GetRepository().LoadMeanings("manger", "verb"));

        return Manger;
    }

    std::string FindMeaningExample(const FTCard& Card)
    {
        const std::vector<FTDictionarySense> Meanings
            = AppDictionary::Get().GetRepository().LoadMeanings(Card.TargetWord, Card.PartOfSpeech);
        const FTDictionarySense* Meaning = FindCardMeaning(Card, Meanings);

        return Meaning != nullptr ? Meaning->Example : std::string{};
    }

    std::unique_ptr<FMasteryStage> BuildMasteryStage(QObject& Parent, std::vector<FTCard> Cards)
    {
        return FMasteryStage::Create(&Parent, MakeParams(std::move(Cards)), SessionSettings::Training());
    }

    SentenceTilesController* GetPhaseTiles(FMasteryStage& Stage, int Phase)
    {
        return qobject_cast<SentenceTilesController*>(Stage.GetPhaseLoop(Phase).GetControllers().front());
    }

    void EnterPhase(FMasteryStage& Stage, int Phase)
    {
        Stage.Start();

        while (Stage.GetPhase() < Phase)
        {
            REQUIRE(Stage.SkipPhase());
        }
    }

    void AnswerTranslation(SentenceTilesController& Native)
    {
        const QStringList Tiles = Native.GetTiles();
        QSet<int> Used;

        for (const std::string& Word : SplitSentenceWords(Native.GetPrompt().toStdString()))
        {
            for (int Index = 0; Index < Tiles.size(); ++Index)
            {
                if (!Used.contains(Index) && Tiles[Index] == QString::fromStdString(Word))
                {
                    Used.insert(Index);
                    Native.placeTile(Index);

                    break;
                }
            }
        }

        Native.submit();
    }

    QStringList AddSessionCards(bool bDueToday)
    {
        const std::vector<FTCard> Cards{MakeCard("", "manger", "verb", "to eat"), MakeCard("", "parler", "verb", "to talk"),
                                        MakeCard("", "maison", "noun", "house"), MakeCard("", "chien", "noun", "dog")};

        FTCardRepository& Repository = AppDatabase::Get().GetCardRepository();
        const FTDayNumber Today = GetTodayDayNumber();

        QStringList CardIds;

        for (const FTCard& Card : Cards)
        {
            const std::optional<std::string> CardId = Repository.AddCard(Card);
            REQUIRE(CardId.has_value());

            if (bDueToday)
            {
                FTCard Stored = **Repository.LoadCard(*CardId);
                Stored.LeitnerStage = 3;
                Stored.BoxChangedDate = Today - 4;
                Stored.DueDate = Today;

                REQUIRE(Repository.UpdateCardSchedule(Stored));
            }

            CardIds.append(QString::fromStdString(*CardId));
        }

        return CardIds;
    }

    struct FSentenceStageSnapshot
    {
        int Stage = 0;
        int SentenceCount = 0;
        QString Answer;
        QVariantList Placed;
    };

    SentenceTilesController* GetActiveTiles(const StageSessionControllerBase& Controller)
    {
        return qobject_cast<SentenceTilesController*>(Controller.GetActiveController());
    }

    FSentenceStageSnapshot LeaveInsideSentenceStage(StageSessionControllerBase& Controller,
                                                    const std::function<QByteArray()>& ReadCheckpoint)
    {
        for (int Step = 0; Step < AnswerLimit && GetActiveTiles(Controller) == nullptr; ++Step)
        {
            REQUIRE_FALSE(Controller.IsFinished());

            Controller.completeStage();
        }

        SentenceTilesController* Tiles = GetActiveTiles(Controller);
        REQUIRE(Tiles != nullptr);
        REQUIRE_FALSE(Tiles->IsBuildingTranslation());

        FSentenceStageSnapshot Snapshot;
        Snapshot.Stage = Controller.GetStage();
        Snapshot.SentenceCount =
            QJsonDocument::fromJson(ReadCheckpoint()).object()["sessionSentences"].toArray().size();
        REQUIRE(Snapshot.SentenceCount >= 2);

        Tiles->submitTyped(Tiles->GetAnswer());
        REQUIRE(Tiles->WasLastAnswerCorrect());
        Tiles->advance();

        Tiles = GetActiveTiles(Controller);
        REQUIRE(Tiles != nullptr);
        REQUIRE(Controller.GetStage() == Snapshot.Stage);

        Tiles->placeTile(0);

        Snapshot.Answer = Tiles->GetAnswer();
        Snapshot.Placed = Tiles->GetPlaced();
        REQUIRE(Snapshot.Placed.size() == 1);

        return Snapshot;
    }

    void CheckResumedInsideSentenceStage(StageSessionControllerBase& Controller, const FSentenceStageSnapshot& Snapshot)
    {
        REQUIRE(Controller.GetStage() == Snapshot.Stage);

        SentenceTilesController* Tiles = GetActiveTiles(Controller);
        REQUIRE(Tiles != nullptr);

        CHECK(Tiles->GetAnswer() == Snapshot.Answer);
        CHECK(Tiles->GetPlaced() == Snapshot.Placed);

        int Answered = 0;

        for (; Answered < AnswerLimit && Tiles != nullptr && !Tiles->IsBuildingTranslation(); ++Answered)
        {
            Tiles->submitTyped(Tiles->GetAnswer());
            Tiles->advance();

            Tiles = GetActiveTiles(Controller);
        }

        CHECK(Answered == Snapshot.SentenceCount - 1);
        CHECK(Controller.GetStage() == Snapshot.Stage);
    }
}

TEST_CASE("A card with a meaning example puts that example into the sentence phase", "[sessionsentences]")
{
    FSentenceStageTestBed TestBed;

    if (!AppDictionary::Get().GetRepository().HasMeaningExamples())
    {
        SKIP("The built dictionary has no meaning examples");
    }

    const FTCard Manger = MakeMangerWithMeaning();
    REQUIRE_FALSE(Manger.MeaningGist.empty());

    const QString Example = QString::fromStdString(FindMeaningExample(Manger));
    REQUIRE_FALSE(Example.isEmpty());

    QObject Parent;
    const std::unique_ptr<FMasteryStage> Stage = BuildMasteryStage(Parent, {Manger});

    REQUIRE(Stage->GetPhaseCount() == 3);

    SentenceTilesController* Controller = GetPhaseTiles(*Stage, 1);
    REQUIRE(Controller != nullptr);
    CHECK(Controller->IsTypingAllowed());
    CHECK_FALSE(Controller->IsBuildingTranslation());

    EnterPhase(*Stage, 1);

    REQUIRE(Stage->GetActiveController() == Controller);

    QSet<QString> Answers;

    for (int Step = 0; Step < AnswerLimit && Stage->GetPhase() == 1; ++Step)
    {
        CHECK_FALSE(Controller->GetPrompt().isEmpty());
        CHECK(Controller->GetTiles().size() >= 2);

        Answers.insert(Controller->GetAnswer());

        Controller->submitTyped(Controller->GetAnswer());
        Controller->advance();
        Stage->OnControllerStateChanged();
    }

    CHECK(Stage->GetPhase() == 2);
    CHECK(Answers.contains(Example));
}

TEST_CASE("A card with a meaning example gets no Tatoeba sentence on top", "[sessionsentences]")
{
    FSentenceStageTestBed TestBed;

    if (!AppDictionary::Get().GetRepository().HasMeaningExamples())
    {
        SKIP("The built dictionary has no meaning examples");
    }

    const FTCard Manger = MakeMangerWithMeaning();
    REQUIRE_FALSE(FindMeaningExample(Manger).empty());

    QObject Parent;
    const std::unique_ptr<FMasteryStage> Stage = BuildMasteryStage(Parent, {Manger});

    REQUIRE(Stage->GetPhaseCount() == 3);
    CHECK(Stage->GetSentences().size() == 1);

    EnterPhase(*Stage, 1);
    CHECK(GetPhaseTiles(*Stage, 1)->GetAnswer() == QString::fromStdString(FindMeaningExample(Manger)));
}

TEST_CASE("A card with no meaning picked takes its first meaning's example", "[sessionsentences]")
{
    FSentenceStageTestBed TestBed;

    if (!AppDictionary::Get().GetRepository().HasMeaningExamples())
    {
        SKIP("The built dictionary has no meaning examples");
    }

    const FTCard Manger = MakeCard("manger", "manger", "verb", "to eat");
    const std::vector<FTDictionarySense> Meanings = AppDictionary::Get().GetRepository().LoadMeanings("manger", "verb");
    const auto First = std::find_if(Meanings.begin(), Meanings.end(),
                                    [](const FTDictionarySense& Meaning) { return !Meaning.Example.empty(); });

    REQUIRE(First != Meanings.end());

    QObject Parent;
    const std::unique_ptr<FMasteryStage> Stage = BuildMasteryStage(Parent, {Manger});

    REQUIRE(Stage->GetPhaseCount() == 3);
    CHECK(Stage->GetSentences().size() == 1);

    EnterPhase(*Stage, 1);
    CHECK(GetPhaseTiles(*Stage, 1)->GetAnswer() == QString::fromStdString(First->Example));
}

TEST_CASE("A phrase the dictionary lacks gets a sentence that contains it", "[sessionsentences]")
{
    FSentenceStageTestBed TestBed;

    const FTCard Phrase = MakeCard("on-y-va", "on y va", "", "let's go");

    QObject Parent;
    const std::unique_ptr<FMasteryStage> Stage = BuildMasteryStage(Parent, {Phrase});

    REQUIRE(Stage->GetPhaseCount() == 3);
    REQUIRE(Stage->GetSentences().size() == 1);
    CHECK(Stage->GetSentences().front().WordCardId == "on-y-va");

    EnterPhase(*Stage, 1);
    CHECK(GetPhaseTiles(*Stage, 1)->GetAnswer().contains("on y va", Qt::CaseInsensitive));
}

TEST_CASE("Each sentence must be answered correctly once and a wrong answer brings it back", "[sessionsentences]")
{
    FSentenceStageTestBed TestBed;

    if (!AppDictionary::Get().GetRepository().HasMeaningExamples())
    {
        SKIP("The built dictionary has no meaning examples");
    }

    QObject Parent;
    const std::unique_ptr<FMasteryStage> Stage = BuildMasteryStage(Parent, {MakeMangerWithMeaning()});

    REQUIRE(Stage->GetPhaseCount() == 3);

    SentenceTilesController* Controller = GetPhaseTiles(*Stage, 1);
    const int SentenceCount = static_cast<int>(Stage->GetSentences().size());
    REQUIRE(SentenceCount >= 1);

    EnterPhase(*Stage, 1);

    int Asked = 0;

    for (; Asked < AnswerLimit && Stage->GetPhase() == 1; ++Asked)
    {
        const QString Given = Asked == 0 ? QStringLiteral("rien du tout ici") : Controller->GetAnswer();

        Controller->submitTyped(Given);
        CHECK(Controller->WasLastAnswerCorrect() == (Asked != 0));

        Controller->advance();
        Stage->OnControllerStateChanged();
    }

    CHECK(Stage->GetPhase() == 2);
    CHECK(Asked == SentenceCount + 1);
}

TEST_CASE("The second sentence phase builds each sentence's translation from native tiles", "[sessionsentences]")
{
    FSentenceStageTestBed TestBed;

    if (!AppDictionary::Get().GetRepository().HasMeaningExamples())
    {
        SKIP("The built dictionary has no meaning examples");
    }

    QObject Parent;
    const std::unique_ptr<FMasteryStage> Stage = BuildMasteryStage(Parent, {MakeMangerWithMeaning()});

    REQUIRE(Stage->GetPhaseCount() == 3);

    SentenceTilesController& Native = *GetPhaseTiles(*Stage, 2);
    CHECK(Native.IsBuildingTranslation());
    CHECK_FALSE(Native.IsTypingAllowed());

    EnterPhase(*Stage, 2);
    REQUIRE(Stage->GetActiveController() == &Native);

    QStringList Bank = Native.GetTiles();

    for (const std::string& Word : SplitSentenceWords(Native.GetPrompt().toStdString()))
    {
        CHECK(Bank.removeOne(QString::fromStdString(Word)));
    }
}

TEST_CASE("The mastery counter of a word with a sentence runs through the repeats and both sentence phases",
          "[sessionsentences][mastery]")
{
    FSentenceStageTestBed TestBed;

    if (!AppDictionary::Get().GetRepository().HasMeaningExamples())
    {
        SKIP("The built dictionary has no meaning examples");
    }

    SessionSettings::Training().SetRepeatsToMaster(3);

    QObject Parent;
    const std::unique_ptr<FMasteryStage> Stage = BuildMasteryStage(Parent, {MakeMangerWithMeaning()});

    REQUIRE(Stage->GetPhaseCount() == 3);

    EnterPhase(*Stage, 1);

    SentenceTilesController& Foreign = *GetPhaseTiles(*Stage, 1);
    REQUIRE(Stage->GetActiveController() == &Foreign);

    CHECK(Foreign.GetMasteryTarget() == 5);
    CHECK(Foreign.GetCardMasteryCount() == 3);

    Foreign.submitTyped(Foreign.GetAnswer());
    REQUIRE(Foreign.WasLastAnswerCorrect());
    CHECK(Foreign.GetCardMasteryCount() == 4);

    Foreign.advance();
    Stage->OnControllerStateChanged();

    SentenceTilesController& Native = *GetPhaseTiles(*Stage, 2);
    REQUIRE(Stage->GetActiveController() == &Native);

    CHECK(Native.GetMasteryTarget() == 5);
    CHECK(Native.GetCardMasteryCount() == 4);

    AnswerTranslation(Native);
    REQUIRE(Native.WasLastAnswerCorrect());
    CHECK(Native.GetCardMasteryCount() == 5);

    Native.advance();
    Stage->OnControllerStateChanged();

    CHECK(Stage->IsFinished());
}

TEST_CASE("A word without a sentence keeps the repeats as its mastery target", "[sessionsentences][mastery]")
{
    FSentenceStageTestBed TestBed;

    if (!AppDictionary::Get().GetRepository().HasMeaningExamples())
    {
        SKIP("The built dictionary has no meaning examples");
    }

    SessionSettings::Training().SetRepeatsToMaster(3);

    QObject Parent;
    const std::unique_ptr<FMasteryStage> Stage =
        BuildMasteryStage(Parent, {MakeMangerWithMeaning(), MakeCard("none", "zzzzqx", "", "nothing")});

    REQUIRE(Stage->GetPhaseCount() == 3);
    REQUIRE(Stage->GetSentences().size() == 1);

    Stage->Start();

    CHECK(Stage->GetControllers().front()->GetMaxSessionCount() == 5 + 3);
}

TEST_CASE("Skipping phases walks through the sentence phases and the last skip leaves the stage to the caller",
          "[sessionsentences][mastery]")
{
    FSentenceStageTestBed TestBed;

    if (!AppDictionary::Get().GetRepository().HasMeaningExamples())
    {
        SKIP("The built dictionary has no meaning examples");
    }

    QObject Parent;
    const std::unique_ptr<FMasteryStage> Stage = BuildMasteryStage(Parent, {MakeMangerWithMeaning()});

    REQUIRE(Stage->GetPhaseCount() == 3);

    Stage->Start();
    CHECK(Stage->GetPhase() == 0);

    CHECK(Stage->SkipPhase());
    CHECK(Stage->GetPhase() == 1);
    CHECK_FALSE(Stage->IsFinished());

    CHECK(Stage->SkipPhase());
    CHECK(Stage->GetPhase() == 2);

    CHECK_FALSE(Stage->SkipPhase());

    Stage->ForceComplete();
    CHECK(Stage->IsFinished());
}

TEST_CASE("Sentences keep the word they were picked for through the checkpoint JSON", "[sessionsentences][checkpoint]")
{
    FTTreeSentence Sentence;
    Sentence.Id = 5;
    Sentence.Text = "Il mange une pomme.";
    Sentence.Translation = "He eats an apple.";

    const std::vector<FSessionSentence> Saved{
        FSessionSentence{MakeCardHandle(MakeSentenceCard(Sentence)), Sentence, "word-1"}};

    const std::vector<FSessionSentence> Restored = SessionSentencesFromJson(SessionSentencesToJson(Saved));

    REQUIRE(Restored.size() == 1);
    CHECK(Restored.front().WordCardId == "word-1");
    CHECK(Restored.front().Card->Id == Saved.front().Card->Id);
}

TEST_CASE("Saved sentences from before words were recorded keep a meaning example's word and drop the rest",
          "[sessionsentences][checkpoint]")
{
    FTTreeSentence Sentence;
    Sentence.Id = 5;
    Sentence.Text = "Il mange une pomme.";
    Sentence.Translation = "He eats an apple.";

    FTCard Example = MakeSentenceCard(Sentence);
    Example.Id = "meaning:word-9";

    const std::vector<FSessionSentence> Saved{FSessionSentence{MakeCardHandle(Example), Sentence, "word-9"}};

    QJsonArray Legacy = SessionSentencesToJson(Saved);
    QJsonObject Entry = Legacy.at(0).toObject();
    Entry.remove("wordCardId");
    Legacy = QJsonArray{Entry};

    const std::vector<FSessionSentence> Restored = SessionSentencesFromJson(Legacy);

    REQUIRE(Restored.size() == 1);
    CHECK(Restored.front().WordCardId == "word-9");

    Entry["cardId"] = QStringLiteral("sentence:5");

    CHECK(SessionSentencesFromJson(QJsonArray{Entry}).empty());
}

TEST_CASE("Typing mode is a remembered setting the sentence stage allows", "[sessionsentences]")
{
    FSentenceStageTestBed TestBed;

    SentenceTilesController Plain;
    Plain.SetTyping(true);
    CHECK_FALSE(Plain.IsTyping());

    SentenceTilesController Allowed;
    Allowed.SetTypingAllowed(true);
    Allowed.SetTyping(true);
    CHECK(Allowed.IsTyping());

    SentenceTilesController Later;
    Later.SetTypingAllowed(true);
    CHECK(Later.IsTyping());

    Later.SetTyping(false);
    CHECK_FALSE(Allowed.IsTyping());
}

TEST_CASE("An empty session gets no sentence phases", "[sessionsentences]")
{
    FSentenceStageTestBed TestBed;

    QObject Parent;
    const FSessionSentenceStage Built = BuildSessionSentenceStage(&Parent, MakeParams({}), SessionSettings::Training());

    CHECK(Built.Sentences.empty());
    CHECK(Built.Controllers.empty());

    const std::unique_ptr<FMasteryStage> Stage = BuildMasteryStage(Parent, {});

    CHECK(Stage->GetPhaseCount() == 1);
}

TEST_CASE("A Train session left inside the sentence stage resumes on the same sentence and progress",
          "[sessionsentences][checkpoint]")
{
    FSentenceStageTestBed TestBed;

    if (!AppDictionary::Get().GetRepository().HasMeaningExamples())
    {
        SKIP("The built dictionary has no meaning examples");
    }

    const QStringList CardIds = AddSessionCards(false);
    const QString SessionKey = QStringLiteral("sentence-stage-resume");

    FSentenceStageSnapshot Snapshot;

    {
        IntensiveLearningController Controller;
        REQUIRE(Controller.startCards(SessionKey, CardIds));

        Snapshot = LeaveInsideSentenceStage(
            Controller, [&SessionKey] { return AppState::Get().IntensiveLearningCheckpoints[SessionKey]; });
    }

    IntensiveLearningController Resumed;
    REQUIRE(Resumed.resume(SessionKey));

    CheckResumedInsideSentenceStage(Resumed, Snapshot);
}

TEST_CASE("A Review session left inside the sentence stage resumes on the same sentence and progress",
          "[sessionsentences][checkpoint]")
{
    FSentenceStageTestBed TestBed;

    if (!AppDictionary::Get().GetRepository().HasMeaningExamples())
    {
        SKIP("The built dictionary has no meaning examples");
    }

    SessionSettings::Review().SetSentenceStage(true);

    AddSessionCards(true);

    FSentenceStageSnapshot Snapshot;

    {
        RepetitionController Controller;
        REQUIRE(Controller.startDueToday());

        Snapshot = LeaveInsideSentenceStage(Controller, [] { return AppState::Get().RepetitionCheckpoint; });
    }

    RepetitionController Resumed;
    REQUIRE(Resumed.startDueToday());

    CheckResumedInsideSentenceStage(Resumed, Snapshot);
}
