#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "ExerciseFlowStages.h"

#include "TestAppSpeech.h"
#include "AudioTranscriptionController.h"
#include "WrittenProductionController.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
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

    FTSessionStartParams MakeParams(std::vector<FTCardHandle> Cards, int FixedLength)
    {
        FTSessionStartParams Params{};
        Params.CardState = std::make_shared<FCardState>(std::move(Cards));
        Params.Title = "Test Session";
        Params.TargetLanguage = "fr";
        Params.FixedLength = FixedLength;
        Params.Selector = std::make_shared<FEachOnceSelector>(Params.CardState->GetCount(), true);

        return Params;
    }
}

TEST_CASE("A proxy stage is not finished until its controller finishes", "[exerciseflowstages]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    FProxyStage Stage(&Controller, MakeParams(MakeCards(3), 3));

    Stage.Start();

    CHECK_FALSE(Stage.IsFinished());
}

TEST_CASE("ForceComplete marks a proxy stage finished without touching its controller's own state",
         "[exerciseflowstages]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    FProxyStage Stage(&Controller, MakeParams(MakeCards(3), 3));

    Stage.Start();
    REQUIRE_FALSE(Stage.IsFinished());

    Stage.ForceComplete();

    CHECK(Stage.IsFinished());
    CHECK_FALSE(Controller.IsFinished());
    CHECK_FALSE(Controller.IsAnswered());
}

TEST_CASE("ForceRestart on a proxy stage clears the force-complete flag and restarts the controller",
         "[exerciseflowstages]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Controller;
    FProxyStage Stage(&Controller, MakeParams(MakeCards(3), 3));

    Stage.Start();
    Controller.submitAnswer(Controller.GetAnswer());
    Stage.ForceComplete();
    REQUIRE(Stage.IsFinished());

    Stage.ForceRestart();

    CHECK_FALSE(Stage.IsFinished());
    CHECK_FALSE(Controller.IsAnswered());
    CHECK(Controller.GetCurrentSessionCount() == 1);
}

namespace
{
    std::vector<FMasteryStageSessionController> MakeCandidates(
        AudioTranscriptionController& Audio, WrittenProductionController& Written)
    {
        return {{&Audio, 1.0, [] { return true; }}, {&Written, 1.0, [] { return true; }}};
    }

    std::vector<int> MakeOrder()
    {
        return {0, 1};
    }
}

TEST_CASE("A mastery loop stage is not finished before any round completes", "[exerciseflowstages]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Audio;
    WrittenProductionController Written;

    const int RepeatsToMaster = 2;

    FMasteryLoopStage Stage(MakeCandidates(Audio, Written), MakeOrder(), MakeParams(MakeCards(3), 10),
        RepeatsToMaster);

    Stage.Start();

    CHECK_FALSE(Stage.IsFinished());
    CHECK(Stage.GetActiveController() != nullptr);
}

TEST_CASE("ForceComplete masters every card", "[exerciseflowstages]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Audio;
    WrittenProductionController Written;

    const int RepeatsToMaster = 2;

    FMasteryLoopStage Stage(MakeCandidates(Audio, Written), MakeOrder(), MakeParams(MakeCards(3), 10),
        RepeatsToMaster);

    Stage.Start();
    REQUIRE_FALSE(Stage.IsFinished());

    Stage.ForceComplete();

    CHECK(Stage.IsFinished());
}

TEST_CASE("ForceRestart on a mastery loop stage clears mastery and batch progress",
         "[exerciseflowstages]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Audio;
    WrittenProductionController Written;

    const int RepeatsToMaster = 2;

    FMasteryLoopStage Stage(MakeCandidates(Audio, Written), MakeOrder(), MakeParams(MakeCards(3), 10),
        RepeatsToMaster);

    Stage.Start();
    Stage.ForceComplete();
    REQUIRE(Stage.IsFinished());

    Stage.ForceRestart();

    CHECK_FALSE(Stage.IsFinished());
    CHECK(Stage.GetActiveController() != nullptr);
}

