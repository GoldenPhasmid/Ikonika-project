#include "core/FTDiscoveryPlanner.h"

#include "core/FTEnglishForms.h"
#include "core/FTTextNormalization.h"

#include <QString>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace
{
    constexpr int OptionCount = 4;
    constexpr int MaximumOtherMeanings = 2;
    constexpr std::size_t StruckOptionCount = 2;

    constexpr std::size_t NeighbourCount = 8;
    constexpr std::size_t PickChoices = 3;

    constexpr double TargetChance = 0.5;
    constexpr double OtherDeckPenalty = 1.0;

    constexpr double KnownIfKnown = 0.97;
    constexpr double KnownIfUnknown = 0.25;
    constexpr double NewIfKnown = 0.03;
    constexpr double NewIfUnknown = 0.40;

    constexpr int ShortStemLength = 5;
    constexpr int MinimumStemLength = 3;


    struct FNativeSpan
    {
        int Start = 0;

        int Length = 0;

        std::string Translation;
    };

    struct FToken
    {
        int Start = 0;

        int End = 0;

        std::string Text;
    };

    bool IsApostrophe(QChar Character)
    {
        return Character == QChar(0x27) || Character == QChar(0x2019);
    }

    std::vector<FToken> SplitTokens(const QString& Text)
    {
        std::vector<FToken> Tokens;
        qsizetype Index = 0;

        while (Index < Text.size())
        {
            if (!Text[Index].isLetterOrNumber())
            {
                ++Index;

                continue;
            }

            const qsizetype Start = Index;

            while (Index < Text.size()
                   && (Text[Index].isLetterOrNumber()
                       || (IsApostrophe(Text[Index]) && Index + 1 < Text.size() && Text[Index + 1].isLetter())))
            {
                ++Index;
            }

            Tokens.push_back(FToken{static_cast<int>(Start), static_cast<int>(Index),
                                    Text.mid(Start, Index - Start).toLower().toStdString()});
        }

        return Tokens;
    }

    bool IsBlankBetween(const QString& Text, int Start, int End)
    {
        for (int Index = Start; Index < End; ++Index)
        {
            if (!Text[Index].isSpace())
            {
                return false;
            }
        }

        return true;
    }

    std::vector<std::string> SplitPhrase(const std::string& Translation)
    {
        static const std::set<std::string> Particles{"to", "a", "an", "the"};
        static const std::set<std::string> Placeholders{"sb", "sth", "someone", "something", "somebody", "one's", "oneself"};

        std::vector<std::string> Words;

        for (FToken& Token : SplitTokens(QString::fromStdString(CleanDiscoveryTranslation(Translation))))
        {
            if (Placeholders.contains(Token.Text))
            {
                return {};
            }

            Words.push_back(std::move(Token.Text));
        }

        if (Words.size() > 1 && Particles.contains(Words.front()))
        {
            Words.erase(Words.begin());
        }

        return Words;
    }

    std::optional<FNativeSpan> FindNativeSpan(const std::string& Translation, const FTDictionarySense& Meaning, bool bVerb)
    {
        static const std::set<std::string> Auxiliaries{
            "has", "have", "had", "will", "would", "is", "are", "was", "were", "am", "be", "been", "being", "shall"};

        const QString Text = QString::fromStdString(Translation);
        const std::vector<FToken> Tokens = SplitTokens(Text);

        std::optional<std::pair<std::size_t, std::size_t>> Best;
        std::string Matched;

        for (const FTDictionaryTranslation& Candidate : Meaning.Translations)
        {
            const std::vector<std::string> Phrase = SplitPhrase(Candidate.Text);

            if (Phrase.empty() || Phrase.size() > Tokens.size())
            {
                continue;
            }

            for (std::size_t Start = 0; Start + Phrase.size() <= Tokens.size(); ++Start)
            {
                bool bMatches = true;

                for (std::size_t Offset = 0; Offset < Phrase.size() && bMatches; ++Offset)
                {
                    bMatches = IsEnglishForm(Tokens[Start + Offset].Text, Phrase[Offset]);
                }

                if (!bMatches)
                {
                    continue;
                }

                if (!Best || Start < Best->first || (Start == Best->first && Phrase.size() > Best->second))
                {
                    Best = std::pair<std::size_t, std::size_t>{Start, Phrase.size()};
                    Matched = CleanDiscoveryTranslation(Candidate.Text);
                }

                break;
            }
        }

        if (!Best)
        {
            return std::nullopt;
        }

        std::size_t First = Best->first;
        const std::size_t Last = Best->first + Best->second - 1;

        while (bVerb && First > 0 && Auxiliaries.contains(Tokens[First - 1].Text)
               && IsBlankBetween(Text, Tokens[First - 1].End, Tokens[First].Start))
        {
            --First;
        }

        return FNativeSpan{Tokens[First].Start, Tokens[Last].End - Tokens[First].Start, Matched};
    }

    int WidenVerbGroup(const QString& Sentence, int Start)
    {
        static const std::set<QString> Leading{
            QStringLiteral("ai"), QStringLiteral("as"), QStringLiteral("a"), QStringLiteral("avons"),
            QStringLiteral("avez"), QStringLiteral("ont"), QStringLiteral("avais"), QStringLiteral("avait"),
            QStringLiteral("avions"), QStringLiteral("aviez"), QStringLiteral("avaient"), QStringLiteral("aurai"),
            QStringLiteral("auras"), QStringLiteral("aura"), QStringLiteral("aurons"), QStringLiteral("aurez"),
            QStringLiteral("auront"), QStringLiteral("aurais"), QStringLiteral("aurait"), QStringLiteral("aurions"),
            QStringLiteral("auriez"), QStringLiteral("auraient"), QStringLiteral("eu"), QStringLiteral("suis"),
            QStringLiteral("es"), QStringLiteral("est"), QStringLiteral("sommes"), QStringLiteral("êtes"),
            QStringLiteral("sont"), QStringLiteral("étais"), QStringLiteral("était"), QStringLiteral("étions"),
            QStringLiteral("étiez"), QStringLiteral("étaient"), QStringLiteral("serai"), QStringLiteral("seras"),
            QStringLiteral("sera"), QStringLiteral("serons"), QStringLiteral("serez"), QStringLiteral("seront"),
            QStringLiteral("serais"), QStringLiteral("serait"), QStringLiteral("serions"), QStringLiteral("seriez"),
            QStringLiteral("seraient"), QStringLiteral("été"), QStringLiteral("se"), QStringLiteral("me"),
            QStringLiteral("te")};
        static const std::set<QString> Elided{QStringLiteral("s"), QStringLiteral("m"), QStringLiteral("t")};

        int Result = Start;

        while (Result > 0)
        {
            const bool bElided = IsApostrophe(Sentence[Result - 1]);
            int WordEnd = bElided ? Result - 1 : Result;

            if (!bElided)
            {
                while (WordEnd > 0 && Sentence[WordEnd - 1].isSpace())
                {
                    --WordEnd;
                }

                if (WordEnd == Result)
                {
                    return Result;
                }
            }

            int WordStart = WordEnd;

            while (WordStart > 0 && Sentence[WordStart - 1].isLetter())
            {
                --WordStart;
            }

            const QString Word = Sentence.mid(WordStart, WordEnd - WordStart);

            if (Word.isEmpty() || !(bElided ? Elided : Leading).contains(Word))
            {
                return Result;
            }

            Result = WordStart;
        }

        return Result;
    }

    bool AsksInSentence(const std::string& PartOfSpeech)
    {
        static const std::set<std::string> SentenceParts{
            "verb", "adj", "adv", "prep", "prep_phrase", "phrase", "conj"};

        return SentenceParts.contains(PartOfSpeech);
    }

    std::string FoldHeadword(const std::string& Headword)
    {
        return FoldCase(Headword);
    }

    QString FoldForSearch(const QString& Text)
    {
        QString Folded;
        Folded.reserve(Text.size());

        for (const QChar Character : Text)
        {
            const QString Parts = Character.decomposition();
            const QChar Base = Parts.isEmpty() ? Character : Parts.front();

            Folded.append(Base.toLower());
        }

        return Folded;
    }

    std::optional<std::pair<int, int>> FindAtWordStart(const QString& Sentence, const QString& Needle)
    {
        if (Needle.isEmpty())
        {
            return std::nullopt;
        }

        qsizetype From = 0;

        while (true)
        {
            const qsizetype Start = Sentence.indexOf(Needle, From);

            if (Start < 0)
            {
                return std::nullopt;
            }

            if (Start == 0 || !Sentence[Start - 1].isLetter())
            {
                qsizetype End = Start + Needle.size();

                while (End < Sentence.size() && Sentence[End].isLetter())
                {
                    ++End;
                }

                return std::pair<int, int>{static_cast<int>(Start), static_cast<int>(End - Start)};
            }

            From = Start + 1;
        }
    }

    QString StripReflexive(const QString& Word)
    {
        if (Word.startsWith(QStringLiteral("se ")))
        {
            return Word.mid(3);
        }

        if (Word.startsWith(QStringLiteral("s'")) || Word.startsWith(QStringLiteral("s’")))
        {
            return Word.mid(2);
        }

        return Word;
    }

    std::optional<std::pair<int, int>> FindWholeWord(const QString& Sentence, const QString& Needle)
    {
        if (Needle.isEmpty())
        {
            return std::nullopt;
        }

        qsizetype From = 0;

        while (true)
        {
            const qsizetype Start = Sentence.indexOf(Needle, From);

            if (Start < 0)
            {
                return std::nullopt;
            }

            const qsizetype End = Start + Needle.size();

            if ((Start == 0 || !Sentence[Start - 1].isLetter()) && (End == Sentence.size() || !Sentence[End].isLetter()))
            {
                return std::pair<int, int>{static_cast<int>(Start), static_cast<int>(Needle.size())};
            }

            From = Start + 1;
        }
    }

    std::optional<std::pair<int, int>> FindWordSpan(const std::string& Example, const std::string& Headword,
                                                    const std::vector<std::string>& Forms)
    {
        const QString Sentence = FoldForSearch(QString::fromStdString(Example));

        std::optional<std::pair<int, int>> Best;

        for (const std::string& Form : Forms)
        {
            const auto Found = FindWholeWord(Sentence, StripReflexive(FoldForSearch(QString::fromStdString(Form)).trimmed()));

            if (Found && (!Best || Found->first < Best->first || (Found->first == Best->first && Found->second > Best->second)))
            {
                Best = Found;
            }
        }

        if (Best)
        {
            return Best;
        }

        const QString Word = StripReflexive(FoldForSearch(QString::fromStdString(Headword)).trimmed());

        if (const auto Exact = FindAtWordStart(Sentence, Word))
        {
            return Exact;
        }

        const qsizetype StemLength = Word.size() > ShortStemLength
            ? Word.size() - 2
            : std::max<qsizetype>(MinimumStemLength, Word.size() - 1);

        if (StemLength < MinimumStemLength || StemLength >= Word.size())
        {
            return std::nullopt;
        }

        return FindAtWordStart(Sentence, Word.left(StemLength));
    }

    std::set<std::string> NormalizedTranslations(const FTDictionarySense& Meaning)
    {
        std::set<std::string> Result;

        for (const FTDictionaryTranslation& Translation : Meaning.Translations)
        {
            const std::string Normalized = NormalizeDiscoveryTranslation(Translation.Text);

            if (!Normalized.empty())
            {
                Result.insert(Normalized);
            }
        }

        return Result;
    }

    const FTDictionarySense* FindAskedMeaning(const FTDiscoveryRow& Row, const std::vector<FTDictionarySense>& Meanings)
    {
        if (Meanings.empty())
        {
            return nullptr;
        }

        if (Row.Gist.empty())
        {
            return &Meanings.front();
        }

        const auto Found = std::find_if(Meanings.begin(), Meanings.end(),
                                        [&Row](const FTDictionarySense& Meaning) { return Meaning.Gist == Row.Gist; });

        return Found != Meanings.end() ? &*Found : nullptr;
    }
}

