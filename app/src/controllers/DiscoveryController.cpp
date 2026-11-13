#include "DiscoveryController.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppSpeech.h"
#include "SentenceSelection.h"

#include "core/FTCard.h"
#include "core/FTCardMeaning.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QVariantMap>

#include <algorithm>
#include <map>
#include <random>
#include <set>

namespace
{
    constexpr int ScarcePartOfSpeechRows = 12;
    constexpr int ExtraDistractorRows = 60;
    constexpr int ShownTranslationCount = 3;
    constexpr int MaximumTranslationOptions = 6;


    QString TargetLanguageCode()
    {
        return QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage);
    }

    QString SavedDecksKey()
    {
        return QStringLiteral("discover/decks/") + TargetLanguageCode();
    }

    QString OutcomeName(EFTDiscoveryOutcome Outcome)
    {
        switch (Outcome)
        {
        case EFTDiscoveryOutcome::Known:
            return QStringLiteral("known");
        case EFTDiscoveryOutcome::Almost:
            return QStringLiteral("almost");
        case EFTDiscoveryOutcome::New:
            return QStringLiteral("new");
        }

        return {};
    }

    std::vector<FTDictionarySense> LoadRowMeanings(const FTDiscoveryRow& Row)
    {
        std::vector<FTDictionarySense> Meanings;

        if (!AppDictionary::Get().IsReady())
        {
            return Meanings;
        }

        for (FTDictionarySense& Meaning : AppDictionary::Get().GetRepository().LoadMeanings(Row.Headword, Row.PartOfSpeech))
        {
            if (MeaningFitsGender(Meaning, Row.Gender))
            {
                Meanings.push_back(std::move(Meaning));
            }
        }

        SortMeaningsByUse(Meanings);

        return Meanings;
    }

    std::vector<std::string> LoadRowForms(const FTDiscoveryRow& Row)
    {
        std::vector<std::string> Forms;

        if (!AppDictionary::Get().IsReady())
        {
            return Forms;
        }

        for (FTHeadwordForm& Form : AppDictionary::Get().GetRepository().ListHeadwordForms({Row.Headword}))
        {
            if (Form.PartOfSpeech == Row.PartOfSpeech)
            {
                Forms.push_back(std::move(Form.Form));
            }
        }

        return Forms;
    }

    std::function<bool(const FTDiscoveryRow&, const std::string&)> MakeKnownExampleCheck()
    {
        const FTSentenceTree* Tree = AppDictionary::Get().GetSentenceTree();

        if (Tree == nullptr)
        {
            return {};
        }

        auto Known = std::make_shared<std::optional<FTWordIdSet>>();

        return [Tree, Known](const FTDiscoveryRow& Row, const std::string& Example)
        {
            const FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();
            const std::vector<uint32_t> WordIds = Dictionary.ListWordIds(Row.Headword);

            if (WordIds.empty())
            {
                return false;
            }

            if (!*Known)
            {
                *Known = BuildKnownSentenceWords(*Tree);
            }

            FTWordIdSet Open = **Known;

            for (const uint32_t WordId : WordIds)
            {
                Open.Add(WordId);
            }

            if (Row.PartOfSpeech == "verb")
            {
                AddPresentTenseKeys(*Tree, WordIds, Open);
            }

            std::vector<uint32_t> SentenceIds;

            for (const FTFocusedSentence& Found : Tree->FindFocusedSentences(Open, {WordIds}))
            {
                SentenceIds.push_back(Found.SentenceId);
            }

            for (const FTTreeSentence& Sentence : Dictionary.LoadTreeSentences(SentenceIds))
            {
                if (Sentence.Text == Example)
                {
                    qCInfo(LogApp) << "Discover: every word of the example of" << QString::fromStdString(Row.Headword)
                                   << "but the word itself is known, asking it in French";

                    return true;
                }
            }

            return false;
        };
    }

    FTDiscoveryLearner LoadLearner()
    {
        const FTCardRepository& Cards = AppDatabase::Get().GetCardRepository();

        FTDiscoveryLearner Learner;
        Learner.CutoffRank = Cards.GetPlacementCutoff();
        Learner.KnownWordCount = Learner.CutoffRank + Cards.CountCards() - Cards.CountUnlearnedCards()
            + Cards.CountTriagedWords();

        return Learner;
    }

    std::vector<FTDiscoveryRow> LoadExtraDistractors(const std::vector<FTDiscoveryRow>& Rows)
    {
        std::map<std::string, int> PartCounts;

        for (const FTDiscoveryRow& Row : Rows)
        {
            ++PartCounts[Row.PartOfSpeech];
        }

        std::vector<FTDiscoveryRow> Extra;

        for (const auto& [PartOfSpeech, Count] : PartCounts)
        {
            if (Count >= ScarcePartOfSpeechRows)
            {
                continue;
            }

            FTLearnableWordFilter Filter;
            Filter.PartOfSpeech = PartOfSpeech;

            for (const FTLearnableWord& Word : AppDictionary::Get().GetRepository().LoadLearnableWords(ExtraDistractorRows, 0, Filter))
            {
                FTDiscoveryRow Row;
                Row.Headword = Word.Headword;
                Row.PartOfSpeech = Word.PartOfSpeech;
                Row.Gender = Word.Gender;
                Row.Translation = Word.Translation;
                Row.TeachingRank = Word.FrequencyRank;

                Extra.push_back(std::move(Row));
            }

            qCInfo(LogApp) << "Discover: the deck has" << Count << QString::fromStdString(PartOfSpeech)
                           << "words, so wrong options also come from" << ExtraDistractorRows << "dictionary words";
        }

        return Extra;
    }
}