TEST_CASE("A correct answer increments mastery count, a wrong answer decrements it",
         "[exerciseflowstages]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Audio;
    WrittenProductionController Written;

    const int RepeatsToMaster = 5;

    std::vector<FTCardHandle> Cards = MakeCards(1);
    const std::string CardId = Cards.front()->Id;

    FMasteryLoopStage Stage(MakeCandidates(Audio, Written), MakeOrder(), MakeParams(std::move(Cards), 10),
        RepeatsToMaster);

    Stage.Start();
    REQUIRE(Stage.GetActiveController() == &Audio);

    Audio.submitAnswer(Audio.GetPrompt());
    Stage.OnControllerStateChanged();

    CHECK(Stage.GetCardMasteryCount(CardId) == 1);

    Audio.advance();
    Stage.OnControllerStateChanged();

    REQUIRE(Stage.GetActiveController() == &Written);

    Written.submitAnswer(Written.GetAnswer() + "wrong");
    Stage.OnControllerStateChanged();

    CHECK(Stage.GetCardMasteryCount(CardId) == 0);
}

TEST_CASE("SaveProgress reports the stage's mastery bookkeeping directly", "[exerciseflowstages]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController Audio;
    WrittenProductionController Written;

    const int RepeatsToMaster = 2;

    FMasteryLoopStage Stage(MakeCandidates(Audio, Written), MakeOrder(), MakeParams(MakeCards(3), 10),
        RepeatsToMaster);

    Stage.Start();

    const FMasteryLoopProgress FreshProgress = Stage.SaveProgress();
    CHECK(FreshProgress.MasteryCount.empty());
    CHECK(FreshProgress.ConsecutiveWrongCount.empty());
    CHECK(FreshProgress.BatchCount == 0);

    Stage.ForceComplete();

    const FMasteryLoopProgress FinishedProgress = Stage.SaveProgress();
    REQUIRE(FinishedProgress.MasteryCount.size() == 3);

    for (const auto& [CardId, Count] : FinishedProgress.MasteryCount)
    {
        CHECK(Count >= RepeatsToMaster);
    }
}

TEST_CASE("RestoreProgress carries mastery bookkeeping onto a fresh stage", "[exerciseflowstages]")
{
    FAppSpeechGuard SpeechGuard;

    AudioTranscriptionController SourceAudio;
    WrittenProductionController SourceWritten;

    const int RepeatsToMaster = 2;

    FMasteryLoopStage Source(MakeCandidates(SourceAudio, SourceWritten), MakeOrder(), MakeParams(MakeCards(3), 10),
        RepeatsToMaster);

    Source.Start();
    Source.ForceComplete();
    REQUIRE(Source.IsFinished());

    const FMasteryLoopProgress Progress = Source.SaveProgress();

    AudioTranscriptionController FreshAudio;
    WrittenProductionController FreshWritten;

    FMasteryLoopStage Restored(MakeCandidates(FreshAudio, FreshWritten), MakeOrder(), MakeParams(MakeCards(3), 10),
        RepeatsToMaster);

    Restored.Start();
    REQUIRE_FALSE(Restored.IsFinished());

    Restored.RestoreProgress(Progress);

    CHECK(Restored.IsFinished());
    CHECK(Restored.GetActiveController() != nullptr);
}

TEST_CASE("A controller does not speak its next card once the mastery stage is complete", "[exerciseflowstages]")
{
    FAppSpeechGuard SpeechGuard;

    const int RepeatsToMaster = 1;
    const int Attempts = 20;

    for (int Attempt = 0; Attempt < Attempts; ++Attempt)
    {
        AudioTranscriptionController Audio;
        WrittenProductionController Written;

        FMasteryLoopStage Stage(MakeCandidates(Audio, Written), MakeOrder(), MakeParams(MakeCards(3), 3),
            RepeatsToMaster);

        FMasteryLoopProgress Progress;
        Progress.MasteryCount["word0"] = RepeatsToMaster;
        Progress.MasteryCount["word1"] = RepeatsToMaster;

        Stage.RestoreProgress(Progress);
        REQUIRE(Stage.GetActiveController() == &Audio);

        for (int Round = 0; Round < 3 && !Stage.IsFinished(); ++Round)
        {
            Audio.submitAnswer(Audio.GetPrompt());
            Stage.OnControllerStateChanged();

            const int SpeakCountBeforeAdvance = SpeechGuard.Mock.SpeakCount;

            Audio.advance();

            if (Stage.IsFinished())
            {
                CHECK(SpeechGuard.Mock.SpeakCount == SpeakCountBeforeAdvance);
            }

            Stage.OnControllerStateChanged();
        }

        CHECK(Stage.IsFinished());
    }
}

