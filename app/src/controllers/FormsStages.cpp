#include "FormsStages.h"

#include "AppCards.h"
#include "AppDictionary.h"

#include "ExerciseFlowStages.h"
#include "FlashcardController.h"
#include "SentenceSelection.h"
#include "SpeedMatchController.h"
#include "GapProductionController.h"

#include "core/FCardSelectorBase.h"
#include "core/FTLog.h"
#include "core/FTSentenceCards.h"
#include "core/FTWordForms.h"

#include <QElapsedTimer>

#include <algorithm>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

static constexpr size_t FormsRoundCount = 2;

static bool IsVerbForm(const FTCard& Card)
{
    return GetFormsKind(Card.Id) == FormsKindVerb;
}

static QVariantList BuildWordFormsTable(const FTCard& Form)
{
    const std::optional<FTWordForm> Read = ReadWordForm(Form);

    if (!Read.has_value())
    {
        return {};
    }

    FTCard Stored;
    Stored.Id = GetFormsCardId(Form.Id);
    Stored.TargetWord = Read->Headword;

    return AppCards::BuildFormsTable(Stored);
}

static FMatchPair ReadMatchPair(const FTCard& Card)
{
    const std::optional<FTWordForm> Read = ReadWordForm(Card);

    if (!Read.has_value())
    {
        return {Card.TargetWord, Card.Translation, {}, {}};
    }

    return {Read->SubjectLabel, Read->Form, AppCards::MarkFormEnding(*Read),
            QString::fromStdString(Read->SubjectLabel).toHtmlEscaped()};
}

static void AddStudyStage(QObject* Parent, const FTSessionStartParams& Params, FFormsStages& Result)
{
    const int WordCount = Params.CardState->GetCount();

    FTSessionStartParams StudyParams = Params;
    StudyParams.FixedLength = WordCount;
    StudyParams.Selector =
        std::make_shared<FGroupedSelector>(GroupCardsByWord(Params.CardState->GetCards()), true, false);

    auto* StudyInstance = new FlashcardController(Parent);
    StudyInstance->SetFormsTable(AppCards::BuildFormsTable);
    Result.Controllers.push_back(StudyInstance);
    Result.Stages.push_back(std::make_unique<FProxyStage>(StudyInstance, StudyParams));
}

static void AddMatchStage(QObject* Parent, const FTSessionStartParams& Params, FFormsStages& Result)
{
    FTSessionStartParams MatchParams = Params;
    MatchParams.FixedLength = Params.CardState->GetCount();
    MatchParams.Selector = std::make_shared<FGroupedSelector>(GroupCardsByWord(Params.CardState->GetCards()), true, true);

    auto* MatchInstance = new SpeedMatchController(Parent);
    MatchInstance->SetPairText(ReadMatchPair);
    MatchInstance->SetScreenKey([](const FTCard& Card) { return GetFormsCardId(Card.Id); });
    MatchInstance->SetScreenHeading([](const FTCard& Card)
    {
        const std::optional<FTWordForm> Read = ReadWordForm(Card);

        return Read.has_value() ? QString::fromStdString(Read->Headword) : QString();
    });
    MatchInstance->SetOrderedSides(true, false);
    MatchInstance->SetFormsTable(BuildWordFormsTable);

    Result.Controllers.push_back(MatchInstance);
    Result.Stages.push_back(std::make_unique<FProxyStage>(MatchInstance, MatchParams));
}

static std::vector<uint32_t> ListFocusWordIds(const std::vector<FTCardHandle>& Forms, const std::vector<int>& Word)
{
    const FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();

    std::vector<std::string> Spellings;

    for (const int FormIndex : Word)
    {
        const FTCard& Form = *Forms[static_cast<size_t>(FormIndex)];
        const std::optional<FTWordForm> Read = ReadWordForm(Form);

        if (!Read.has_value())
        {
            continue;
        }

        Spellings.push_back(Read->Headword);

        if (!IsVerbForm(Form))
        {
            Spellings.push_back(Read->Form);
        }
    }

    std::vector<uint32_t> WordIds;

    for (const std::string& Spelling : Spellings)
    {
        for (const uint32_t WordId : Dictionary.ListWordIds(Spelling))
        {
            if (std::find(WordIds.begin(), WordIds.end(), WordId) == WordIds.end())
            {
                WordIds.push_back(WordId);
            }
        }
    }

    return WordIds;
}