int GetDiscoveryClueCount(EFTDiscoveryPrompt Prompt)
{
    return Prompt == EFTDiscoveryPrompt::Sentence || Prompt == EFTDiscoveryPrompt::Beginner ? 1 : 2;
}

bool ShowsDiscoveryExample(const FTDiscoveryQuestion& Question)
{
    return Question.Prompt != EFTDiscoveryPrompt::Sentence && Question.Prompt != EFTDiscoveryPrompt::Beginner
        && Question.ClueStep >= 1;
}

bool ShowsDiscoverySentence(const FTDiscoveryQuestion& Question)
{
    return Question.Prompt == EFTDiscoveryPrompt::Sentence || Question.Prompt == EFTDiscoveryPrompt::Beginner
        || ShowsDiscoveryExample(Question);
}

std::string CleanDiscoveryTranslation(const std::string& Text)
{
    std::string Result;
    Result.reserve(Text.size());

    int Depth = 0;

    for (const char Character : Text)
    {
        if (Character == '(')
        {
            ++Depth;

            continue;
        }

        if (Character == ')')
        {
            Depth = std::max(0, Depth - 1);

            continue;
        }

        if (Depth > 0)
        {
            continue;
        }

        const bool bSpace = Character == ' ' || Character == '\t';

        if (bSpace && (Result.empty() || Result.back() == ' '))
        {
            continue;
        }

        Result.push_back(bSpace ? ' ' : Character);
    }

    while (!Result.empty() && Result.back() == ' ')
    {
        Result.pop_back();
    }

    return Result;
}

