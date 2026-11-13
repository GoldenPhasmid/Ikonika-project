#include "TriageController.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppSpeech.h"
#include "AppWebSearch.h"
#include "CefrFormatting.h"

#include "core/FCardSelectorBase.h"
#include "core/FTCard.h"
#include "core/FTCardMeaning.h"
#include "core/FTDeckProgress.h"
#include "core/FTWordForms.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"

#include <QSettings>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <limits>
#include <cstddef>
#include <vector>

namespace
{
    const QString AddedListOpenKey = QStringLiteral("triage/addedListOpen");
    const QString RangePaneOpenKey = QStringLiteral("triage/rangePaneOpen");

    bool StorePaneOpen(const QString& Key, bool bOpen, const char* PaneName)
    {
        if (FTSettings::Get().OpenStore().value(Key, false).toBool() == bOpen)
        {
            return false;
        }

        FTSettings::Get().OpenStore().setValue(Key, bOpen);

        qCInfo(LogApp) << "Triage:" << PaneName << (bOpen ? "opened" : "closed");

        return true;
    }

    constexpr int CandidateBatchSize = 50;
    
    constexpr std::size_t ShuffleWindowWords = 6;
    
    const char* const LearnableParts[] = { "noun", "adj", "verb", "adv",
                                           "pron", "prep", "conj", "intj" };
    
    constexpr int GradeCount = 7;
    constexpr int UngradedIndex = 6;
    
    constexpr int MaximumTranslationOptions = 6;
    
    constexpr int MaxConsecutiveOffers = 4;

    constexpr int WordsAboveRankCeiling = 1000;

    QString FoldHeadword(const std::string& Headword)
    {
        return QString::fromStdString(Headword).toLower();
    }
}

TriageController::TriageController(QObject* Parent)
    : QObject(Parent)
{
}

bool TriageController::start(int WantedCount)
{
    return BeginRun(WantedCount);
}

void TriageController::ResetRun()
{
    Current = {};
    CurrentCursorIndex = -1;

    Cursors.clear();
    GradeIndex = 0;
    LastCursorIndex = -1;
    ConsecutiveOffers = 0;

    AddedCount = 0;
    AddedWords.clear();
    KnownCount = 0;
    SkippedCount = 0;

    SkippedHeadwordsThisRun.clear();
    CollectedHeadwordsThisRun.clear();

    bDeckMode = false;
    DeckId = 0;
    DeckQueue.clear();
    DeckCursor = 0;
    DeckGist.clear();
}

bool TriageController::startDeck(qint64 InDeckId)
{
    ResetRun();

    if (!AppDictionary::Get().IsReady() || !AppDatabase::Get().IsReady())
    {
        qCWarning(LogApp) << "Cannot triage a deck without both a dictionary and a database";

        emit progressChanged();
        emit currentChanged();

        return false;
    }

    bDeckMode = true;
    DeckId = InDeckId;

    std::vector<FTDeckWord> Words = AppDictionary::Get().GetRepository().LoadDeckWords(InDeckId);

    const std::vector<FTDeckWordProgress> Progress = MatchDeckWords(
        Words, AppCards::LoadCards(), AppDatabase::Get().GetCardRepository().ListKnownWords());

    for (std::size_t Index = 0; Index < Words.size(); ++Index)
    {
        if (!Progress[Index].Card && !Progress[Index].bKnown)
        {
            DeckQueue.push_back(std::move(Words[Index]));
        }
    }

    std::stable_sort(DeckQueue.begin(), DeckQueue.end(), [](const FTDeckWord& Left, const FTDeckWord& Right)
    {
        return Left.TeachingRank.value_or(std::numeric_limits<int>::max())
            < Right.TeachingRank.value_or(std::numeric_limits<int>::max());
    });

    TargetCount = static_cast<int>(DeckQueue.size());

    qCInfo(LogApp) << "Triage: deck" << InDeckId << "has" << TargetCount << "of"
                   << static_cast<int>(Words.size()) << "words left";

    SelectNextWord();
    AnnounceCurrentWord();

    return !Current.Headword.empty();
}