static std::vector<std::vector<uint32_t>> OrderSentencesByWord(const std::vector<std::vector<uint32_t>>& FocusWordIds,
                                                               uint32_t Seed)
{
    std::vector<std::vector<uint32_t>> SentencesByWord(FocusWordIds.size());

    const FTSentenceTree* Tree = AppDictionary::Get().GetSentenceTree();

    if (Tree == nullptr)
    {
        return SentencesByWord;
    }

    std::vector<FTFocusedSentence> Found = Tree->FindSentencesWithWords(FocusWordIds);

    std::shuffle(Found.begin(), Found.end(), std::mt19937{Seed});
    std::stable_sort(Found.begin(), Found.end(), [](const FTFocusedSentence& Left, const FTFocusedSentence& Right)
    {
        return Left.Level < Right.Level;
    });

    for (const FTFocusedSentence& Candidate : Found)
    {
        for (size_t Word = 0; Word < SentencesByWord.size(); ++Word)
        {
            if ((Candidate.FocusMask & (uint64_t{1} << Word)) != 0)
            {
                SentencesByWord[Word].push_back(Candidate.SentenceId);
            }
        }
    }

    return SentencesByWord;
}

struct FFormCandidate
{
    FTTreeSentence Sentence;
    bool bSubjectSpoken = false;
    size_t Order = 0;
};

static std::vector<std::vector<FFormCandidate>> SortSentencesByForm(const std::vector<FTCardHandle>& Forms,
                                                                    const std::vector<int>& Word,
                                                                    const std::vector<uint32_t>& Ordered,
                                                                    const std::unordered_set<uint32_t>& Used,
                                                                    size_t& LoadedCount)
{
    std::vector<std::vector<FFormCandidate>> Buckets(Word.size());

    std::vector<FTTreeSentence> Loaded = AppDictionary::Get().GetRepository().LoadTreeSentences(Ordered);
    LoadedCount += Loaded.size();

    std::unordered_map<uint32_t, size_t> OrderById;

    for (size_t Order = 0; Order < Ordered.size(); ++Order)
    {
        OrderById.emplace(Ordered[Order], Order);
    }

    for (const FTTreeSentence& Sentence : Loaded)
    {
        if (Used.count(Sentence.Id) != 0 || FindUnusableSentenceReason(Sentence) != nullptr)
        {
            continue;
        }

        for (size_t Slot = 0; Slot < Word.size(); ++Slot)
        {
            const std::optional<FTFormGap> Gap = LocateFormGap(*Forms[static_cast<size_t>(Word[Slot])], Sentence);

            if (!Gap.has_value())
            {
                continue;
            }

            FFormCandidate Candidate{Sentence, Gap->bSubjectSpoken, OrderById[Sentence.Id]};
            Candidate.Sentence.GapStart = Gap->Start;
            Candidate.Sentence.GapLength = Gap->Length;

            Buckets[Slot].push_back(std::move(Candidate));
        }
    }

    for (std::vector<FFormCandidate>& Bucket : Buckets)
    {
        std::sort(Bucket.begin(), Bucket.end(), [](const FFormCandidate& Left, const FFormCandidate& Right)
        {
            if (Left.bSubjectSpoken != Right.bSubjectSpoken)
            {
                return Left.bSubjectSpoken;
            }

            return Left.Order < Right.Order;
        });
    }

    return Buckets;
}

static void PlaceWordForms(const std::vector<FTCardHandle>& Forms, const std::vector<int>& Word,
                           const std::vector<uint32_t>& Ordered, std::unordered_set<uint32_t>& Used,
                           std::unordered_map<std::string, std::vector<FTTreeSentence>>& Picked, size_t& LoadedCount)
{
    std::vector<std::vector<FFormCandidate>> Buckets = SortSentencesByForm(Forms, Word, Ordered, Used, LoadedCount);

    std::vector<size_t> Slots(Word.size());
    std::iota(Slots.begin(), Slots.end(), size_t{0});
    std::stable_sort(Slots.begin(), Slots.end(), [&Buckets](size_t Left, size_t Right)
    {
        return Buckets[Left].size() < Buckets[Right].size();
    });

    for (const size_t Slot : Slots)
    {
        std::vector<FTTreeSentence>& FormSentences = Picked[Forms[static_cast<size_t>(Word[Slot])]->Id];

        for (FFormCandidate& Candidate : Buckets[Slot])
        {
            if (FormSentences.size() >= FormsRoundCount)
            {
                break;
            }

            if (Used.insert(Candidate.Sentence.Id).second)
            {
                FormSentences.push_back(std::move(Candidate.Sentence));
            }
        }
    }
}