DiscoveryController::DiscoveryController(QObject* Parent)
    : QObject(Parent)
{
}

bool DiscoveryController::startDeck(qint64 InDeckId)
{
    if (HasActiveSession() && DeckIds == std::vector<qint64>{InDeckId})
    {
        qCInfo(LogApp) << "Discover: continuing the round over deck" << InDeckId;

        return true;
    }

    return startDecks({InDeckId});
}

QVariantList DiscoveryController::savedDecks() const
{
    return FTSettings::Get().OpenStore().value(SavedDecksKey()).toList();
}

void DiscoveryController::saveDecks(const QVariantList& InDeckIds)
{
    qCInfo(LogApp) << "Discover: saving decks" << InDeckIds << "for" << TargetLanguageCode();

    FTSettings::Get().OpenStore().setValue(SavedDecksKey(), InDeckIds);
}

bool DiscoveryController::startDecks(const QVariantList& InDeckIds)
{
    if (HasActiveSession())
    {
        qCInfo(LogApp) << "Discover: dropping the unfinished round over decks" << DeckIds;
    }

    Planner.reset();
    Written.clear();
    DeckWords.clear();
    DeckIds.clear();
    DeckName.clear();

    for (const QVariant& Id : InDeckIds)
    {
        DeckIds.push_back(Id.toLongLong());
    }

    if (!AppDictionary::Get().IsReady() || !AppDatabase::Get().IsReady())
    {
        qCWarning(LogApp) << "Discover: cannot start without both a dictionary and a database";

        emit stateChanged();
        emit resultsChanged();

        return false;
    }

    const FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();
    const std::vector<FTDeck> Decks = Dictionary.ListDecks();

    QStringList DeckNames;
    std::vector<qint64> WordDeckIds;

    for (const qint64 Id : DeckIds)
    {
        const auto Found = std::find_if(Decks.begin(), Decks.end(), [Id](const FTDeck& Deck) { return Deck.Id == Id; });

        if (Found != Decks.end())
        {
            DeckNames.append(QString::fromStdString(Found->Name));
        }

        std::vector<FTDeckWord> Words = Dictionary.LoadDeckWords(Id);
        WordDeckIds.insert(WordDeckIds.end(), Words.size(), Id);
        DeckWords.insert(DeckWords.end(), std::make_move_iterator(Words.begin()), std::make_move_iterator(Words.end()));
    }

    DeckName = DeckNames.join(QStringLiteral(" · "));

    const std::vector<FTDeckWordProgress> Progress = MatchDeckWords(
        DeckWords, AppCards::LoadCards(), AppDatabase::Get().GetCardRepository().ListKnownWords());

    std::vector<FTDiscoveryRow> Rows;
    Rows.reserve(DeckWords.size());

    int AskableCount = 0;

    for (std::size_t Index = 0; Index < DeckWords.size(); ++Index)
    {
        const FTDeckWord& Word = DeckWords[Index];

        FTDiscoveryRow Row;
        Row.DeckId = WordDeckIds[Index];
        Row.Headword = Word.Headword;
        Row.PartOfSpeech = Word.PartOfSpeech;
        Row.Gist = Word.Gist;
        Row.Gender = Word.Gender;
        Row.Translation = Word.Translation;
        Row.TeachingRank = Word.TeachingRank.value_or(0);
        Row.bAskable = !Progress[Index].Card && !Progress[Index].bKnown;

        AskableCount += Row.bAskable ? 1 : 0;

        Rows.push_back(std::move(Row));
    }

    std::vector<FTDiscoveryRow> Extra = LoadExtraDistractors(Rows);
    Rows.insert(Rows.end(), std::make_move_iterator(Extra.begin()), std::make_move_iterator(Extra.end()));

    const FTDiscoveryLearner Learner = LoadLearner();

    FTDiscoverySources Sources;
    Sources.LoadMeanings = LoadRowMeanings;
    Sources.LoadForms = LoadRowForms;
    Sources.IsKnownExample = MakeKnownExampleCheck();

    Planner = std::make_unique<FTDiscoveryPlanner>(std::move(Rows), std::move(Sources), Learner,
                                                   FTDiscoveryPlanner::DefaultQuestionCount, std::random_device{}());

    qCInfo(LogApp) << "Discover: decks" << DeckIds << DeckName << "have" << AskableCount << "of"
                   << static_cast<int>(DeckWords.size()) << "words to ask, starting from cutoff" << Learner.CutoffRank
                   << "with" << Learner.KnownWordCount << "known words,"
                   << (Planner->IsBeginner() ? "in beginner mode" : "in standard mode");

    if (Planner->IsFinished())
    {
        qCInfo(LogApp) << "Discover: decks" << DeckIds << "have no word that can be asked";

        Planner.reset();

        emit stateChanged();
        emit resultsChanged();

        return false;
    }

    AnnounceQuestion();

    emit resultsChanged();

    return true;
}