void TriageController::SelectNextDeckWord()
{
    while (DeckCursor < DeckQueue.size())
    {
        const FTDeckWord& Word = DeckQueue[DeckCursor++];

        if (SkippedHeadwordsThisRun.contains(FoldHeadword(Word.Headword)))
        {
            continue;
        }

        Current.Headword = Word.Headword;
        Current.PartOfSpeech = Word.PartOfSpeech;
        Current.Gender = Word.Gender;
        Current.Translation = Word.Translation;

        DeckGist = Word.Gist;

        return;
    }
}

bool TriageController::BeginRun(int WantedCount)
{
    ResetRun();

    TargetCount = std::max(WantedCount, 0);

    Random.seed(MakeRandomSeed());

    if (TargetCount == 0)
    {
        emit progressChanged();
        emit currentChanged();

        return false;
    }

    if (!AppDictionary::Get().IsReady() || !AppDatabase::Get().IsReady())
    {
        qCWarning(LogApp) << "Cannot triage without both a dictionary and a database";

        emit progressChanged();
        emit currentChanged();

        return false;
    }

    StartRank = LoadSavedStartRank();

    emit startRankChanged();

    BeginGrade(0);
    SelectNextWord();

    if (Current.Headword.empty() && StartRank > 0)
    {
        qCWarning(LogApp) << "Triage: nothing left after teaching rank" << StartRank
                          << "- this run starts from the most common words";

        StartRank = 0;

        emit startRankChanged();

        BeginGrade(0);
        SelectNextWord();
    }

    AnnounceCurrentWord();

    return !Current.Headword.empty();
}

bool TriageController::BeginGrade(int Index)
{
    Cursors.clear();
    GradeIndex = Index;
    LastCursorIndex = -1;
    ConsecutiveOffers = 0;

    if (Index >= GradeCount)
    {
        return false;
    }

    FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();

    // The grade's own composition, read once. This is where the mix comes from: a grade holding
    // 55% nouns and 24% verbs yields a session of the same shape, without anything here choosing
    // those numbers.
    const std::vector<FTPartOfSpeechCount> Composition =
        Dictionary.CountLearnableWords(MakeFilter(Index, {}));

    for (const char* const Part : LearnableParts)
    {
        Cursor Entry;
        Entry.PartOfSpeech = Part;

        for (const FTPartOfSpeechCount& Counted : Composition)
        {
            if (Counted.PartOfSpeech == Entry.PartOfSpeech)
            {
                Entry.Share = Counted.Count;
                break;
            }
        }

        // A part of speech the grade has none of is kept as a spent cursor rather than left out,
        // so the four stay in a fixed order and the collected tallies survive the grade change.
        Entry.bExhausted = Entry.Share <= 0;

        Cursors.push_back(std::move(Entry));
    }

    return std::any_of(Cursors.begin(), Cursors.end(),
                       [](const Cursor& Entry) { return Entry.Share > 0; });
}

bool TriageController::AdvanceGrade()
{
    // Collected tallies are deliberately carried across: they are the run's proportions, not the
    // grade's, so a run that crosses from A1 into A2 keeps the shape it had.
    std::vector<int> Collected;
    Collected.reserve(Cursors.size());

    for (const Cursor& Entry : Cursors)
    {
        Collected.push_back(Entry.Collected);
    }

    for (int Next = GradeIndex + 1; Next < GradeCount; ++Next)
    {
        if (!BeginGrade(Next))
        {
            continue;
        }

        for (std::size_t Index = 0; Index < Cursors.size() && Index < Collected.size(); ++Index)
        {
            Cursors[Index].Collected = Collected[Index];
        }

        return true;
    }

    Cursors.clear();

    return false;
}