std::string NormalizeDiscoveryTranslation(const std::string& Text)
{
    std::string Result = FoldAccents(FoldCase(CleanDiscoveryTranslation(Text)));

    if (Result.rfind("to ", 0) == 0)
    {
        Result.erase(0, 3);
    }

    return Result;
}

FTDiscoveryPlanner::FTDiscoveryPlanner(std::vector<FTDiscoveryRow> InRows, FTDiscoverySources InSources,
                                       const FTDiscoveryLearner& Learner, int InQuestionCount, uint32_t Seed)
    : Rows(std::move(InRows))
    , Sources(std::move(InSources))
    , MeaningCache(Rows.size())
    , FormCache(Rows.size())
    , Belief(FTLevelBelief::FromCutoffRank(Learner.CutoffRank))
    , Random(Seed)
    , QuestionCount(std::max(1, InQuestionCount))
    , CutoffRank(Learner.CutoffRank)
    , bBeginner(Learner.KnownWordCount < BeginnerWordCount)
{
    Advance();
}

const std::vector<FTDiscoveryRow>& FTDiscoveryPlanner::GetRows() const
{
    return Rows;
}

const FTDiscoveryQuestion* FTDiscoveryPlanner::GetCurrentQuestion() const
{
    return Current ? &*Current : nullptr;
}