bool DiscoveryController::restart()
{
    QVariantList Ids;

    for (const qint64 Id : DeckIds)
    {
        Ids.append(Id);
    }

    return startDecks(Ids);
}

void DiscoveryController::choose(int Index)
{
    if (!Planner || !GetQuestion() || GetQuestion()->Outcome)
    {
        return;
    }

    const int ClueStep = GetQuestion()->ClueStep;

    Planner->Choose(Index);

    const FTDiscoveryQuestion* Question = GetQuestion();

    if (Question->Outcome)
    {
        qCInfo(LogApp) << "Discover:" << GetWord() << "answered" << GetOutcome();
    }
    else if (Question->ClueStep != ClueStep)
    {
        qCInfo(LogApp) << "Discover: wrong answer on" << GetWord() << "gives clue" << Question->ClueStep;

        SpeakClue();
    }

    emit stateChanged();
    emit resultsChanged();
}

void DiscoveryController::askClue()
{
    if (!Planner || !GetQuestion() || GetQuestion()->Outcome)
    {
        return;
    }

    Planner->AskClue();

    qCInfo(LogApp) << "Discover: clue" << GetQuestion()->ClueStep << "on" << GetWord()
                   << (GetQuestion()->Outcome ? "shows the answer" : "");

    SpeakClue();

    emit stateChanged();
    emit resultsChanged();
}

void DiscoveryController::SpeakClue()
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    if (Question->ClueStep == 1 && Question->Prompt == EFTDiscoveryPrompt::Word && !Question->Outcome)
    {
        speakWord();
    }
}

void DiscoveryController::setTranslation(const QString& Translation)
{
    if (!Planner)
    {
        return;
    }

    Planner->SetTranslation(Planner->GetCurrentResultIndex(), Translation.toStdString());

    emit stateChanged();
    emit resultsChanged();
}

void DiscoveryController::commit(bool bLearn)
{
    if (!Planner || !GetQuestion() || !GetQuestion()->Outcome)
    {
        return;
    }

    Planner->SetLearn(Planner->GetCurrentResultIndex(), bLearn);

    qCInfo(LogApp) << "Discover:" << GetWord() << (bLearn ? "to learn" : "known");

    WriteResults();

    Planner->Advance();

    if (Planner->IsFinished())
    {
        qCInfo(LogApp) << "Discover: round over after" << Planner->GetAskedCount() << "questions,"
                       << GetLearnCount() << "to learn";

        emit stateChanged();
        emit resultsChanged();

        return;
    }

    AnnounceQuestion();

    emit resultsChanged();
}