FTLearnableWordFilter TriageController::MakeFilter(int InGrade,
                                                   const std::string& PartOfSpeech) const
{
    FTLearnableWordFilter Filter;
    Filter.PartOfSpeech = PartOfSpeech;
    Filter.AboveTeachingRank = StartRank;
    
    if (InGrade >= UngradedIndex)
    {
        Filter.bOnlyUngraded = true;
    }
    else
    {
        Filter.CefrLevel = InGrade;
    }

    return Filter;
}

int TriageController::PickCursor() const
{
    const auto Score = [](const Cursor& Entry)
    {
        return static_cast<double>(Entry.Share) / static_cast<double>(Entry.Collected + 1);
    };

    const auto Eligible = [this](std::size_t Index, bool bAllowRepeat)
    {
        const Cursor& Entry = Cursors[Index];

        if (Entry.Share <= 0 || (Entry.bExhausted && Entry.Queue.empty()))
        {
            return false;
        }

        return bAllowRepeat || static_cast<int>(Index) != LastCursorIndex
            || ConsecutiveOffers < MaxConsecutiveOffers;
    };

    // Two passes: first honouring the run cap, then ignoring it, so a cap can never end a run that
    // still has words in it.
    for (const bool bAllowRepeat : { false, true })
    {
        int Best = -1;

        for (std::size_t Index = 0; Index < Cursors.size(); ++Index)
        {
            if (!Eligible(Index, bAllowRepeat))
            {
                continue;
            }

            if (Best < 0 || Score(Cursors[Index]) > Score(Cursors[Best]))
            {
                Best = static_cast<int>(Index);
            }
        }

        if (Best >= 0)
        {
            return Best;
        }
    }

    return -1;
}

bool TriageController::FillCursor(Cursor& Target)
{
    if (!Target.Queue.empty())
    {
        return true;
    }

    if (Target.bExhausted)
    {
        return false;
    }

    FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();
    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();
    
    while (Target.Queue.empty())
    {
        const std::vector<FTLearnableWord> Candidates = Dictionary.LoadLearnableWords(
            CandidateBatchSize, Target.Offset, MakeFilter(GradeIndex, Target.PartOfSpeech));

        if (Candidates.empty())
        {
            Target.bExhausted = true;

            return false;
        }

        Target.Offset += static_cast<int>(Candidates.size());

        for (const FTLearnableWord& Candidate : Candidates)
        {
            if (!CardRepository.IsWordOwnedOrTriaged(Candidate.Headword, Candidate.PartOfSpeech))
            {
                Target.Queue.push_back(Candidate);
            }
        }
        
        std::vector<std::vector<FTLearnableWord>> Groups;

        for (FTLearnableWord& Candidate : Target.Queue)
        {
            if (Groups.empty() || Groups.back().front().Headword != Candidate.Headword)
            {
                Groups.push_back({});
            }

            Groups.back().push_back(std::move(Candidate));
        }

        for (std::size_t Start = 0; Start < Groups.size(); Start += ShuffleWindowWords)
        {
            const std::size_t End = std::min(Start + ShuffleWindowWords, Groups.size());

            std::shuffle(Groups.begin() + Start, Groups.begin() + End, Random);
        }

        Target.Queue.clear();

        for (std::vector<FTLearnableWord>& Group : Groups)
        {
            for (FTLearnableWord& Candidate : Group)
            {
                Target.Queue.push_back(std::move(Candidate));
            }
        }
    }

    return true;
}

void TriageController::LoadTranslationOptions()
{
    TranslationOptions.clear();

    if (Current.Headword.empty())
    {
        return;
    }

    FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();

    const std::vector<FTDictionaryTranslation> Offered = Dictionary.LoadTranslationsForReading(
        Current.Headword, Current.PartOfSpeech, Current.Gender, MaximumTranslationOptions);

    for (const FTDictionaryTranslation& Translation : Offered)
    {
        TranslationOption Option;
        Option.Text = Translation.Text;
        Option.Probability = Translation.Probability;

        TranslationOptions.push_back(std::move(Option));
    }
    
    if (TranslationOptions.empty() && !Current.Translation.empty())
    {
        TranslationOption Option;
        Option.Text = Current.Translation;

        TranslationOptions.push_back(std::move(Option));
    }

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();
    
    for (TranslationOption& Option : TranslationOptions)
    {
        Option.LearnedStage = CardRepository.FindWordTranslationStage(
            Current.Headword, Current.PartOfSpeech, Current.Gender, Option.Text).value_or(-1);
    }
    
    for (TranslationOption& Option : TranslationOptions)
    {
        if (CardRepository.IsWordUnseen(Current.Headword, Current.PartOfSpeech, Current.Gender, Option.Text))
        {
            Option.bSelected = true;

            return;
        }
    }

    // Every one of them is already known, which still has to leave a card that teaches something.
    if (!TranslationOptions.empty())
    {
        TranslationOptions.front().bSelected = true;
    }
}