static void PlaceLockedForms(const std::vector<FTCardHandle>& Forms, const std::vector<std::vector<int>>& Words,
                             std::unordered_map<std::string, std::vector<FTTreeSentence>>& Picked, size_t& LoadedCount)
{
    const FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();

    std::vector<std::pair<const FTCard*, uint32_t>> Locked;

    for (const std::vector<int>& Word : Words)
    {
        const std::optional<FTFormsSentenceKey> WordKey =
            ReadFormsSentenceKey(*Forms[static_cast<size_t>(Word.front())]);

        if (!WordKey.has_value())
        {
            continue;
        }

        for (const FTFormSentence& Row : Dictionary.FindFormSentences(WordKey->Headword, WordKey->Kind, WordKey->Tense))
        {
            for (const int FormIndex : Word)
            {
                const FTCard& Form = *Forms[static_cast<size_t>(FormIndex)];
                const std::optional<FTFormsSentenceKey> Key = ReadFormsSentenceKey(Form);

                if (Key.has_value() && Key->First == Row.First && Key->Number == Row.Number)
                {
                    Locked.emplace_back(&Form, Row.SentenceId);
                }
            }
        }
    }

    std::vector<uint32_t> SentenceIds;

    for (const auto& [Form, SentenceId] : Locked)
    {
        SentenceIds.push_back(SentenceId);
    }

    std::unordered_map<uint32_t, FTTreeSentence> LoadedById;

    for (FTTreeSentence& Sentence : Dictionary.LoadTreeSentences(SentenceIds))
    {
        ++LoadedCount;

        const uint32_t Id = Sentence.Id;
        LoadedById.emplace(Id, std::move(Sentence));
    }

    for (const auto& [Form, SentenceId] : Locked)
    {
        const auto Found = LoadedById.find(SentenceId);

        if (Found == LoadedById.end())
        {
            continue;
        }

        FTTreeSentence Sentence = Found->second;

        if (!PlaceFormGap(*Form, Sentence))
        {
            qCWarning(LogSession) << "Locked sentence" << SentenceId << "does not hold the form"
                                  << QString::fromStdString(Form->Id);

            continue;
        }

        Picked[Form->Id].push_back(std::move(Sentence));
    }
}

static std::unordered_map<std::string, std::vector<FTTreeSentence>> PickFormSentences(
    const std::vector<FTCardHandle>& Forms, uint32_t Seed)
{
    std::unordered_map<std::string, std::vector<FTTreeSentence>> Picked;

    if (!AppDictionary::Get().IsReady() || Forms.empty())
    {
        return Picked;
    }

    QElapsedTimer Timer;
    Timer.start();

    const std::vector<std::vector<int>> Words = GroupCardsByWord(Forms);

    const auto CountPlaced = [&Forms, &Picked]()
    {
        return std::count_if(Forms.begin(), Forms.end(), [&Picked](const FTCardHandle& Form)
        {
            return !Picked[Form->Id].empty();
        });
    };

    if (AppDictionary::Get().GetRepository().HasFormSentences())
    {
        size_t LoadedCount = 0;
        PlaceLockedForms(Forms, Words, Picked, LoadedCount);

        qCInfo(LogSession) << "Forms typing: locked sentences for" << CountPlaced() << "of" << Forms.size()
                           << "forms from" << LoadedCount << "loaded in" << Timer.elapsed() << "ms";

        return Picked;
    }

    std::vector<std::vector<uint32_t>> FocusWordIds;

    for (const std::vector<int>& Word : Words)
    {
        FocusWordIds.push_back(ListFocusWordIds(Forms, Word));
    }

    const std::vector<std::vector<uint32_t>> SentencesByWord = OrderSentencesByWord(FocusWordIds, Seed);
    const qint64 SearchMs = Timer.restart();

    std::unordered_set<uint32_t> Used;
    size_t LoadedCount = 0;

    for (size_t Word = 0; Word < Words.size(); ++Word)
    {
        PlaceWordForms(Forms, Words[Word], SentencesByWord[Word], Used, Picked, LoadedCount);
    }

    qCInfo(LogSession) << "Forms typing: sentences for" << CountPlaced() << "of" << Forms.size() << "forms from"
                       << LoadedCount << "loaded: search" << SearchMs << "ms, load and place" << Timer.elapsed() << "ms";

    return Picked;
}

struct FFormGaps
{
    std::vector<FTCardHandle> Asked;
    uint32_t Seed = 0;
    bool bPicked = false;
    std::unordered_map<std::string, FTTreeSentence> ByRound;
};