TEST_CASE("A grouped mastery stage asks one group at a time and keeps it until every card in it is right",
          "[exerciseflowstages]")
{
    FAppSpeechGuard SpeechGuard;

    WrittenProductionController Written;

    std::vector<FMasteryStageSessionController> Candidates{{&Written, 1.0, [] { return true; }}};

    FMasteryLoopStage Stage(std::move(Candidates), {0}, MakeParams(MakeCards(4), 10), 1);
    Stage.SetBatchGroups({{2, 3}, {0, 1}});
    Stage.Start();

    std::vector<std::string> Asked;

    while (!Stage.IsFinished())
    {
        REQUIRE(Asked.size() < 10);
        REQUIRE(Written.GetCurrentCard() != nullptr);

        Asked.push_back(Written.GetCurrentCard()->Id);

        const QString Expected = Written.GetAnswer();

        if (Asked.size() == 1)
        {
            Written.submitAnswer(Expected + "wrong");
            REQUIRE_FALSE(Written.WasLastAnswerCorrect());
        }

        if (!Written.IsAnswered() || Written.IsRetypePending())
        {
            Written.submitAnswer(Expected);
        }

        Stage.OnControllerStateChanged();

        Written.advance();
        Stage.OnControllerStateChanged();
    }

    REQUIRE(Asked.size() == 5);
    CHECK(std::set<std::string>(Asked.begin(), Asked.begin() + 3) == std::set<std::string>{"word2", "word3"});
    CHECK(Asked[2] == Asked[0]);
    CHECK(std::set<std::string>(Asked.begin() + 3, Asked.end()) == std::set<std::string>{"word0", "word1"});
}

namespace
{
    std::vector<int> DrainSorted(FUnmasteredSelector& Selector)
    {
        std::vector<int> Drawn;

        while (const std::optional<int> Index = Selector.Next())
        {
            Drawn.push_back(*Index);
        }

        std::sort(Drawn.begin(), Drawn.end());

        return Drawn;
    }
}

TEST_CASE("An unmastered selector returns every unmastered card exactly once", "[exerciseflowstages][unmasteredselector]")
{
    const std::set<int> Mastered{1, 3};
    FUnmasteredSelector Selector(5, [&Mastered](int32 Index) { return Mastered.count(Index) > 0; });

    CHECK(DrainSorted(Selector) == std::vector<int>{0, 2, 4});
}

TEST_CASE("An unmastered selector returns every card when none is mastered", "[exerciseflowstages][unmasteredselector]")
{
    FUnmasteredSelector Selector(4, [](int32) { return false; });

    CHECK(DrainSorted(Selector) == std::vector<int>{0, 1, 2, 3});
}

TEST_CASE("An unmastered selector returns nothing when every card is mastered", "[exerciseflowstages][unmasteredselector]")
{
    FUnmasteredSelector Selector(4, [](int32) { return true; });

    CHECK_FALSE(Selector.Next().has_value());
}

TEST_CASE("An unmastered selector over an empty batch returns nothing", "[exerciseflowstages][unmasteredselector]")
{
    FUnmasteredSelector Selector(0, [](int32) { return false; });

    CHECK_FALSE(Selector.Next().has_value());
}

TEST_CASE("An unmastered selector skips a card that reached mastery after it was created", "[exerciseflowstages][unmasteredselector]")
{
    std::set<int> Mastered;
    FUnmasteredSelector Selector(3, [&Mastered](int32 Index) { return Mastered.count(Index) > 0; });

    const std::optional<int> First = Selector.Next();
    REQUIRE(First.has_value());

    Mastered = {0, 1, 2};

    CHECK_FALSE(Selector.Next().has_value());
}

TEST_CASE("An unmastered selector stops for good once it runs out", "[exerciseflowstages][unmasteredselector]")
{
    FUnmasteredSelector Selector(2, [](int32 Index) { return Index == 0; });

    REQUIRE(Selector.Next() == std::optional<int>(1));

    CHECK_FALSE(Selector.Next().has_value());
    CHECK_FALSE(Selector.Next().has_value());
}