void TriageController::LoadMeanings()
{
    Meanings.clear();

    if (Current.Headword.empty() || !AppDictionary::Get().IsReady())
    {
        return;
    }

    for (FTDictionarySense& Meaning :
         AppDictionary::Get().GetRepository().LoadMeanings(Current.Headword, Current.PartOfSpeech))
    {
        if (MeaningFitsGender(Meaning, Current.Gender))
        {
            Meanings.push_back(std::move(Meaning));
        }
    }

    SortMeaningsByUse(Meanings);

    if (DeckGist.empty())
    {
        return;
    }

    std::erase_if(Meanings, [this](const FTDictionarySense& Meaning) { return Meaning.Gist != DeckGist; });

    if (Meanings.empty())
    {
        qCWarning(LogApp) << "Triage: the dictionary has no meaning" << QString::fromStdString(DeckGist)
                          << "for the deck word" << QString::fromStdString(Current.Headword);
    }
}

QVariantList TriageController::GetMeaningOptions() const
{
    QVariantList Result;

    const FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    for (const FTDictionarySense& Meaning : Meanings)
    {
        QVariantMap Entry = AppCards::MakeMeaningEntry(Meaning);
        Entry["learnedStage"] = CardRepository.FindMeaningStage(Current.Headword, Current.PartOfSpeech, Current.Gender,
                                                       Meaning.Gist).value_or(-1);

        Result.append(Entry);
    }

    return Result;
}

void TriageController::toggleTranslation(int Index)
{
    if (Index < 0 || Index >= static_cast<int>(TranslationOptions.size()))
    {
        return;
    }

    TranslationOption& Option = TranslationOptions[static_cast<size_t>(Index)];

    if (bTranslationEdited)
    {
        for (TranslationOption& Entry : TranslationOptions)
        {
            Entry.bSelected = false;
        }

        Option.bSelected = true;
        bTranslationEdited = false;

        emit currentChanged();

        return;
    }

    if (Option.bSelected)
    {
        const int SelectedCount = static_cast<int>(std::count_if(
            TranslationOptions.begin(), TranslationOptions.end(),
            [](const TranslationOption& Entry) { return Entry.bSelected; }));

        // A card with nothing on its back teaches nothing, so the last one cannot be turned off.
        if (SelectedCount <= 1)
        {
            return;
        }
    }

    Option.bSelected = !Option.bSelected;

    emit currentChanged();
}

void TriageController::speakWord()
{
    if (Current.Headword.empty())
    {
        return;
    }

    qCInfo(LogSpeech) << "TriageController speaks on request:" << QString::fromStdString(Current.Headword);

    AppSpeech::Get().Speak(QString::fromStdString(Current.Headword),
                    QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage));
}

void TriageController::openWebSearch()
{
    AppWebSearch::Open(QString::fromStdString(Current.Headword));
}

