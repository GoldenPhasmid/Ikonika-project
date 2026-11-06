#include "SessionSentenceStage.h"

#include "AppDictionary.h"
#include "SentenceSelection.h"
#include "SentenceTilesController.h"
#include "SentenceTrainingController.h"

#include "core/FCardSelectorBase.h"
#include "core/FTAnswerComparison.h"
#include "core/FTCardMeaning.h"
#include "core/FTEnglishForms.h"
#include "core/FTLog.h"
#include "core/FTSentenceCards.h"
#include "core/sessions/FCardState.h"

#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <bit>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
    const std::string MeaningSentenceCardPrefix = "meaning:";

    constexpr size_t MeaningCheckSpread = 4;
    constexpr size_t PhraseCandidateLimit = 40;

    using FSentencesByCard = std::unordered_map<std::string, FTTreeSentence>;

    std::string MakeTextKey(const std::string& Text)
    {
        return QString::fromStdString(Text).simplified().toLower().toStdString();
    }

    struct FSentencePicks
    {
        std::vector<FTCardHandle> Cards;
        FSentencesByCard Sentences;
        std::unordered_map<std::string, std::string> WordByCard;
        std::unordered_set<std::string> UsedTexts;
        std::unordered_set<std::string> CoveredCardIds;
        int MeaningCount = 0;
        int KnownWordCount = 0;
        int PhraseCount = 0;

        bool Add(FTCard Card, FTTreeSentence Sentence, const std::string& WordCardId)
        {
            if (!UsedTexts.insert(MakeTextKey(Sentence.Text)).second)
            {
                return false;
            }

            WordByCard.emplace(Card.Id, WordCardId);
            Sentences.emplace(Card.Id, std::move(Sentence));
            Cards.push_back(MakeCardHandle(std::move(Card)));

            return true;
        }
    };

    const FTDictionarySense* FindFirstMeaningWithExample(const std::vector<FTDictionarySense>& Meanings)
    {
        const auto Found = std::find_if(Meanings.begin(), Meanings.end(),
                                        [](const FTDictionarySense& Meaning) { return !Meaning.Example.empty(); });

        return Found != Meanings.end() ? &*Found : nullptr;
    }

    std::optional<FTTreeSentence> FindMeaningSentence(const FTCard& WordCard, const FTDictionaryRepository& Dictionary)
    {
        const std::vector<FTDictionarySense> Meanings = Dictionary.LoadMeanings(WordCard.TargetWord, WordCard.PartOfSpeech);
        const FTDictionarySense* Meaning = WordCard.MeaningGist.empty() ? FindFirstMeaningWithExample(Meanings)
                                                                        : FindCardMeaning(WordCard, Meanings);

        if (Meaning == nullptr || Meaning->Example.empty())
        {
            return std::nullopt;
        }

        FTTreeSentence Sentence = MakeMeaningSentence(*Meaning, {WordCard.TargetWord}, WordCard.PartOfSpeech == "noun");

        if (const char* Reason = FindUnusableSentenceReason(Sentence))
        {
            qCDebug(LogSession) << "Sentence stage: the example of" << QString::fromStdString(WordCard.TargetWord)
                                << "is left out -" << Reason;

            return std::nullopt;
        }

        return Sentence;
    }

    void PickMeaningSentences(const FCardState& WordCards, FSentencePicks& Picks)
    {
        const FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();

        if (!Dictionary.HasMeaningExamples())
        {
            return;
        }

        for (int Index = 0; Index < WordCards.GetCount(); ++Index)
        {
            const FTCard* WordCard = WordCards.GetCard(Index);

            if (WordCard == nullptr)
            {
                continue;
            }

            std::optional<FTTreeSentence> Sentence = FindMeaningSentence(*WordCard, Dictionary);

            if (!Sentence.has_value())
            {
                continue;
            }

            FTCard Card = MakeSentenceCard(*Sentence);
            Card.Id = MeaningSentenceCardPrefix + WordCard->Id;

            if (Picks.Add(std::move(Card), std::move(*Sentence), WordCard->Id))
            {
                ++Picks.MeaningCount;
                Picks.CoveredCardIds.insert(WordCard->Id);
            }
        }
    }

    std::vector<std::string> ListCardTranslations(const FTCard& WordCard, const FTDictionaryRepository& Dictionary)
    {
        std::vector<std::string> Translations = SplitTranslations(WordCard.Translation);

        if (WordCard.MeaningGist.empty())
        {
            return Translations;
        }

        const std::vector<FTDictionarySense> Meanings = Dictionary.LoadMeanings(WordCard.TargetWord, WordCard.PartOfSpeech);

        if (const FTDictionarySense* Meaning = FindCardMeaning(WordCard, Meanings))
        {
            for (const FTDictionaryTranslation& Translation : Meaning->Translations)
            {
                Translations.push_back(Translation.Text);
            }
        }

        return Translations;
    }

    struct FKnownWordCandidates
    {
        std::vector<std::vector<FTTreeSentence>> ByWord;
        int MeaningMismatchCount = 0;
    };

    FKnownWordCandidates FindKnownWordSentences(const FCardState& WordCards, const std::vector<bool>& Skipped)
    {
        FKnownWordCandidates Result;

        const FTSentenceTree* Tree = AppDictionary::Get().GetSentenceTree();

        if (Tree == nullptr)
        {
            return Result;
        }

        const FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();

        FTWordIdSet Known = BuildKnownSentenceWords(*Tree);

        const bool bChecksMeaning = Dictionary.GetNativeLanguage() == "en";

        std::vector<std::vector<uint32_t>> FocusWordIds;
        std::vector<std::vector<std::string>> TranslationsByWord;
        std::vector<uint32_t> SessionVerbs;

        for (int Index = 0; Index < WordCards.GetCount(); ++Index)
        {
            const FTCard* WordCard = WordCards.GetCard(Index);
            std::vector<uint32_t> WordIds = WordCard != nullptr ? Dictionary.ListWordIds(WordCard->TargetWord)
                                                                : std::vector<uint32_t>{};

            for (const uint32_t WordId : WordIds)
            {
                Known.Add(WordId);

                if (WordCard->PartOfSpeech == "verb")
                {
                    SessionVerbs.push_back(WordId);
                }
            }

            if (Skipped[static_cast<size_t>(Index)])
            {
                WordIds.clear();
            }

            FocusWordIds.push_back(std::move(WordIds));
            TranslationsByWord.push_back(bChecksMeaning && WordCard != nullptr ? ListCardTranslations(*WordCard, Dictionary)
                                                                               : std::vector<std::string>{});
        }

        AddPresentTenseKeys(*Tree, SessionVerbs, Known);

        const size_t CandidatesPerWord = std::max<size_t>(1, static_cast<size_t>(WordCards.GetCount()) / 2)
            * (bChecksMeaning ? MeaningCheckSpread : 1);

        std::vector<FTFocusedSentence> Focused = Tree->FindFocusedSentences(Known, FocusWordIds);

        std::shuffle(Focused.begin(), Focused.end(), std::mt19937{MakeRandomSeed()});
        std::stable_sort(Focused.begin(), Focused.end(),
                         [](const FTFocusedSentence& Left, const FTFocusedSentence& Right)
                         {
                             return std::popcount(Left.FocusMask) > std::popcount(Right.FocusMask);
                         });

        std::vector<std::vector<uint32_t>> RankedByWord(FocusWordIds.size());
        std::vector<uint32_t> ToLoad;

        for (const FTFocusedSentence& Candidate : Focused)
        {
            bool bWanted = false;

            for (size_t Word = 0; Word < RankedByWord.size(); ++Word)
            {
                if ((Candidate.FocusMask & (uint64_t{1} << Word)) != 0 && RankedByWord[Word].size() < CandidatesPerWord)
                {
                    RankedByWord[Word].push_back(Candidate.SentenceId);
                    bWanted = true;
                }
            }

            if (bWanted)
            {
                ToLoad.push_back(Candidate.SentenceId);
            }
        }

        std::unordered_map<uint32_t, FTTreeSentence> Loaded;

        for (FTTreeSentence& Sentence : Dictionary.LoadTreeSentences(ToLoad))
        {
            if (FindUnusableSentenceReason(Sentence) == nullptr)
            {
                const uint32_t Id = Sentence.Id;
                Loaded.emplace(Id, std::move(Sentence));
            }
        }

        Result.ByWord.resize(RankedByWord.size());

        for (size_t Word = 0; Word < RankedByWord.size(); ++Word)
        {
            for (const uint32_t SentenceId : RankedByWord[Word])
            {
                const auto Found = Loaded.find(SentenceId);

                if (Found == Loaded.end())
                {
                    continue;
                }

                if (bChecksMeaning && !MentionsTranslation(Found->second.Translation, TranslationsByWord[Word]))
                {
                    ++Result.MeaningMismatchCount;

                    continue;
                }

                Result.ByWord[Word].push_back(Found->second);
            }
        }

        return Result;
    }

    void PickKnownWordSentences(const FCardState& WordCards, FSentencePicks& Picks)
    {
        std::vector<bool> Skipped(static_cast<size_t>(WordCards.GetCount()), false);

        int ExampleCoveredCount = 0;

        for (int Index = 0; Index < WordCards.GetCount(); ++Index)
        {
            const FTCard* WordCard = WordCards.GetCard(Index);

            if (WordCard != nullptr && Picks.Sentences.contains(MeaningSentenceCardPrefix + WordCard->Id))
            {
                Skipped[static_cast<size_t>(Index)] = true;
                ++ExampleCoveredCount;
            }
        }

        const FKnownWordCandidates Candidates = FindKnownWordSentences(WordCards, Skipped);

        std::vector<size_t> WordOrder(Candidates.ByWord.size());

        for (size_t Word = 0; Word < WordOrder.size(); ++Word)
        {
            WordOrder[Word] = Word;
        }

        std::stable_sort(WordOrder.begin(), WordOrder.end(),
                         [&Candidates](size_t Left, size_t Right)
                         {
                             return Candidates.ByWord[Left].size() < Candidates.ByWord[Right].size();
                         });

        for (const size_t Word : WordOrder)
        {
            const std::string& WordCardId = WordCards.GetCard(static_cast<int>(Word))->Id;

            for (const FTTreeSentence& Sentence : Candidates.ByWord[Word])
            {
                if (Picks.Add(MakeSentenceCard(Sentence), Sentence, WordCardId))
                {
                    ++Picks.KnownWordCount;
                    Picks.CoveredCardIds.insert(WordCardId);

                    break;
                }
            }
        }

        qCDebug(LogSession) << "Sentence stage:" << ExampleCoveredCount << "words already have a meaning example,"
                            << Candidates.MeaningMismatchCount
                            << "known-word sentences left out, their translation misses the card's meaning";
    }

    bool IsPhrase(const std::string& Word)
    {
        return Word.find_first_of(" '") != std::string::npos || Word.find("’") != std::string::npos;
    }

    void PickPhraseSentences(const FCardState& WordCards, FSentencePicks& Picks)
    {
        const FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();
        const bool bChecksMeaning = Dictionary.GetNativeLanguage() == "en";

        std::vector<const FTCard*> PhraseCards;
        std::vector<std::string> Phrases;

        for (int Index = 0; Index < WordCards.GetCount(); ++Index)
        {
            const FTCard* WordCard = WordCards.GetCard(Index);

            if (WordCard != nullptr && IsPhrase(WordCard->TargetWord) && !Picks.CoveredCardIds.contains(WordCard->Id))
            {
                PhraseCards.push_back(WordCard);
                Phrases.push_back(WordCard->TargetWord);
            }
        }

        if (PhraseCards.empty())
        {
            return;
        }

        std::vector<std::vector<FTTreeSentence>> Found = Dictionary.FindSentencesWithPhrases(Phrases, PhraseCandidateLimit);
        std::mt19937 Generator{MakeRandomSeed()};

        int UncoveredCount = 0;

        for (size_t Index = 0; Index < PhraseCards.size(); ++Index)
        {
            const std::vector<std::string> Translations = bChecksMeaning ? ListCardTranslations(*PhraseCards[Index], Dictionary)
                                                                         : std::vector<std::string>{};
            std::vector<FTTreeSentence>& Candidates = Found[Index];

            std::erase_if(Candidates, [&Translations](const FTTreeSentence& Sentence)
            {
                return FindUnusableSentenceReason(Sentence) != nullptr
                    || (!Translations.empty() && !MentionsTranslation(Sentence.Translation, Translations));
            });

            std::shuffle(Candidates.begin(), Candidates.end(), Generator);
            std::stable_sort(Candidates.begin(), Candidates.end(),
                             [](const FTTreeSentence& Left, const FTTreeSentence& Right) { return Left.Level < Right.Level; });

            const std::string& PhraseCardId = PhraseCards[Index]->Id;

            const auto Picked = std::find_if(Candidates.begin(), Candidates.end(),
                                             [&Picks, &PhraseCardId](const FTTreeSentence& Sentence)
            {
                return Picks.Add(MakeSentenceCard(Sentence), Sentence, PhraseCardId);
            });

            if (Picked == Candidates.end())
            {
                ++UncoveredCount;

                continue;
            }

            ++Picks.PhraseCount;
            Picks.CoveredCardIds.insert(PhraseCards[Index]->Id);
        }

        qCDebug(LogSession) << "Sentence stage:" << PhraseCards.size() << "phrases searched in sentence text,"
                            << UncoveredCount << "left without a sentence";
    }
}