int FTDiscoveryPlanner::GetAskedCount() const
{
    return AskedCount;
}

int FTDiscoveryPlanner::GetQuestionCount() const
{
    return QuestionCount;
}

bool FTDiscoveryPlanner::IsFinished() const
{
    return !Current.has_value();
}

const std::vector<FTDiscoveryResult>& FTDiscoveryPlanner::GetResults() const
{
    return Results;
}

int FTDiscoveryPlanner::GetLevelCutoffRank() const
{
    return Belief.GetCutoffRank();
}

bool FTDiscoveryPlanner::IsBeginner() const
{
    return bBeginner;
}

void FTDiscoveryPlanner::SetLearn(int ResultIndex, bool bLearn)
{
    if (ResultIndex < 0 || ResultIndex >= static_cast<int>(Results.size()))
    {
        return;
    }

    Results[static_cast<std::size_t>(ResultIndex)].bLearn = bLearn;
}

void FTDiscoveryPlanner::SetTranslation(int ResultIndex, const std::string& Translation)
{
    if (ResultIndex < 0 || ResultIndex >= static_cast<int>(Results.size()))
    {
        return;
    }

    Results[static_cast<std::size_t>(ResultIndex)].Translation = Translation;
}

int FTDiscoveryPlanner::GetCurrentResultIndex() const
{
    if (!Current || !Current->Outcome)
    {
        return -1;
    }

    return static_cast<int>(Results.size()) - 1;
}