QVariantList TriageController::GetTranslationOptions() const
{
    QVariantList Result;

    double Best = 0.0;
    for (const TranslationOption& Option : TranslationOptions)
    {
        Best = std::max(Best, Option.Probability.value_or(0.0));
    }

    QStringList BoxedTranslations;

    for (const QString& Part : GetTranslation().split(QLatin1Char(','), Qt::SkipEmptyParts))
    {
        BoxedTranslations.append(Part.trimmed().toLower());
    }

    for (const TranslationOption& Option : TranslationOptions)
    {
        QVariantMap Entry;
        Entry["text"] = QString::fromStdString(Option.Text);
        Entry["selected"] = BoxedTranslations.contains(
            QString::fromStdString(Option.Text).trimmed().toLower());
        Entry["hasProbability"] = Option.Probability.has_value();
        Entry["relative"] = Best > 0.0 ? Option.Probability.value_or(0.0) / Best : 0.0;
        Entry["learnedStage"] = Option.LearnedStage;

        Result.append(Entry);
    }

    return Result;
}

void TriageController::MoveToNextWord()
{
    SelectNextWord();
    AnnounceCurrentWord();
}

void TriageController::SelectNextWord()
{
    Current = {};
    CurrentCursorIndex = -1;
    TranslationOptions.clear();
    bTranslationEdited = false;
    DeckGist.clear();

    if (AddedCount >= TargetCount)
    {
        return;
    }

    if (bDeckMode)
    {
        SelectNextDeckWord();
        LoadTranslationOptions();
        LoadMeanings();

        return;
    }

    while (true)
    {
        const int Index = PickCursor();

        if (Index < 0)
        {
            if (!AdvanceGrade())
            {
                break;
            }

            continue;
        }

        if (!FillCursor(Cursors[Index]))
        {
            // Spent in this grade. PickCursor skips it from here, so this cannot spin.
            continue;
        }

        FTLearnableWord Candidate = std::move(Cursors[Index].Queue.front());
        Cursors[Index].Queue.erase(Cursors[Index].Queue.begin());
        
        const QString Folded = FoldHeadword(Candidate.Headword);

        if (SkippedHeadwordsThisRun.contains(Folded) || CollectedHeadwordsThisRun.contains(Folded))
        {
            continue;
        }

        Current = std::move(Candidate);
        CurrentCursorIndex = Index;

        ConsecutiveOffers = Index == LastCursorIndex ? ConsecutiveOffers + 1 : 1;
        LastCursorIndex = Index;

        break;
    }

    LoadTranslationOptions();
    LoadMeanings();
}

void TriageController::AnnounceCurrentWord()
{
    if (!Current.Headword.empty() && AppSpeech::Get().AllowsAutomaticSpeech())
    {
        qCInfo(LogSpeech) << "TriageController speaks on new word:" << GetWord();

        AppSpeech::Get().Speak(GetWord(),
                        QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage));
    }

    emit progressChanged();
    emit currentChanged();
}

void TriageController::markKnown()
{
    if (Current.Headword.empty())
    {
        return;
    }

    if (!AppDatabase::Get().GetCardRepository().MarkWordTriaged(Current.Headword, Current.PartOfSpeech,
                                                      Current.Gender))
    {
        qCWarning(LogApp) << "Could not record" << QString::fromStdString(Current.Headword)
                          << "as known";
    }

    ++KnownCount;

    MoveToNextWord();
}

void TriageController::markUnknown()
{
    if (Current.Headword.empty())
    {
        return;
    }

    FTCard Card;
    Card.TargetWord = Current.Headword;
    Card.Translation = GetTranslation().toStdString();
    Card.Gender = Current.Gender;
    Card.PartOfSpeech = Current.PartOfSpeech;
    Card.MeaningGist = DeckGist;

    AddCardAndAdvance(Card);
}

bool TriageController::learnMeaning(const QString& Gist, const QString& Translation)
{
    const QString Trimmed = Translation.trimmed();

    if (Current.Headword.empty() || Gist.isEmpty() || Trimmed.isEmpty())
    {
        return false;
    }

    FTCard Card;
    Card.TargetWord = Current.Headword;
    Card.Translation = Trimmed.toStdString();
    Card.Gender = Current.Gender;
    Card.PartOfSpeech = Current.PartOfSpeech;
    Card.MeaningGist = Gist.toStdString();

    SetUserEdited(Card, EFTCardField::Translation, true);

    qCInfo(LogApp) << "Triage: learning" << QString::fromStdString(Current.Headword) << "in the meaning" << Gist;

    return AddCardAndAdvance(Card);
}