FGapSentences PickGapSentences(const FCardState& WordCards)
{
    FGapSentences Result;

    if (!AppDictionary::Get().IsReady())
    {
        return Result;
    }

    const FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();
    const FKnownWordCandidates Candidates =
        FindKnownWordSentences(WordCards, std::vector<bool>(static_cast<size_t>(WordCards.GetCount()), false));

    int KnownWordCount = 0;
    int MeaningCount = 0;

    for (int Index = 0; Index < WordCards.GetCount(); ++Index)
    {
        const FTCard* WordCard = WordCards.GetCard(Index);

        if (WordCard == nullptr)
        {
            continue;
        }

        const std::vector<std::string> Spellings{WordCard->TargetWord};
        const size_t Word = static_cast<size_t>(Index);

        if (Word < Candidates.ByWord.size())
        {
            for (FTTreeSentence Sentence : Candidates.ByWord[Word])
            {
                if (!Sentence.Translation.empty() && PlaceGap(Sentence, Spellings, WordCard->PartOfSpeech == "noun"))
                {
                    Result.emplace(WordCard->Id, std::move(Sentence));
                    ++KnownWordCount;

                    break;
                }
            }
        }

        if (Result.contains(WordCard->Id) || !Dictionary.HasMeaningExamples())
        {
            continue;
        }

        std::optional<FTTreeSentence> Example = FindMeaningSentence(*WordCard, Dictionary);

        if (Example.has_value() && !Example->Translation.empty() && HasGap(*Example))
        {
            Result.emplace(WordCard->Id, std::move(*Example));
            ++MeaningCount;
        }
    }

    qCInfo(LogSession) << "Gap sentences:" << KnownWordCount << "known-word sentences and" << MeaningCount
                       << "meaning examples for" << WordCards.GetCount() << "words";

    return Result;
}