void FTDiscoveryPlanner::Choose(int OptionIndex)
{
    if (!Current || Current->Outcome || OptionIndex < 0 || OptionIndex >= static_cast<int>(Current->Options.size())
        || Current->Struck[static_cast<std::size_t>(OptionIndex)])
    {
        return;
    }

    Current->Chosen = OptionIndex;

    if (!Current->Options[static_cast<std::size_t>(OptionIndex)].bCorrect)
    {
        const bool bLastChance = Current->Prompt == EFTDiscoveryPrompt::Beginner
            ? Current->ClueStep >= GetDiscoveryClueCount(Current->Prompt)
            : ShowsDiscoverySentence(*Current);

        if (bLastChance)
        {
            Settle(EFTDiscoveryOutcome::New);

            return;
        }

        Current->Struck[static_cast<std::size_t>(OptionIndex)] = true;

        AskClue();

        return;
    }

    Settle(Current->ClueStep > 0 ? EFTDiscoveryOutcome::Almost : EFTDiscoveryOutcome::Known);
}

void FTDiscoveryPlanner::AskClue()
{
    if (!Current || Current->Outcome)
    {
        return;
    }

    const int ClueCount = GetDiscoveryClueCount(Current->Prompt);

    if (Current->ClueStep >= ClueCount)
    {
        Settle(EFTDiscoveryOutcome::New);

        return;
    }

    ++Current->ClueStep;

    if (Current->ClueStep < ClueCount)
    {
        return;
    }

    std::vector<std::size_t> Wrong;

    for (std::size_t Index = 0; Index < Current->Options.size(); ++Index)
    {
        if (!Current->Options[Index].bCorrect && !Current->Struck[Index])
        {
            Wrong.push_back(Index);
        }
    }

    std::shuffle(Wrong.begin(), Wrong.end(), Random);

    const auto StruckCount = static_cast<std::size_t>(std::count(Current->Struck.begin(), Current->Struck.end(), true));
    const std::size_t MissingCount = StruckCount < StruckOptionCount ? StruckOptionCount - StruckCount : 0;

    for (std::size_t Index = 0; Index < Wrong.size() && Index < MissingCount; ++Index)
    {
        Current->Struck[Wrong[Index]] = true;
    }
}

void FTDiscoveryPlanner::Advance()
{
    if (Current && !Current->Outcome)
    {
        return;
    }

    Current.reset();

    if (AskedCount >= QuestionCount)
    {
        return;
    }

    std::optional<FTDiscoveryQuestion> Next = PickFreshQuestion();

    if (!Next)
    {
        return;
    }

    Current = std::move(Next);
    ++AskedCount;
}