void DiscoveryController::leave()
{
    if (!Planner)
    {
        return;
    }

    qCInfo(LogApp) << "Discover: leaving decks" << DeckIds;

    WriteResults();
}

void DiscoveryController::restartStage()
{
    leave();
    restart();
}

void DiscoveryController::endSession()
{
    if (!Planner)
    {
        return;
    }

    leave();

    qCInfo(LogApp) << "Discover: ending the round over decks" << DeckIds;

    Planner.reset();
    Written.clear();

    emit stateChanged();
    emit resultsChanged();
}

void DiscoveryController::speakWord()
{
    const FTDiscoveryRow* Row = GetQuestionRow();

    if (!Row)
    {
        return;
    }

    AppSpeech::Get().Speak(FormatHeadword(*Row), TargetLanguageCode());
}

void DiscoveryController::speakSlowly()
{
    const FTDiscoveryRow* Row = GetQuestionRow();

    if (!Row)
    {
        return;
    }

    qCInfo(LogApp) << "Discover: slow speech requested for" << GetWord();

    AppSpeech::Get().SpeakAtRate(FormatHeadword(*Row), TargetLanguageCode(), AppSpeech::SlowRateScale);
}

void DiscoveryController::WriteResults()
{
    if (!Planner)
    {
        return;
    }

    for (std::size_t Index = 0; Index < Planner->GetResults().size(); ++Index)
    {
        WriteResult(Index);
    }
}

void DiscoveryController::WriteResult(std::size_t Index)
{
    if (Written.size() <= Index)
    {
        Written.resize(Index + 1);
    }

    const FTDiscoveryResult& Result = Planner->GetResults()[Index];
    const FTDiscoveryRow& Row = Planner->GetRows()[static_cast<std::size_t>(Result.RowIndex)];
    FWritten& State = Written[Index];

    FTCardRepository& Cards = AppDatabase::Get().GetCardRepository();

    if (Result.bLearn && !State.CardId)
    {
        if (State.bMarked && Cards.UnmarkWordTriaged(Row.Headword, Row.PartOfSpeech, Row.Gender, Result.Meaning.Gist))
        {
            State.bMarked = false;
        }

        FTCard Card;
        Card.TargetWord = Row.Headword;
        Card.Translation = QString::fromStdString(Result.Translation).trimmed().toStdString();

        if (Card.Translation.empty())
        {
            Card.Translation = CleanDiscoveryTranslation(Result.Meaning.Translations.front().Text);
        }
        Card.Gender = Row.Gender;
        Card.PartOfSpeech = Row.PartOfSpeech;
        Card.MeaningGist = Result.Meaning.Gist;

        SetUserEdited(Card, EFTCardField::Translation, true);

        State.CardId = Cards.AddCard(Card);

        if (!State.CardId)
        {
            qCWarning(LogApp) << "Discover: could not add" << QString::fromStdString(Row.Headword) << "to My Vocabulary";

            return;
        }

        qCInfo(LogApp) << "Discover: added" << QString::fromStdString(Row.Headword) << "in the meaning"
                       << QString::fromStdString(Result.Meaning.Gist) << "to My Vocabulary";

        return;
    }

    if (Result.bLearn || State.bMarked)
    {
        return;
    }

    if (State.CardId)
    {
        if (!Cards.DeleteCards({*State.CardId}))
        {
            qCWarning(LogApp) << "Discover: could not remove the card of" << QString::fromStdString(Row.Headword);

            return;
        }

        State.CardId.reset();
    }


    State.bMarked = Cards.MarkWordTriaged(Row.Headword, Row.PartOfSpeech, Row.Gender, Result.Meaning.Gist);

    if (!State.bMarked)
    {
        qCWarning(LogApp) << "Discover: could not record" << QString::fromStdString(Row.Headword) << "as known";

        return;
    }

    qCInfo(LogApp) << "Discover: marked" << QString::fromStdString(Row.Headword) << "known in the meaning"
                   << QString::fromStdString(Result.Meaning.Gist);
}