static const FTTreeSentence* FindFormGap(FFormGaps& Gaps, const std::string& RoundId)
{
    if (!Gaps.bPicked)
    {
        Gaps.bPicked = true;

        for (const auto& [FormId, FormSentences] : PickFormSentences(Gaps.Asked, Gaps.Seed))
        {
            for (size_t Round = 0; Round < FormsRoundCount && !FormSentences.empty(); ++Round)
            {
                Gaps.ByRound.emplace(MakeFormsRoundId(FormId, static_cast<int>(Round) + 1),
                                     FormSentences[Round % FormSentences.size()]);
            }
        }
    }

    const auto Found = Gaps.ByRound.find(RoundId);

    return Found != Gaps.ByRound.end() ? &Found->second : nullptr;
}

static std::vector<std::vector<size_t>> ShuffleWordsPerRound(size_t WordCount, uint32_t Seed)
{
    std::mt19937 Generator{Seed};

    std::vector<size_t> Order(WordCount);
    std::iota(Order.begin(), Order.end(), size_t{0});

    std::vector<std::vector<size_t>> Rounds;

    for (size_t Round = 0; Round < FormsRoundCount; ++Round)
    {
        std::shuffle(Order.begin(), Order.end(), Generator);

        if (!Rounds.empty() && Order.size() > 1 && Order.front() == Rounds.back().back())
        {
            std::swap(Order.front(), Order.back());
        }

        Rounds.push_back(Order);
    }

    return Rounds;
}

struct FFormRounds
{
    std::vector<FTCardHandle> Cards;
    std::vector<std::vector<int>> Batches;
};

static FFormRounds MakeFormRounds(const std::vector<FTCardHandle>& Asked, uint32_t Seed)
{
    const std::vector<std::vector<int>> Words = GroupCardsByWord(Asked);
    const std::vector<std::vector<size_t>> WordsPerRound = ShuffleWordsPerRound(Words.size(), Seed);

    FFormRounds Result;

    for (size_t Round = 0; Round < WordsPerRound.size(); ++Round)
    {
        for (const size_t Word : WordsPerRound[Round])
        {
            std::vector<int>& Batch = Result.Batches.emplace_back();

            for (const int FormIndex : Words[Word])
            {
                const FTCardHandle& Form = Asked[static_cast<size_t>(FormIndex)];

                FTCard Card = *Form;
                Card.Id = MakeFormsRoundId(Form->Id, static_cast<int>(Round) + 1);

                Batch.push_back(static_cast<int>(Result.Cards.size()));
                Result.Cards.push_back(MakeCardHandle(std::move(Card)));
            }
        }
    }

    return Result;
}

static void AddTypingStage(QObject* Parent, const FTSessionStartParams& Params, uint32_t SentenceSeed,
                           FFormsStages& Result)
{
    auto Gaps = std::make_shared<FFormGaps>();
    Gaps->Seed = SentenceSeed;

    for (const FTCardHandle& Card : Params.CardState->GetCards())
    {
        const std::optional<FTWordForm> Read = ReadWordForm(*Card);

        if (!Read.has_value() || Read->Form != Read->Headword)
        {
            Gaps->Asked.push_back(Card);
        }
    }

    FFormRounds Rounds = MakeFormRounds(Gaps->Asked, SentenceSeed);

    FTSessionStartParams TypingParams = Params;
    TypingParams.CardState = std::make_shared<FCardState>(Rounds.Cards);

    auto* TypingInstance = new FormGapController(Parent);
    TypingInstance->SetWordMarkup(AppCards::MarkFormWord);
    TypingInstance->SetFormsTable(BuildWordFormsTable);
    TypingInstance->SetSentenceLookup([Gaps](const FTCard& Card) { return FindFormGap(*Gaps, Card.Id); });
    Result.Controllers.push_back(TypingInstance);

    std::vector<FMasteryStageSessionController> Candidates;
    Candidates.push_back(FMasteryStageSessionController{TypingInstance, 1.0, [] { return true; }});

    auto Stage = std::make_unique<FMasteryLoopStage>(std::move(Candidates), std::vector<int>{0}, TypingParams, 1);
    Stage->SetCardGroup(GetFormsItemId);
    Stage->SetBatchGroups(std::move(Rounds.Batches));

    Result.TypingStage = Stage.get();
    Result.Stages.push_back(std::move(Stage));
}

FFormsStages BuildFormsStages(QObject* Parent, const FTSessionStartParams& Params, uint32_t SentenceSeed)
{
    FFormsStages Result;

    AddStudyStage(Parent, Params, Result);

    FTSessionStartParams FormsParams = Params;
    FormsParams.CardState = std::make_shared<FCardState>(AppCards::ExpandStudiedForms(Params.CardState->GetCards()));

    AddMatchStage(Parent, FormsParams, Result);
    AddTypingStage(Parent, FormsParams, SentenceSeed, Result);

    return Result;
}