void FTDiscoveryPlanner::Settle(EFTDiscoveryOutcome Outcome)
{
    Current->Outcome = Outcome;

    const int Rank = Rows[static_cast<std::size_t>(Current->RowIndex)].TeachingRank;
    const bool bMeasures = Current->Prompt != EFTDiscoveryPrompt::Beginner;

    if (bMeasures && Outcome == EFTDiscoveryOutcome::Known)
    {
        Belief.Update(Rank, KnownIfKnown, KnownIfUnknown);
    }
    else if (bMeasures && Outcome == EFTDiscoveryOutcome::New)
    {
        Belief.Update(Rank, NewIfKnown, NewIfUnknown);
    }

    FTDiscoveryResult Result;
    Result.RowIndex = Current->RowIndex;
    Result.Meaning = Current->Meaning;
    Result.Translation = Current->Translation;
    Result.Outcome = Outcome;
    Result.bLearn = Outcome != EFTDiscoveryOutcome::Known || bBeginner;

    Results.push_back(std::move(Result));
}

std::optional<FTDiscoveryQuestion> FTDiscoveryPlanner::PickFreshQuestion()
{
    if (bBeginner)
    {
        if (std::optional<FTDiscoveryQuestion> Question = PickFreshQuestion(true))
        {
            return Question;
        }
    }

    return PickFreshQuestion(false);
}

std::optional<FTDiscoveryQuestion> FTDiscoveryPlanner::PickFreshQuestion(bool bBeginnerPrompt)
{
    std::set<int>& Unaskable = bBeginnerPrompt ? UnaskableBeginnerRows : UnaskableRows;

    while (true)
    {
        std::vector<std::pair<double, int>> Candidates;

        for (std::size_t Index = 0; Index < Rows.size(); ++Index)
        {
            const FTDiscoveryRow& Row = Rows[Index];

            if (!Row.bAskable || Row.TeachingRank <= 0 || Row.TeachingRank <= CutoffRank
                || UnaskableRows.contains(static_cast<int>(Index))
                || Unaskable.contains(static_cast<int>(Index))
                || AskedHeadwords.contains(FoldHeadword(Row.Headword)))
            {
                continue;
            }

            Candidates.emplace_back(std::abs(Belief.GetKnownChance(Row.TeachingRank) - TargetChance),
                                    static_cast<int>(Index));
        }

        if (Candidates.empty())
        {
            return std::nullopt;
        }

        const std::size_t ChoiceCount = std::min(PickChoices, Candidates.size());

        std::partial_sort(Candidates.begin(), Candidates.begin() + static_cast<std::ptrdiff_t>(ChoiceCount),
                          Candidates.end());

        const int RowIndex = Candidates[Random() % ChoiceCount].second;
        const FTDiscoveryRow& Row = Rows[static_cast<std::size_t>(RowIndex)];

        std::optional<FTDiscoveryQuestion> Question =
            BuildQuestion(RowIndex, bBeginnerPrompt ? EFTDiscoveryPrompt::Beginner : ChoosePrompt(Row));

        if (!Question)
        {
            Unaskable.insert(RowIndex);

            continue;
        }

        AskedHeadwords.insert(FoldHeadword(Row.Headword));

        return Question;
    }
}

EFTDiscoveryPrompt FTDiscoveryPlanner::ChoosePrompt(const FTDiscoveryRow& Row)
{
    return bBeginner || AsksInSentence(Row.PartOfSpeech) ? EFTDiscoveryPrompt::Sentence : EFTDiscoveryPrompt::Word;
}

const std::vector<FTDictionarySense>& FTDiscoveryPlanner::LoadMeanings(int RowIndex)
{
    std::optional<std::vector<FTDictionarySense>>& Cached = MeaningCache[static_cast<std::size_t>(RowIndex)];

    if (!Cached)
    {
        Cached = Sources.LoadMeanings ? Sources.LoadMeanings(Rows[static_cast<std::size_t>(RowIndex)])
                                      : std::vector<FTDictionarySense>{};
    }

    return *Cached;
}

const std::vector<std::string>& FTDiscoveryPlanner::LoadForms(int RowIndex)
{
    std::optional<std::vector<std::string>>& Cached = FormCache[static_cast<std::size_t>(RowIndex)];

    if (!Cached)
    {
        Cached = Sources.LoadForms ? Sources.LoadForms(Rows[static_cast<std::size_t>(RowIndex)]) : std::vector<std::string>{};
    }

    return *Cached;
}