void DiscoveryController::AnnounceQuestion()
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    if (Question && AppSpeech::Get().AllowsAutomaticSpeech())
    {
        speakWord();
    }

    emit stateChanged();
}

const FTDiscoveryQuestion* DiscoveryController::GetQuestion() const
{
    return Planner ? Planner->GetCurrentQuestion() : nullptr;
}

const FTDiscoveryRow* DiscoveryController::GetQuestionRow() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question ? &Planner->GetRows()[static_cast<std::size_t>(Question->RowIndex)] : nullptr;
}

QString DiscoveryController::FormatHeadword(const FTDiscoveryRow& Row) const
{
    auto Card = std::make_shared<FTCard>();
    Card->TargetWord = Row.Headword;
    Card->PartOfSpeech = Row.PartOfSpeech;
    Card->Gender = Row.Gender;

    AppCards::Annotate(std::vector<FTCardHandle>{Card});

    return QString::fromStdString(::GetDisplayWord(*Card, FTSettings::Get().GetLanguageRules().TargetLanguage));
}

QObject* DiscoveryController::GetActiveController()
{
    return this;
}

bool DiscoveryController::IsSessionFinished() const
{
    return false;
}

int DiscoveryController::GetStage() const
{
    return 0;
}

int DiscoveryController::GetStageCount() const
{
    return 1;
}

QString DiscoveryController::GetTense() const
{
    return {};
}

float DiscoveryController::GetExerciseProgress() const
{
    if (!Planner || Planner->GetQuestionCount() <= 0)
    {
        return 0.0f;
    }

    if (!GetQuestion())
    {
        return 1.0f;
    }

    const int Done = Planner->GetAskedCount() - (IsAnswered() ? 0 : 1);

    return static_cast<float>(Done) / static_cast<float>(Planner->GetQuestionCount());
}

QVariantList DiscoveryController::GetForms() const
{
    const FTDiscoveryRow* Row = GetQuestionRow();

    if (!Row || !IsAnswered())
    {
        return {};
    }

    FTCard Card;
    Card.TargetWord = Row->Headword;
    Card.PartOfSpeech = Row->PartOfSpeech;
    Card.Gender = Row->Gender;

    return AppCards::BuildFormsTable(Card);
}

bool DiscoveryController::IsRoundOver() const
{
    return !GetQuestion();
}

bool DiscoveryController::HasActiveSession() const
{
    return Planner && !Planner->IsFinished();
}

QString DiscoveryController::GetDeckName() const
{
    return DeckName;
}

QString DiscoveryController::GetPromptKind() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    if (!Question)
    {
        return {};
    }

    switch (Question->Prompt)
    {
    case EFTDiscoveryPrompt::Word:
        return QStringLiteral("word");
    case EFTDiscoveryPrompt::Sentence:
        return QStringLiteral("sentence");
    case EFTDiscoveryPrompt::Beginner:
        return QStringLiteral("beginner");
    }

    return {};
}

QString DiscoveryController::GetWord() const
{
    const FTDiscoveryRow* Row = GetQuestionRow();

    return Row ? QString::fromStdString(Row->Headword) : QString();
}

QString DiscoveryController::GetDisplayWord() const
{
    const FTDiscoveryRow* Row = GetQuestionRow();

    return Row ? FormatHeadword(*Row) : QString();
}

QString DiscoveryController::GetArticle() const
{
    const QString Display = GetDisplayWord();
    const QString Word = GetWord();

    if (Word.isEmpty() || !Display.endsWith(Word) || Display.size() == Word.size())
    {
        return {};
    }

    return Display.left(Display.size() - Word.size());
}

QString DiscoveryController::GetPartOfSpeech() const
{
    const FTDiscoveryRow* Row = GetQuestionRow();

    return Row ? QString::fromStdString(Row->PartOfSpeech) : QString();
}

QString DiscoveryController::GetGender() const
{
    const FTDiscoveryRow* Row = GetQuestionRow();

    if (!Row || !Row->Gender.has_value())
    {
        return {};
    }

    if (*Row->Gender == GenderNeuter)
    {
        return tr("neuter");
    }

    return *Row->Gender == GenderMasculine ? tr("masculine") : tr("feminine");
}

int DiscoveryController::GetHeadwordGender() const
{
    const FTDiscoveryRow* Row = GetQuestionRow();

    return Row ? Row->Gender.value_or(-1) : -1;
}