FSessionSentenceStage BuildSessionSentenceStage(QObject* Parent, const FTSessionStartParams& Params,
                                                SessionSettings& Options, std::vector<FSessionSentence> Saved)
{
    FSessionSentenceStage Built;

    if (!Options.GetRules().bSentenceStage)
    {
        qCInfo(LogSession) << "Sentence stage: turned off in settings";

        return Built;
    }

    if (!Saved.empty())
    {
        qCInfo(LogSession) << "Sentence stage: rebuilt from" << Saved.size() << "saved sentences";

        Built.Sentences = std::move(Saved);
    }
    else
    {
        if (!Params.CardState || !AppDictionary::Get().IsReady())
        {
            return Built;
        }

        FSentencePicks Picks;

        PickMeaningSentences(*Params.CardState, Picks);
        PickKnownWordSentences(*Params.CardState, Picks);
        PickPhraseSentences(*Params.CardState, Picks);

        qCInfo(LogSession) << "Sentence stage:" << Picks.MeaningCount << "meaning examples," << Picks.KnownWordCount
                           << "known-word sentences and" << Picks.PhraseCount << "phrase sentences for"
                           << Params.CardState->GetCount() << "words";

        std::shuffle(Picks.Cards.begin(), Picks.Cards.end(), std::mt19937{MakeRandomSeed()});

        for (const FTCardHandle& Card : Picks.Cards)
        {
            Built.Sentences.push_back(
                FSessionSentence{Card, Picks.Sentences.at(Card->Id), Picks.WordByCard.at(Card->Id)});
        }
    }

    if (Built.Sentences.empty())
    {
        return Built;
    }

    FSentencesByCard SentencesByCard;

    for (const FSessionSentence& Picked : Built.Sentences)
    {
        SentencesByCard.emplace(Picked.Card->Id, Picked.Sentence);
    }

    const auto Sentences = std::make_shared<const FSentencesByCard>(std::move(SentencesByCard));

    for (const bool bBuildsTranslation : {false, true})
    {
        auto* Tiles = new SentenceTilesController(Parent);
        Tiles->SetTypingAllowed(!bBuildsTranslation);
        Tiles->SetBuildsTranslation(bBuildsTranslation);
        Tiles->SetSessionSettings(&Options);
        Tiles->SetSpeechRateScale(SentenceTrainingController::SentenceSpeechRateScale);
        Tiles->SetSentenceSource(
            [Sentences](const FTCard& Card)
            {
                const auto Found = Sentences->find(Card.Id);

                return Found != Sentences->end() ? Found->second : FTTreeSentence{};
            });

        Built.Controllers.push_back(Tiles);
    }

    return Built;
}