bool TriageController::AddCardAndAdvance(const FTCard& Card)
{
    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    const std::optional<std::string> Added = CardRepository.AddCard(Card);

    if (!Added.has_value())
    {
        qCWarning(LogApp) << "Could not add" << QString::fromStdString(Current.Headword) << "to My Vocabulary";

        MoveToNextWord();

        return false;
    }

    CardRepository.MarkWordTriaged(Current.Headword, Current.PartOfSpeech, Current.Gender);

    CollectedHeadwordsThisRun.insert(FoldHeadword(Current.Headword));

    ++AddedCount;

    QVariantMap AddedWord;
    AddedWord[QStringLiteral("word")] = QString::fromStdString(GetDisplayHeadword(
        Card.TargetWord, Card.PartOfSpeech, FTSettings::Get().GetLanguageRules().TargetLanguage));
    AddedWord[QStringLiteral("translation")] = QString::fromStdString(Card.Translation);
    AddedWord[QStringLiteral("partOfSpeech")] = QString::fromStdString(Card.PartOfSpeech);
    AddedWords.append(AddedWord);

    if (CurrentCursorIndex >= 0 && CurrentCursorIndex < static_cast<int>(Cursors.size()))
    {
        ++Cursors[CurrentCursorIndex].Collected;
    }

    MoveToNextWord();

    return true;
}

void TriageController::markSkipped()
{
    if (Current.Headword.empty())
    {
        return;
    }
    
    SkippedHeadwordsThisRun.insert(FoldHeadword(Current.Headword));

    ++SkippedCount;

    MoveToNextWord();
}

QString TriageController::GetWord() const
{
    return QString::fromStdString(Current.Headword);
}

QString TriageController::GetDisplayWord() const
{
    return QString::fromStdString(GetDisplayHeadword(
        Current.Headword, Current.PartOfSpeech, FTSettings::Get().GetLanguageRules().TargetLanguage));
}

QString TriageController::GetPartOfSpeech() const
{
    return Current.Headword.empty() ? QString() : QString::fromStdString(Current.PartOfSpeech);
}

QString TriageController::GetGender() const
{
    if (Current.Headword.empty() || !Current.Gender.has_value())
    {
        return {};
    }

    if (*Current.Gender == GenderNeuter)
    {
        return tr("neuter");
    }

    return *Current.Gender == GenderMasculine ? tr("masculine") : tr("feminine");
}

int TriageController::GetHeadwordGender() const
{
    return Current.Headword.empty() ? -1 : Current.Gender.value_or(-1);
}

QString TriageController::GetCardMeta() const
{
    const QString Speech = GetPartOfSpeech();
    const QString Gender = GetGender();

    if (Speech.isEmpty() || Gender.isEmpty())
    {
        return Speech.isEmpty() ? Gender : Speech;
    }

    return QStringLiteral("%1 · %2").arg(Speech, Gender);
}

void TriageController::restart()
{
    if (bDeckMode)
    {
        startDeck(DeckId);

        return;
    }

    BeginRun(TargetCount);
}

QString TriageController::GetTranslation() const
{
    if (bTranslationEdited)
    {
        return TranslationOverride;
    }

    QStringList Selected;

    for (const TranslationOption& Option : TranslationOptions)
    {
        if (Option.bSelected)
        {
            Selected.append(QString::fromStdString(Option.Text));
        }
    }
    
    if (!Selected.isEmpty())
    {
        return Selected.join(QStringLiteral(", "));
    }

    return QString::fromStdString(Current.Translation);
}

void TriageController::SetTranslation(const QString& Text)
{
    bTranslationEdited = true;
    TranslationOverride = Text;

    emit currentChanged();
}

int TriageController::GetAddedCount() const
{
    return AddedCount;
}

QVariantList TriageController::GetAddedWords() const
{
    return AddedWords;
}