QString DiscoveryController::GetPhonetic() const
{
    const FTDiscoveryRow* Row = GetQuestionRow();

    if (!Row || !AppDictionary::Get().IsReady())
    {
        return {};
    }

    return QString::fromStdString(AppDictionary::Get().GetRepository().FindPhonetic(Row->Headword, Row->PartOfSpeech));
}

QString DiscoveryController::GetTranslations() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    if (!Question)
    {
        return {};
    }

    QStringList Shown;

    for (const FTDictionaryTranslation& Translation : Question->Meaning.Translations)
    {
        const QString Text = QString::fromStdString(CleanDiscoveryTranslation(Translation.Text));

        if (!Text.isEmpty() && !Shown.contains(Text) && Shown.size() < ShownTranslationCount)
        {
            Shown.append(Text);
        }
    }

    return Shown.join(QStringLiteral(" · "));
}

QString DiscoveryController::GetDefinition() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    if (!Question)
    {
        return {};
    }

    const std::string& Definition = Question->Meaning.NativeDefinition.empty() ? Question->Meaning.Definition
                                                                                : Question->Meaning.NativeDefinition;

    return QString::fromStdString(Definition);
}

QString DiscoveryController::GetExample() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question ? QString::fromStdString(Question->Meaning.Example) : QString();
}

QString DiscoveryController::GetExampleTranslation() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question ? QString::fromStdString(Question->Meaning.ExampleTranslation) : QString();
}

int DiscoveryController::GetExampleSpanStart() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question ? Question->ExampleSpanStart : -1;
}

int DiscoveryController::GetExampleSpanLength() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question ? Question->ExampleSpanLength : 0;
}

QString DiscoveryController::GetBeginnerSentence() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question ? QString::fromStdString(Question->BeginnerSentence) : QString();
}

int DiscoveryController::GetBeginnerSpanStart() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question ? Question->BeginnerSpanStart : -1;
}

int DiscoveryController::GetBeginnerSpanLength() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question ? Question->BeginnerSpanLength : 0;
}

QStringList DiscoveryController::GetOptions() const
{
    QStringList Result;

    const FTDiscoveryQuestion* Question = GetQuestion();

    if (!Question)
    {
        return Result;
    }

    for (std::size_t Index = 0; Index < Question->Options.size(); ++Index)
    {
        const FTDiscoveryOption& Option = Question->Options[Index];

        QString Text = QString::fromStdString(Option.Text);

        if (Option.bOtherMeaning && Question->Chosen == static_cast<int>(Index))
        {
            Text = tr("%1 · another meaning").arg(Text);
        }

        Result.append(Text);
    }

    return Result;
}

QVariantList DiscoveryController::GetStruckOptions() const
{
    QVariantList Result;

    if (const FTDiscoveryQuestion* Question = GetQuestion())
    {
        for (const bool bStruck : Question->Struck)
        {
            Result.append(bStruck);
        }
    }

    return Result;
}

int DiscoveryController::GetCorrectOptionIndex() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    if (!Question)
    {
        return -1;
    }

    for (std::size_t Index = 0; Index < Question->Options.size(); ++Index)
    {
        if (Question->Options[Index].bCorrect)
        {
            return static_cast<int>(Index);
        }
    }

    return -1;
}

int DiscoveryController::GetChosenOptionIndex() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question ? Question->Chosen.value_or(-1) : -1;
}

bool DiscoveryController::IsAnswered() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question && Question->Outcome.has_value();
}

bool DiscoveryController::ShowsExample() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question && ShowsDiscoveryExample(*Question);
}

bool DiscoveryController::IsLastClue() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question && Question->ClueStep >= GetDiscoveryClueCount(Question->Prompt);
}

QString DiscoveryController::GetOutcome() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    return Question && Question->Outcome ? OutcomeName(*Question->Outcome) : QString();
}

bool DiscoveryController::IsLearn() const
{
    const int Index = Planner ? Planner->GetCurrentResultIndex() : -1;

    return Index >= 0 && Planner->GetResults()[static_cast<std::size_t>(Index)].bLearn;
}