namespace
{
    QJsonObject TreeSentenceToJson(const FTTreeSentence& Sentence)
    {
        QJsonObject Saved;
        Saved["id"] = static_cast<qint64>(Sentence.Id);
        Saved["text"] = QString::fromStdString(Sentence.Text);
        Saved["translation"] = QString::fromStdString(Sentence.Translation);
        Saved["gapStart"] = Sentence.GapStart;
        Saved["gapLength"] = Sentence.GapLength;
        Saved["level"] = Sentence.Level;
        Saved["changedCount"] = Sentence.ChangedCount;

        return Saved;
    }

    FTTreeSentence TreeSentenceFromJson(const QJsonObject& Object)
    {
        FTTreeSentence Sentence;
        Sentence.Id = static_cast<uint32_t>(Object["id"].toInteger());
        Sentence.Text = Object["text"].toString().toStdString();
        Sentence.Translation = Object["translation"].toString().toStdString();
        Sentence.GapStart = Object["gapStart"].toInt(-1);
        Sentence.GapLength = Object["gapLength"].toInt(0);
        Sentence.Level = Object["level"].toInt(1);
        Sentence.ChangedCount = Object["changedCount"].toInt(0);

        return Sentence;
    }
}

QJsonArray SessionSentencesToJson(const std::vector<FSessionSentence>& Sentences)
{
    QJsonArray Result;

    for (const FSessionSentence& Picked : Sentences)
    {
        QJsonObject Saved = TreeSentenceToJson(Picked.Sentence);
        Saved["cardId"] = QString::fromStdString(Picked.Card->Id);
        Saved["wordCardId"] = QString::fromStdString(Picked.WordCardId);

        Result.append(Saved);
    }

    return Result;
}