void TriageController::refreshStartRank()
{
    emit startRankChanged();
}

int TriageController::GetStartRank() const
{
    if (TargetCount > 0 && AddedCount < TargetCount)
    {
        return StartRank;
    }

    return LoadSavedStartRank();
}

int TriageController::LoadSavedStartRank() const
{
    if (!AppDatabase::Get().IsReady())
    {
        return 0;
    }

    const int Saved = AppDatabase::Get().GetCardRepository().GetPlacementCutoff();
    const int Ceiling = GetMaximumRank();

    return Ceiling > 0 ? std::min(Saved, Ceiling) : Saved;
}

void TriageController::SetStartRank(int Rank)
{
    const int Clamped = std::clamp(Rank, 0, std::max(GetMaximumRank(), 0));

    if (bDeckMode || Clamped == GetStartRank() || !AppDatabase::Get().IsReady())
    {
        return;
    }

    const std::string TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;

    if (!AppDatabase::Get().GetCardRepository().SetPlacementCutoff(TargetLanguage, Clamped))
    {
        qCWarning(LogApp) << "Triage: could not save the start rank" << Clamped;

        return;
    }

    const int Previous = StartRank;

    StartRank = Clamped;

    qCInfo(LogApp) << "Triage: words now start after teaching rank" << Clamped;

    emit startRankChanged();

    if (TargetCount <= 0 || AddedCount >= TargetCount || !AppDictionary::Get().IsReady())
    {
        return;
    }

    BeginGrade(0);
    SelectNextWord();

    if (Current.Headword.empty() && Previous != Clamped)
    {
        qCWarning(LogApp) << "Triage: nothing left after teaching rank" << Clamped << "- back to" << Previous;

        AppDatabase::Get().GetCardRepository().SetPlacementCutoff(TargetLanguage, Previous);
        StartRank = Previous;

        emit startRankChanged();

        BeginGrade(0);
        SelectNextWord();
    }

    AnnounceCurrentWord();
}

int TriageController::GetMaximumRank() const
{
    return AppDictionary::Get().IsReady()
        ? AppDictionary::Get().GetRepository().GetTeachingRankCeiling(WordsAboveRankCeiling) : 0;
}

QVariantList TriageController::GetLevelMarks() const
{
    QVariantList Result;

    if (!AppDictionary::Get().IsReady())
    {
        return Result;
    }

    for (const FTLevelRank& Mark : AppDictionary::Get().GetRepository().LoadLevelTeachingRanks())
    {
        QVariantMap Entry;
        Entry[QStringLiteral("label")] = CefrLetter(Mark.Level);
        Entry[QStringLiteral("rank")] = Mark.TeachingRank;

        Result.append(Entry);
    }

    return Result;
}

bool TriageController::IsAddedListOpen() const
{
    return FTSettings::Get().OpenStore().value(AddedListOpenKey, false).toBool();
}

void TriageController::SetAddedListOpen(bool bOpen)
{
    if (StorePaneOpen(AddedListOpenKey, bOpen, "added words list"))
    {
        emit addedListOpenChanged();
    }
}

bool TriageController::IsRangePaneOpen() const
{
    return FTSettings::Get().OpenStore().value(RangePaneOpenKey, false).toBool();
}

void TriageController::SetRangePaneOpen(bool bOpen)
{
    if (StorePaneOpen(RangePaneOpenKey, bOpen, "word range pane"))
    {
        emit rangePaneOpenChanged();
    }
}

int TriageController::GetTargetCount() const
{
    return TargetCount;
}

int TriageController::GetKnownCount() const
{
    return KnownCount;
}

int TriageController::GetSkippedCount() const
{
    return SkippedCount;
}

bool TriageController::IsFinished() const
{
    return AddedCount >= TargetCount || Current.Headword.empty();
}

bool TriageController::IsDeckMode() const
{
    return bDeckMode;
}

int TriageController::GetProgressCount() const
{
    return bDeckMode ? AddedCount + KnownCount + SkippedCount : AddedCount;
}