bool FTDiscoveryPlanner::AddBeginnerSentence(FTDiscoveryQuestion& Question)
{
    const FTDiscoveryRow& Row = Rows[static_cast<std::size_t>(Question.RowIndex)];
    const bool bVerb = Row.PartOfSpeech == "verb";

    if (Question.ExampleSpanStart < 0 || Question.Meaning.ExampleTranslation.empty())
    {
        return false;
    }

    const auto Native = FindNativeSpan(Question.Meaning.ExampleTranslation, Question.Meaning, bVerb);

    if (!Native)
    {
        return false;
    }

    const QString Example = QString::fromStdString(Question.Meaning.Example);
    const int FrenchEnd = Question.ExampleSpanStart + Question.ExampleSpanLength;
    const int FrenchStart = bVerb ? WidenVerbGroup(Example.toLower(), Question.ExampleSpanStart) : Question.ExampleSpanStart;

    QString French = Example.mid(FrenchStart, FrenchEnd - FrenchStart);

    if (Native->Start == 0 && !French.isEmpty())
    {
        French[0] = French[0].toUpper();
    }

    const QString Translation = QString::fromStdString(Question.Meaning.ExampleTranslation);

    Question.BeginnerSentence = (Translation.left(Native->Start) + French + Translation.mid(Native->Start + Native->Length))
                                    .toStdString();
    Question.BeginnerSpanStart = Native->Start;
    Question.BeginnerSpanLength = static_cast<int>(French.size());

    return true;
}

std::vector<int> FTDiscoveryPlanner::FindNeighbours(int RowIndex) const
{
    const FTDiscoveryRow& Asked = Rows[static_cast<std::size_t>(RowIndex)];
    const std::string AskedHeadword = FoldHeadword(Asked.Headword);
    const double AskedLevel = std::log(static_cast<double>(std::max(Asked.TeachingRank, 1)));

    std::vector<std::pair<double, int>> Scored;

    for (std::size_t Index = 0; Index < Rows.size(); ++Index)
    {
        const FTDiscoveryRow& Row = Rows[Index];

        if (Row.PartOfSpeech != Asked.PartOfSpeech || Row.TeachingRank <= 0 || FoldHeadword(Row.Headword) == AskedHeadword)
        {
            continue;
        }

        const double Distance = std::abs(std::log(static_cast<double>(Row.TeachingRank)) - AskedLevel)
            + (Row.DeckId == Asked.DeckId ? 0.0 : OtherDeckPenalty);

        Scored.emplace_back(Distance, static_cast<int>(Index));
    }

    const std::size_t Take = std::min(NeighbourCount, Scored.size());

    std::partial_sort(Scored.begin(), Scored.begin() + static_cast<std::ptrdiff_t>(Take), Scored.end());

    std::vector<int> Result;
    Result.reserve(Take);

    for (std::size_t Index = 0; Index < Take; ++Index)
    {
        Result.push_back(Scored[Index].second);
    }

    return Result;
}