QVariantMap DiscoveryController::GetMeaning() const
{
    const FTDiscoveryQuestion* Question = GetQuestion();

    QVariantMap Entry = Question ? AppCards::MakeMeaningEntry(Question->Meaning)
                                 : QVariantMap{ { QStringLiteral("gist"), QString() },
                                                { QStringLiteral("nativeDefinition"), QString() } };
    Entry[QStringLiteral("learnedStage")] = -1;

    return Entry;
}

QVariantList DiscoveryController::GetTranslationOptions() const
{
    QVariantList Result;

    const FTDiscoveryQuestion* Question = GetQuestion();

    if (!Question)
    {
        return Result;
    }

    double Best = 0.0;

    for (const FTDictionaryTranslation& Translation : Question->Meaning.Translations)
    {
        Best = std::max(Best, Translation.Probability.value_or(0.0));
    }

    for (const FTDictionaryTranslation& Translation : Question->Meaning.Translations)
    {
        if (Result.size() >= MaximumTranslationOptions)
        {
            break;
        }

        QVariantMap Entry;
        Entry[QStringLiteral("text")] = QString::fromStdString(Translation.Text);
        Entry[QStringLiteral("hasProbability")] = Translation.Probability.has_value();
        Entry[QStringLiteral("relative")] = Best > 0.0 ? Translation.Probability.value_or(0.0) / Best : 0.0;
        Entry[QStringLiteral("learnedStage")] = -1;

        Result.append(Entry);
    }

    return Result;
}

QString DiscoveryController::GetTranslation() const
{
    const int Index = Planner ? Planner->GetCurrentResultIndex() : -1;

    return Index >= 0 ? QString::fromStdString(Planner->GetResults()[static_cast<std::size_t>(Index)].Translation) : QString();
}

int DiscoveryController::GetAskedCount() const
{
    return Planner ? Planner->GetAskedCount() : 0;
}

int DiscoveryController::GetQuestionCount() const
{
    return Planner ? Planner->GetQuestionCount() : 0;
}

bool DiscoveryController::updateCard(const QString& CardId, const QString& Word, const QString& Translation, int Gender,
                                     const QString& PartOfSpeech, const QString& MeaningGist, const QString& Hint)
{
    if (!AppCards::UpdateCardContent(CardId, Word, Translation, Gender, PartOfSpeech, MeaningGist, Hint))
    {
        qCWarning(LogApp) << "Discover: could not update the card" << CardId;

        return false;
    }

    qCInfo(LogApp) << "Discover: updated the card" << CardId << "from the round results";
    emit resultsChanged();

    return true;
}

int DiscoveryController::findWordTranslationStage(const QString& Word, const QString& Translation) const
{
    return AppCards::FindWordTranslationStage(Word, Translation);
}

QVariantList DiscoveryController::GetResults() const
{
    QVariantList Result;

    if (!Planner)
    {
        return Result;
    }

    const std::vector<FTDiscoveryResult>& Results = Planner->GetResults();

    for (std::size_t Index = 0; Index < Results.size(); ++Index)
    {
        const FTDiscoveryResult& Entry = Results[Index];
        const FTDiscoveryRow& Row = Planner->GetRows()[static_cast<std::size_t>(Entry.RowIndex)];

        QVariantMap Map;
        Map[QStringLiteral("index")] = static_cast<int>(Index);
        Map[QStringLiteral("word")] = FormatHeadword(Row);
        Map[QStringLiteral("gender")] = Row.Gender.value_or(-1);
        Map[QStringLiteral("translation")] = QString::fromStdString(Entry.Translation);
        Map[QStringLiteral("learn")] = Entry.bLearn;

        const std::optional<std::string>& CardId = Index < Written.size() ? Written[Index].CardId : std::nullopt;
        const std::optional<FTCardHandle> Card = CardId ? AppDatabase::Get().GetCardRepository().LoadCard(*CardId) : std::nullopt;

        if (Card && *Card)
        {
            Map[QStringLiteral("card")] = AppCards::DescribeCard(**Card);
            Map[QStringLiteral("translation")] = QString::fromStdString((*Card)->Translation);
        }

        Result.append(Map);
    }

    return Result;
}

int DiscoveryController::GetLearnCount() const
{
    if (!Planner)
    {
        return 0;
    }

    return static_cast<int>(std::count_if(Planner->GetResults().begin(), Planner->GetResults().end(),
                                          [](const FTDiscoveryResult& Result) { return Result.bLearn; }));
}