QJsonObject GapSentencesToJson(const FGapSentences& Sentences)
{
    QJsonObject Result;

    for (const auto& [CardId, Sentence] : Sentences)
    {
        Result[QString::fromStdString(CardId)] = TreeSentenceToJson(Sentence);
    }

    return Result;
}

FGapSentences GapSentencesFromJson(const QJsonObject& Saved)
{
    FGapSentences Result;

    for (auto It = Saved.constBegin(); It != Saved.constEnd(); ++It)
    {
        FTTreeSentence Sentence = TreeSentenceFromJson(It.value().toObject());

        if (!Sentence.Text.empty())
        {
            Result.emplace(It.key().toStdString(), std::move(Sentence));
        }
    }

    return Result;
}

std::vector<FSessionSentence> SessionSentencesFromJson(const QJsonArray& Saved)
{
    std::vector<FSessionSentence> Result;
    Result.reserve(static_cast<size_t>(Saved.size()));

    for (const QJsonValue& Value : Saved)
    {
        const QJsonObject Object = Value.toObject();

        FTTreeSentence Sentence = TreeSentenceFromJson(Object);

        FTCard Card = MakeSentenceCard(Sentence);
        Card.Id = Object["cardId"].toString().toStdString();

        std::string WordCardId = Object["wordCardId"].toString().toStdString();

        if (WordCardId.empty() && Card.Id.starts_with(MeaningSentenceCardPrefix))
        {
            WordCardId = Card.Id.substr(MeaningSentenceCardPrefix.size());
        }

        if (Card.Id.empty() || Sentence.Text.empty() || WordCardId.empty())
        {
            qCWarning(LogSession) << "Sentence stage: saved sentences dropped, one has no card id, text or word";

            return {};
        }

        Result.push_back(FSessionSentence{MakeCardHandle(std::move(Card)), std::move(Sentence), std::move(WordCardId)});
    }

    return Result;
}