std::optional<FTDiscoveryQuestion> FTDiscoveryPlanner::BuildQuestion(int RowIndex, EFTDiscoveryPrompt Prompt)
{
    const FTDiscoveryRow& Row = Rows[static_cast<std::size_t>(RowIndex)];
    const std::vector<FTDictionarySense> Meanings = LoadMeanings(RowIndex);
    const FTDictionarySense* Asked = FindAskedMeaning(Row, Meanings);

    if (!Asked || Asked->Translations.empty() || Asked->Example.empty()
        || CleanDiscoveryTranslation(Asked->Translations.front().Text).empty())
    {
        return std::nullopt;
    }

    FTDiscoveryQuestion Question;
    Question.RowIndex = RowIndex;
    Question.Meaning = *Asked;
    Question.Prompt = Prompt;

    const auto Native = FindNativeSpan(Asked->ExampleTranslation, *Asked, Row.PartOfSpeech == "verb");
    Question.Translation = Native ? Native->Translation : CleanDiscoveryTranslation(Asked->Translations.front().Text);

    if (const auto Span = FindWordSpan(Asked->Example, Row.Headword, LoadForms(RowIndex)))
    {
        Question.ExampleSpanStart = Span->first;
        Question.ExampleSpanLength = Span->second;
    }

    if (Prompt == EFTDiscoveryPrompt::Beginner && Question.ExampleSpanStart >= 0 && Sources.IsKnownExample
        && Sources.IsKnownExample(Row, Asked->Example))
    {
        Question.Prompt = EFTDiscoveryPrompt::Sentence;
    }

    if (Question.Prompt == EFTDiscoveryPrompt::Beginner && !AddBeginnerSentence(Question))
    {
        return std::nullopt;
    }

    if (Prompt == EFTDiscoveryPrompt::Sentence && Question.ExampleSpanStart < 0)
    {
        return std::nullopt;
    }

    if (!AddNativeOptions(Question, Meanings))
    {
        return std::nullopt;
    }

    std::shuffle(Question.Options.begin(), Question.Options.end(), Random);
    Question.Struck.assign(Question.Options.size(), false);

    return Question;
}

bool FTDiscoveryPlanner::AddNativeOptions(FTDiscoveryQuestion& Question, const std::vector<FTDictionarySense>& Meanings)
{
    const FTDiscoveryRow& Row = Rows[static_cast<std::size_t>(Question.RowIndex)];

    FTDiscoveryOption Correct;
    Correct.Text = Question.Translation;
    Correct.bCorrect = true;

    std::set<std::string> Taken{NormalizeDiscoveryTranslation(Correct.Text)};
    Question.Options.push_back(std::move(Correct));

    const std::set<std::string> AskedTranslations = NormalizedTranslations(Question.Meaning);

    std::set<std::string> EntryTranslations = AskedTranslations;
    EntryTranslations.insert(NormalizeDiscoveryTranslation(Row.Translation));

    for (const FTDictionarySense& Meaning : Meanings)
    {
        const std::set<std::string> Translations = NormalizedTranslations(Meaning);
        EntryTranslations.insert(Translations.begin(), Translations.end());
    }

    if (Question.Prompt == EFTDiscoveryPrompt::Sentence)
    {
        int Added = 0;

        for (const FTDictionarySense& Meaning : Meanings)
        {
            if (Added >= MaximumOtherMeanings || Meaning.Gist == Question.Meaning.Gist || Meaning.Translations.empty())
            {
                continue;
            }

            const std::string Text = CleanDiscoveryTranslation(Meaning.Translations.front().Text);
            const std::string Normalized = NormalizeDiscoveryTranslation(Text);

            if (Normalized.empty() || AskedTranslations.contains(Normalized) || Taken.contains(Normalized))
            {
                continue;
            }

            Taken.insert(Normalized);

            FTDiscoveryOption Option;
            Option.Text = Text;
            Option.bOtherMeaning = true;

            Question.Options.push_back(std::move(Option));
            ++Added;
        }
    }

    std::vector<int> Neighbours = FindNeighbours(Question.RowIndex);
    std::shuffle(Neighbours.begin(), Neighbours.end(), Random);

    for (const int Neighbour : Neighbours)
    {
        if (static_cast<int>(Question.Options.size()) >= OptionCount)
        {
            break;
        }

        const std::string Text = CleanDiscoveryTranslation(Rows[static_cast<std::size_t>(Neighbour)].Translation);
        const std::string Normalized = NormalizeDiscoveryTranslation(Text);

        if (Normalized.empty() || EntryTranslations.contains(Normalized) || Taken.contains(Normalized))
        {
            continue;
        }

        Taken.insert(Normalized);

        FTDiscoveryOption Option;
        Option.Text = Text;
        Option.RowIndex = Neighbour;

        Question.Options.push_back(std::move(Option));
    }

    return static_cast<int>(Question.Options.size()) == OptionCount;
}
