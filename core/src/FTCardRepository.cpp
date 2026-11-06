#include "core/FTCardRepository.h"

#include "core/FTDatabase.h"
#include "core/FTErasure.h"
#include "core/FTJournal.h"
#include "core/FTLog.h"
#include "core/FTSchedule.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"
#include "core/FTUuid.h"
#include "core/FTWordForms.h"

#include <QString>

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace
{
    std::string Trim(const std::string& Value)
    {
        const auto NotSpace = [](unsigned char Character)
        {
            return std::isspace(Character) == 0;
        };

        const auto First = std::find_if(Value.begin(), Value.end(), NotSpace);
        const auto Last = std::find_if(Value.rbegin(), Value.rend(), NotSpace).base();

        return First < Last ? std::string(First, Last) : std::string();
    }

    /** Counted in characters, so an accented word is not cut short of the limit the interface
        enforces. Applied here as well as in the fields, since import and sync reach the same
        columns without passing a field at all. */
    std::string ClampFieldLength(const std::string& Value, int MaxLength = MaximumCardFieldLength)
    {
        const QString Wide = QString::fromStdString(Value);

        if (Wide.size() <= MaxLength)
        {
            return Value;
        }

        return Wide.left(MaxLength).trimmed().toStdString();
    }

    /** ASCII only, which is enough: this compares translations, which are native-language text,
        and the database's own NOCASE collation is equally ASCII-only. */
    std::string ToLowerCase(std::string Value)
    {
        std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char Character)
        {
            return static_cast<char>(std::tolower(Character));
        });

        return Value;
    }

    bool IsBlank(const std::string& Value)
    {
        return std::all_of(Value.begin(), Value.end(), [](unsigned char Character)
        {
            return std::isspace(Character) != 0;
        });
    }

    /** A NULL date column is a real state, not a zero. A pool card has no due date at all. */
    std::optional<FTDayNumber> ReadOptionalDay(const FTStatement& Query, int Column)
    {
        if (Query.IsNull(Column))
        {
            return std::nullopt;
        }

        return static_cast<FTDayNumber>(Query.GetInt(Column));
    }

    void BindOptionalDay(FTStatement& Query, int Index, const std::optional<FTDayNumber>& Value)
    {
        if (!Value.has_value())
        {
            Query.BindNull(Index);

            return;
        }

        Query.BindInt(Index, static_cast<int>(*Value));
    }

    std::optional<int> ReadOptionalInt(const FTStatement& Query, int Column)
    {
        if (Query.IsNull(Column))
        {
            return std::nullopt;
        }

        return Query.GetInt(Column);
    }

    void BindOptionalInt(FTStatement& Query, int Index, const std::optional<int>& Value)
    {
        if (!Value.has_value())
        {
            Query.BindNull(Index);

            return;
        }

        Query.BindInt(Index, *Value);
    }

    /** Every reader below selects these in this order, so ReadCard can share the column indices. */
    constexpr const char* CardColumns =
        "Id, TargetWord, Translation, Definition, Gender, UserEditedFields, "
        "LeitnerStage, DueDate, BoxChangedDate, CompletedDate, CreatedDate, LastUpdatedDate, "
        "PartOfSpeech, MeaningGist, Hint";

    std::string EscapeLikePattern(const std::string& Value)
    {
        std::string Result;
        Result.reserve(Value.size());

        for (char Character : Value)
        {
            if (Character == '%' || Character == '_' || Character == '\\')
            {
                Result.push_back('\\');
            }

            Result.push_back(Character);
        }

        return Result;
    }

    FTCard ReadCardRow(const FTStatement& Query)
    {
        FTCard Word;
        Word.Id = Query.GetText(0);
        Word.TargetWord = Query.GetText(1);
        Word.Translation = Query.GetText(2);
        Word.Definition = Query.GetText(3);
        Word.Gender = ReadOptionalInt(Query, 4);
        Word.UserEditedFields = static_cast<uint32_t>(Query.GetInt(5));
        Word.LeitnerStage = Query.GetInt(6);
        Word.DueDate = ReadOptionalDay(Query, 7);
        Word.BoxChangedDate = ReadOptionalDay(Query, 8);
        Word.CompletedDate = ReadOptionalDay(Query, 9);
        Word.CreatedDate = Query.GetInt64(10);
        Word.LastUpdatedDate = Query.GetInt64(11);
        Word.PartOfSpeech = Query.GetText(12);
        Word.MeaningGist = Query.GetText(13);
        Word.Hint = Query.GetText(14);

        return Word;
    }

    class TransactionGuard
    {
    public:
        explicit TransactionGuard(FTDatabase& InTarget)
            : Target(InTarget)
        {
            bActive = Target.BeginTransaction();
        }

        ~TransactionGuard()
        {
            if (bActive)
            {
                Target.Rollback();
            }
        }

        TransactionGuard(const TransactionGuard&) = delete;
        TransactionGuard& operator=(const TransactionGuard&) = delete;

        bool IsActive() const
        {
            return bActive;
        }

        bool Commit()
        {
            if (!bActive)
            {
                return false;
            }

            bActive = false;

            return Target.Commit();
        }

    private:
        FTDatabase& Target;
        bool bActive = false;
    };
}

namespace
{
    /*
     * Placeholder numbers the active language pair is bound to.
     *
     * Deliberately far above what any query here uses for its own parameters. The pair is appended by
     * a shared clause that does not know how many placeholders the query it lands in already has, and
     * these are rewritten to named placeholders before SQLite sees them, so the numbers need only be
     * unique rather than contiguous.
     */
    constexpr int TargetLanguageParameter = 90;
    constexpr int NativeLanguageParameter = 91;
}

static constexpr const char* FormsCardColumns =
    "Id, TargetWord, Translation, LearnedDate, CreatedDate, LastUpdatedDate";

static FTCard ReadFormsCard(const FTStatement& Query)
{
    FTCard Card;
    Card.Id = Query.GetText(0);
    Card.TargetWord = Query.GetText(1);
    Card.Translation = Query.GetText(2);
    Card.CompletedDate = ReadOptionalDay(Query, 3);
    Card.CreatedDate = Query.GetInt64(4);
    Card.LastUpdatedDate = Query.GetInt64(5);

    return Card;
}

FTCardRepository::FTCardRepository(FTDatabase& InDatabase, ClockFunction InClock)
    : Target(InDatabase)
    , Clock(std::move(InClock))
{
}

std::vector<FTCard> FTCardRepository::ReadCardRows(FTStatement& Query) const
{
    std::vector<FTCard> Rows;

    while (Query.Step())
    {
        Rows.push_back(ReadCardRow(Query));
    }

    if (Annotator && !Rows.empty())
    {
        Annotator(Rows);
    }

    return Rows;
}

std::vector<FTCardHandle> FTCardRepository::ReadCards(FTStatement& Query) const
{
    std::vector<FTCardHandle> Result;

    for (FTCard& Row : ReadCardRows(Query))
    {
        Result.push_back(Store.UpdateCard(std::move(Row)));
    }

    return Result;
}

void FTCardRepository::SetCardAnnotator(FTCardAnnotator InAnnotator)
{
    Annotator = std::move(InAnnotator);
}

void FTCardRepository::RefreshStoredCard(const std::string& CardId) const
{
    if (Store.FindCard(CardId) == nullptr)
    {
        return;
    }

    LoadCard(CardId);
}

void FTCardRepository::SetActiveLanguages(const std::string& InTargetLanguage,
                                          const std::string& InNativeLanguage)
{
    Pair.Set(InTargetLanguage, InNativeLanguage);
}

const std::string& FTCardRepository::GetTargetLanguage() const
{
    return Pair.GetTargetLanguage();
}

const std::string& FTCardRepository::GetNativeLanguage() const
{
    return Pair.GetNativeLanguage();
}

int64_t FTCardRepository::Now() const
{
    return Clock ? Clock() : GetSystemTimeMilliseconds();
}

bool FTCardRepository::DeleteLanguage(const std::string& TargetLanguage)
{
    const FTQueryTimer Timer("FTCardRepository::DeleteLanguage");

    if (TargetLanguage.empty())
    {
        return false;
    }

    const std::vector<std::string> Statements{
        "DELETE FROM Journal WHERE CardId IN (SELECT Id FROM Cards WHERE TargetLanguage = ?1)",
        "DELETE FROM Cards WHERE TargetLanguage = ?1",
        "DELETE FROM WordForms WHERE TargetLanguage = ?1",
        "DELETE FROM TriagedWords WHERE TargetLanguage = ?1",
        "DELETE FROM SearchHistory WHERE TargetLanguage = ?1",
        "DELETE FROM Placements WHERE TargetLanguage = ?1",
        "DELETE FROM DailyStats WHERE TargetLanguage = ?1"};

    TransactionGuard Transaction(Target);

    if (!Transaction.IsActive())
    {
        return false;
    }

    for (const std::string& Sql : Statements)
    {
        FTStatement Delete = Target.Prepare(Sql);

        if (!Delete.IsValid())
        {
            return false;
        }

        Delete.BindText(1, TargetLanguage);

        if (!Delete.Run())
        {
            qCWarning(LogDatabase) << "Could not delete the rows of" << TargetLanguage.c_str();

            return false;
        }
    }

    FTStatement Erasure = Target.Prepare(
        "INSERT INTO Erasures (Scope, ErasedDate) VALUES (?1, ?2) "
        "ON CONFLICT (Scope) DO UPDATE SET ErasedDate = excluded.ErasedDate");

    if (!Erasure.IsValid())
    {
        return false;
    }

    Erasure.BindText(1, MakeLanguageErasureScope(TargetLanguage));
    Erasure.BindInt64(2, Now());

    if (!Erasure.Run())
    {
        return false;
    }

    return Transaction.Commit();
}

int FTCardRepository::CountLanguageCards(const std::string& TargetLanguage) const
{
    const FTQueryTimer Timer("FTCardRepository::CountLanguageCards");

    FTStatement Query = Target.Prepare(
        "SELECT COUNT(*) FROM Cards WHERE Deleted = 0 AND TargetLanguage = ?1");

    if (!Query.IsValid())
    {
        return 0;
    }

    Query.BindText(1, TargetLanguage);

    if (!Query.Step())
    {
        return 0;
    }

    return Query.GetInt(0);
}

std::vector<FTCardHandle> FTCardRepository::LoadCards() const
{
    const FTQueryTimer Timer("FTCardRepository::LoadCards");

    FTStatement Query = Target.Prepare(
        std::string("SELECT ") + CardColumns + " FROM Cards "
        "WHERE Deleted = 0 AND " + Pair.Clause("Cards") + " "
        "ORDER BY CreatedDate ASC, rowid ASC");

    if (!Query.IsValid())
    {
        return {};
    }

    Pair.Bind(Query);

    return ReadCards(Query);
}

std::optional<FTCardHandle> FTCardRepository::LoadCard(const std::string& CardId) const
{
    const FTQueryTimer Timer("FTCardRepository::LoadCard");

    FTStatement Query = Target.Prepare(
        std::string("SELECT ") + CardColumns + " FROM Cards WHERE Id = ?1 AND Deleted = 0");

    if (!Query.IsValid())
    {
        return std::nullopt;
    }

    Query.BindText(1, CardId);

    const std::vector<FTCardHandle> Loaded = ReadCards(Query);

    if (Loaded.empty())
    {
        return std::nullopt;
    }

    return Loaded.front();
}

std::optional<int> FTCardRepository::FindWordStage(const std::string& TargetWord,
                                                   const std::string& PartOfSpeech) const
{
    const FTQueryTimer Timer("FTCardRepository::FindWordStage");

    if (TargetWord.empty() || IsBlank(TargetWord))
    {
        return std::nullopt;
    }

    FTStatement Query = Target.Prepare(
        "SELECT MAX(LeitnerStage) FROM Cards "
        "WHERE Deleted = 0 AND TargetWord = ?1 COLLATE NOCASE AND PartOfSpeech = ?2 "
        "  AND " + Pair.Clause("Cards"));

    if (!Query.IsValid())
    {
        return std::nullopt;
    }

    Query.BindText(1, TargetWord);
    Query.BindText(2, PartOfSpeech);
    Pair.Bind(Query);

    if (!Query.Step())
    {
        return std::nullopt;
    }

    // MAX over no rows is NULL, which is how "not a card at all" arrives.
    if (Query.IsNull(0))
    {
        return std::nullopt;
    }

    return Query.GetInt(0);
}

bool FTCardRepository::IsWordCompleted(const std::string& TargetWord,
                                       const std::string& PartOfSpeech) const
{
    const FTQueryTimer Timer("FTCardRepository::IsWordCompleted");

    if (TargetWord.empty() || IsBlank(TargetWord))
    {
        return false;
    }

    FTStatement Query = Target.Prepare(
        "SELECT COUNT(*) FROM Cards "
        "WHERE Deleted = 0 AND TargetWord = ?1 COLLATE NOCASE AND PartOfSpeech = ?2 "
        "  AND CompletedDate IS NOT NULL "
        "  AND " + Pair.Clause("Cards"));

    if (!Query.IsValid())
    {
        return false;
    }

    Query.BindText(1, TargetWord);
    Query.BindText(2, PartOfSpeech);
    Pair.Bind(Query);

    if (!Query.Step())
    {
        return false;
    }

    return Query.GetInt(0) > 0;
}

std::vector<FTCard> FTCardRepository::LoadRecentlyStudiedCards(int Limit) const
{
    const FTQueryTimer Timer("FTCardRepository::LoadRecentlyStudiedCards");

    std::vector<FTCard> Result;

    if (Limit <= 0)
    {
        return Result;
    }

    FTStatement Query = Target.Prepare(
        std::string("SELECT ") + CardColumns + " FROM Cards "
        "WHERE Deleted = 0 AND BoxChangedDate IS NOT NULL AND LeitnerStage > 0 AND "
        + Pair.Clause("Cards") + " "
        "ORDER BY BoxChangedDate DESC LIMIT ?1");

    if (!Query.IsValid())
    {
        return Result;
    }

    Query.BindInt(1, Limit);
    Pair.Bind(Query);

    return ReadCardRows(Query);
}

std::vector<FTCard> FTCardRepository::FindCardsByHeadword(const std::string& SearchText,
                                                          int Limit) const
{
    const FTQueryTimer Timer("FTCardRepository::FindCardsByHeadword");

    std::vector<FTCard> Result;

    if (SearchText.empty() || Limit <= 0)
    {
        return Result;
    }

    FTStatement Query = Target.Prepare(
        std::string("SELECT ") + CardColumns + " FROM Cards "
        "WHERE Deleted = 0 AND LOWER(TargetWord) LIKE ?1 ESCAPE '\\' AND "
        + Pair.Clause("Cards") + " "
        "ORDER BY LOWER(TargetWord) <> ?2, TargetWord ASC "
        "LIMIT ?3");

    if (!Query.IsValid())
    {
        return Result;
    }

    Query.BindText(1, EscapeLikePattern(SearchText) + "%");
    Query.BindText(2, SearchText);
    Query.BindInt(3, Limit);
    Pair.Bind(Query);

    return ReadCardRows(Query);
}

namespace
{
    std::vector<std::string> SplitTranslationList(const std::string& Stored)
    {
        std::vector<std::string> Parts;

        size_t Start = 0;

        while (Start <= Stored.size())
        {
            const size_t Comma = Stored.find(',', Start);
            const size_t End = Comma == std::string::npos ? Stored.size() : Comma;

            std::string Part = Trim(Stored.substr(Start, End - Start));

            if (!Part.empty())
            {
                Parts.push_back(std::move(Part));
            }

            if (Comma == std::string::npos)
            {
                break;
            }

            Start = Comma + 1;
        }

        return Parts;
    }

    bool TranslationListContains(const std::string& Stored, const std::string& Wanted)
    {
        size_t Start = 0;

        while (Start <= Stored.size())
        {
            const size_t Comma = Stored.find(',', Start);
            const size_t End = Comma == std::string::npos ? Stored.size() : Comma;

            const std::string Part = ToLowerCase(Trim(Stored.substr(Start, End - Start)));

            if (!Part.empty() && Part == Wanted)
            {
                return true;
            }

            if (Comma == std::string::npos)
            {
                break;
            }

            Start = Comma + 1;
        }

        return false;
    }

    /** One card row as the batched stage query reads it, enough to match against the probes. */
    struct StageRow
    {
        std::string TargetWord;
        std::string PartOfSpeech;
        std::optional<int> Gender;
        std::string Translation;
        int LeitnerStage = 0;
    };
}

std::optional<int> FTCardRepository::FindMeaningStage(
    const std::string& TargetWord, const std::string& PartOfSpeech,
    const std::optional<int>& Gender, const std::string& MeaningGist) const
{
    const FTQueryTimer Timer("FTCardRepository::FindMeaningStage");

    if (TargetWord.empty() || MeaningGist.empty())
    {
        return std::nullopt;
    }

    FTStatement Query = Target.Prepare(
        "SELECT MAX(LeitnerStage) FROM Cards "
        "WHERE Deleted = 0 AND TargetWord = ?1 COLLATE NOCASE "
        "  AND PartOfSpeech = ?2 AND Gender IS ?3 AND MeaningGist = ?4 "
        "  AND " + Pair.Clause("Cards"));

    if (!Query.IsValid())
    {
        return std::nullopt;
    }

    Query.BindText(1, TargetWord);
    Query.BindText(2, PartOfSpeech);
    BindOptionalInt(Query, 3, Gender);
    Query.BindText(4, MeaningGist);

    Pair.Bind(Query);

    if (!Query.Step() || Query.IsNull(0))
    {
        return std::nullopt;
    }

    return Query.GetInt(0);
}

std::optional<int> FTCardRepository::FindWordTranslationStage(
    const std::string& TargetWord, const std::string& PartOfSpeech,
    const std::optional<int>& Gender, const std::string& Translation) const
{
    const FTQueryTimer Timer("FTCardRepository::FindWordTranslationStage");

    if (TargetWord.empty() || IsBlank(TargetWord)
        || Translation.empty() || IsBlank(Translation))
    {
        return std::nullopt;
    }

    // The word's cards are fetched and their translations split here rather than in SQL: a card
    // may hold several translations in one field, and splitting a list inside a query would need
    // string surgery that is harder to read than this loop.
    FTStatement Query = Target.Prepare(
        "SELECT Translation, LeitnerStage FROM Cards "
        "WHERE Deleted = 0 AND TargetWord = ?1 COLLATE NOCASE "
        "  AND PartOfSpeech = ?2 AND Gender IS ?3 "
        "  AND " + Pair.Clause("Cards"));

    if (!Query.IsValid())
    {
        return std::nullopt;
    }

    Query.BindText(1, TargetWord);
    Query.BindText(2, PartOfSpeech);

    if (Gender.has_value())
    {
        Query.BindInt(3, *Gender);
    }
    else
    {
        Query.BindNull(3);
    }

    Pair.Bind(Query);

    const std::string Wanted = ToLowerCase(Trim(Translation));

    std::optional<int> Furthest;

    while (Query.Step())
    {
        if (TranslationListContains(Query.GetText(0), Wanted))
        {
            const int Stage = Query.GetInt(1);
            Furthest = Furthest.has_value() ? std::max(*Furthest, Stage) : Stage;
        }
    }

    return Furthest;
}

std::vector<std::string> FTCardRepository::FindCardTranslationsForReading(
    const std::string& TargetWord, const std::string& PartOfSpeech,
    const std::optional<int>& Gender) const
{
    const FTQueryTimer Timer("FTCardRepository::FindCardTranslationsForReading");

    std::vector<std::string> Result;

    if (TargetWord.empty() || IsBlank(TargetWord))
    {
        return Result;
    }

    FTStatement Query = Target.Prepare(
        "SELECT Translation FROM Cards "
        "WHERE Deleted = 0 AND TargetWord = ?1 COLLATE NOCASE "
        "  AND PartOfSpeech = ?2 AND Gender IS ?3 "
        "  AND " + Pair.Clause("Cards"));

    if (!Query.IsValid())
    {
        return Result;
    }

    Query.BindText(1, TargetWord);
    Query.BindText(2, PartOfSpeech);

    if (Gender.has_value())
    {
        Query.BindInt(3, *Gender);
    }
    else
    {
        Query.BindNull(3);
    }

    Pair.Bind(Query);

    std::unordered_set<std::string> Seen;

    while (Query.Step())
    {
        for (std::string& Part : SplitTranslationList(Query.GetText(0)))
        {
            const std::string Folded = ToLowerCase(Part);

            if (Seen.insert(Folded).second)
            {
                Result.push_back(std::move(Part));
            }
        }
    }

    return Result;
}

std::vector<std::optional<int>> FTCardRepository::FindWordTranslationStages(
    const std::vector<FTStageProbe>& Probes) const
{
    const FTQueryTimer Timer("FTCardRepository::FindWordTranslationStages");

    std::vector<std::optional<int>> Result(Probes.size(), std::nullopt);

    if (Probes.empty())
    {
        return Result;
    }

    // Chunked to stay under SQLite's bound-variable limit (999 by default). One IN list per
    // statement is what turns a subject of hundreds of words into a handful of queries.
    constexpr int MaxProbesPerStatement = 500;

    for (size_t Start = 0; Start < Probes.size(); Start += MaxProbesPerStatement)
    {
        const size_t End = std::min(Start + MaxProbesPerStatement, Probes.size());

        // The words this chunk asks about, de-duplicated and ASCII-lowered to match the NOCASE
        // collation the single-word query relies on. LOWER() is equally ASCII-only in SQLite.
        std::vector<std::string> Words;

        for (size_t Index = Start; Index < End; ++Index)
        {
            const FTStageProbe& Probe = Probes[Index];

            if (Probe.TargetWord.empty() || IsBlank(Probe.TargetWord)
                || Probe.Translation.empty() || IsBlank(Probe.Translation))
            {
                continue;
            }

            const std::string Lowered = ToLowerCase(Probe.TargetWord);

            if (std::find(Words.begin(), Words.end(), Lowered) == Words.end())
            {
                Words.push_back(Lowered);
            }
        }

        if (Words.empty())
        {
            continue;
        }

        std::string Sql =
            "SELECT TargetWord, Translation, LeitnerStage, PartOfSpeech, Gender FROM Cards "
            "WHERE Deleted = 0 AND LOWER(TargetWord) IN (?1";

        for (size_t Index = 1; Index < Words.size(); ++Index)
        {
            Sql += ", ?" + std::to_string(Index + 1);
        }

        Sql += ") AND " + Pair.Clause("Cards");

        FTStatement Query = Target.Prepare(Sql);

        if (!Query.IsValid())
        {
            // A failed chunk leaves its words unanswerable rather than guessed at; the rest of the
            // batch still answers.
            continue;
        }

        for (size_t Index = 0; Index < Words.size(); ++Index)
        {
            Query.BindText(static_cast<int>(Index + 1), Words[Index]);
        }

        Pair.Bind(Query);

        std::vector<StageRow> Rows;

        while (Query.Step())
        {
            StageRow Row;
            Row.TargetWord = Query.GetText(0);
            Row.Translation = Query.GetText(1);
            Row.LeitnerStage = Query.GetInt(2);
            Row.PartOfSpeech = Query.GetText(3);
            Row.Gender = ReadOptionalInt(Query, 4);

            Rows.push_back(std::move(Row));
        }

        // Each probe is matched against every card the words fetched, the same way the single-word
        // query matches its one: the reading first, then the translation's list, taking the highest
        // box a matching card sits in.
        for (size_t Index = Start; Index < End; ++Index)
        {
            const FTStageProbe& Probe = Probes[Index];

            if (Probe.TargetWord.empty() || IsBlank(Probe.TargetWord)
                || Probe.Translation.empty() || IsBlank(Probe.Translation))
            {
                continue;
            }

            const std::string LoweredWord = ToLowerCase(Probe.TargetWord);
            const std::string WantedTranslation = ToLowerCase(Trim(Probe.Translation));

            for (const StageRow& Row : Rows)
            {
                if (ToLowerCase(Row.TargetWord) != LoweredWord
                    || Row.PartOfSpeech != Probe.PartOfSpeech
                    || Row.Gender != Probe.Gender)
                {
                    continue;
                }

                if (!TranslationListContains(Row.Translation, WantedTranslation))
                {
                    continue;
                }

                if (!Result[Index].has_value() || *Result[Index] < Row.LeitnerStage)
                {
                    Result[Index] = Row.LeitnerStage;
                }
            }
        }
    }

    return Result;
}

std::optional<int> FTCardRepository::FindWordTranslationStageAnyReading(
    const std::string& TargetWord, const std::string& Translation) const
{
    const FTQueryTimer Timer("FTCardRepository::FindWordTranslationStageAnyReading");

    if (TargetWord.empty() || IsBlank(TargetWord)
        || Translation.empty() || IsBlank(Translation))
    {
        return std::nullopt;
    }

    FTStatement Query = Target.Prepare(
        "SELECT Translation, LeitnerStage FROM Cards "
        "WHERE Deleted = 0 AND TargetWord = ?1 COLLATE NOCASE "
        "  AND " + Pair.Clause("Cards"));

    if (!Query.IsValid())
    {
        return std::nullopt;
    }

    Query.BindText(1, TargetWord);
    Pair.Bind(Query);

    const std::string Wanted = ToLowerCase(Trim(Translation));

    std::optional<int> Furthest;

    while (Query.Step())
    {
        if (TranslationListContains(Query.GetText(0), Wanted))
        {
            const int Stage = Query.GetInt(1);
            Furthest = Furthest.has_value() ? std::max(*Furthest, Stage) : Stage;
        }
    }

    return Furthest;
}

std::optional<FTWordTranslationMatch> FTCardRepository::FindWordTranslationMatch(
    const std::string& TargetWord, const std::string& PartOfSpeech, const std::string& Translation) const
{
    const FTQueryTimer Timer("FTCardRepository::FindWordTranslationMatch");

    if (TargetWord.empty() || IsBlank(TargetWord)
        || Translation.empty() || IsBlank(Translation))
    {
        return std::nullopt;
    }

    FTStatement Query = Target.Prepare(
        "SELECT Translation, LeitnerStage, Id, PartOfSpeech, Gender FROM Cards "
        "WHERE Deleted = 0 AND TargetWord = ?1 COLLATE NOCASE "
        "  AND (PartOfSpeech = ?2 OR PartOfSpeech = '') "
        "  AND " + Pair.Clause("Cards"));

    if (!Query.IsValid())
    {
        return std::nullopt;
    }

    Query.BindText(1, TargetWord);
    Query.BindText(2, PartOfSpeech);
    Pair.Bind(Query);

    const std::string Wanted = ToLowerCase(Trim(Translation));

    std::optional<FTWordTranslationMatch> Furthest;

    while (Query.Step())
    {
        if (!TranslationListContains(Query.GetText(0), Wanted))
        {
            continue;
        }

        const int Stage = Query.GetInt(1);

        if (!Furthest.has_value() || Stage > Furthest->Stage)
        {
            Furthest = FTWordTranslationMatch{Stage, Query.GetText(2), Query.GetText(3), ReadOptionalInt(Query, 4)};
        }
    }

    return Furthest;
}

std::vector<FTCardHandle> FTCardRepository::LoadDueCards(FTDayNumber Today) const
{
    const FTQueryTimer Timer("FTCardRepository::LoadDueCards");

    std::vector<FTCardHandle> Result;

    FTStatement Query = Target.Prepare(
        std::string("SELECT ") + CardColumns + " FROM Cards "
        "WHERE Deleted = 0 AND CompletedDate IS NULL AND LeitnerStage > 0 "
        "  AND DueDate IS NOT NULL AND DueDate <= ?1 "
        "  AND " + Pair.Clause("Cards") + " "
        "ORDER BY DueDate ASC, TargetWord ASC");

    if (!Query.IsValid())
    {
        return Result;
    }

    Query.BindInt(1, static_cast<int>(Today));
    Pair.Bind(Query);

    return ReadCards(Query);
}

std::vector<FTCardHandle> FTCardRepository::LoadCardsDueOnDay(FTDayNumber Day) const
{
    const FTQueryTimer Timer("FTCardRepository::LoadCardsDueOnDay");

    std::vector<FTCardHandle> Result;

    FTStatement Query = Target.Prepare(
        std::string("SELECT ") + CardColumns + " FROM Cards "
        "WHERE Deleted = 0 AND CompletedDate IS NULL AND LeitnerStage > 0 "
        "  AND DueDate = ?1 "
        "  AND " + Pair.Clause("Cards") + " "
        "ORDER BY TargetWord ASC");

    if (!Query.IsValid())
    {
        return Result;
    }

    Query.BindInt(1, static_cast<int>(Day));
    Pair.Bind(Query);

    return ReadCards(Query);
}

int FTCardRepository::CountDueCards(FTDayNumber Today) const
{
    const FTQueryTimer Timer("FTCardRepository::CountDueCards");

    // The filter has to match LoadDueCards exactly, or a count on screen would promise a different
    // number of cards than the session then works through.
    FTStatement Query = Target.Prepare(
        "SELECT COUNT(*) FROM Cards "
        "WHERE Deleted = 0 AND CompletedDate IS NULL AND LeitnerStage > 0 "
        "  AND DueDate IS NOT NULL AND DueDate <= ?1 "
        "  AND " + Pair.Clause("Cards"));

    if (!Query.IsValid())
    {
        return 0;
    }

    Query.BindInt(1, static_cast<int>(Today));
    Pair.Bind(Query);

    if (!Query.Step())
    {
        return 0;
    }

    return Query.GetInt(0);
}

int FTCardRepository::CountCards() const
{
    const FTQueryTimer Timer("FTCardRepository::CountCards");

    FTStatement Query = Target.Prepare(
        "SELECT COUNT(*) FROM Cards WHERE Deleted = 0 AND " + Pair.Clause("Cards"));

    if (!Query.IsValid())
    {
        return 0;
    }

    Pair.Bind(Query);

    if (!Query.Step())
    {
        return 0;
    }

    return Query.GetInt(0);
}

int FTCardRepository::CountUnlearnedCards() const
{
    const FTQueryTimer Timer("FTCardRepository::CountUnlearnedCards");

    // Matches LoadUnlearnedCards, for the same reason CountDueCards matches LoadDueCards.
    FTStatement Query = Target.Prepare(
        "SELECT COUNT(*) FROM Cards "
        "WHERE Deleted = 0 AND CompletedDate IS NULL AND LeitnerStage = 0 "
        "  AND " + Pair.Clause("Cards"));

    if (!Query.IsValid())
    {
        return 0;
    }

    Pair.Bind(Query);

    if (!Query.Step())
    {
        return 0;
    }

    return Query.GetInt(0);
}

std::vector<FTCardHandle> FTCardRepository::LoadUnlearnedCards() const
{
    const FTQueryTimer Timer("FTCardRepository::LoadUnlearnedCards");

    std::vector<FTCardHandle> Result;

    FTStatement Query = Target.Prepare(
        std::string("SELECT ") + CardColumns + " FROM Cards "
        "WHERE Deleted = 0 AND CompletedDate IS NULL AND LeitnerStage = 0 "
        "  AND " + Pair.Clause("Cards") + " "
        "ORDER BY CreatedDate ASC, rowid ASC");

    if (!Query.IsValid())
    {
        return Result;
    }

    Pair.Bind(Query);

    return ReadCards(Query);
}

std::vector<FTStudiedHeadword> FTCardRepository::ListStudiedHeadwords() const
{
    const FTQueryTimer Timer("FTCardRepository::ListStudiedHeadwords");

    std::vector<FTStudiedHeadword> Result;

    FTStatement Query = Target.Prepare(
        "SELECT TargetWord, PartOfSpeech, Translation, MIN(CreatedDate) FROM Cards "
        "WHERE Deleted = 0 AND (LeitnerStage > 0 OR CompletedDate IS NOT NULL) "
        "  AND " + Pair.Clause("Cards") + " "
        "GROUP BY TargetWord, PartOfSpeech "
        "ORDER BY MIN(CreatedDate) ASC, TargetWord ASC");

    if (!Query.IsValid())
    {
        return Result;
    }

    Pair.Bind(Query);

    while (Query.Step())
    {
        Result.push_back(FTStudiedHeadword{Query.GetText(0), Query.GetText(1), Query.GetText(2)});
    }

    return Result;
}

std::vector<std::string> FTCardRepository::ListKnownHeadwords() const
{
    const FTQueryTimer Timer("FTCardRepository::ListKnownHeadwords");

    std::vector<std::string> Result;

    FTStatement Query = Target.Prepare(
        "SELECT TargetWord FROM Cards "
        "WHERE Deleted = 0 AND (LeitnerStage > 0 OR CompletedDate IS NOT NULL) "
        "  AND " + Pair.Clause("Cards") + " "
        "UNION "
        "SELECT TargetWord FROM TriagedWords "
        "WHERE TargetLanguage = ?90 "
        "  AND NOT EXISTS (SELECT 1 FROM Cards "
        "                  WHERE Cards.Deleted = 0 "
        "                    AND TriagedWords.TargetWord = Cards.TargetWord "
        "                    AND TriagedWords.PartOfSpeech = Cards.PartOfSpeech "
        "                    AND TriagedWords.Gender = IFNULL(Cards.Gender, -1) "
        "                    AND " + Pair.Clause("Cards") + ")");

    if (!Query.IsValid())
    {
        return Result;
    }

    Pair.Bind(Query);
    Query.BindText(TargetLanguageParameter, Pair.GetTargetLanguage());

    while (Query.Step())
    {
        Result.push_back(Query.GetText(0));
    }

    return Result;
}

std::vector<FTKnownVerbTense> FTCardRepository::ListKnownVerbTenses() const
{
    const FTQueryTimer Timer("FTCardRepository::ListKnownVerbTenses");

    std::vector<FTKnownVerbTense> Result;

    for (const FTCard& Card : LoadFormsRows())
    {
        std::string Tense = GetFormsTense(Card.Id);

        if (IsCompleted(Card) && !Tense.empty())
        {
            Result.push_back(FTKnownVerbTense{Card.TargetWord, std::move(Tense)});
        }
    }

    return Result;
}

bool FTCardRepository::AddFormsCards(const std::vector<FTCard>& Cards)
{
    const FTQueryTimer Timer("FTCardRepository::AddFormsCards");

    if (!Pair.IsSet())
    {
        return false;
    }

    TransactionGuard Transaction(Target);

    if (!Transaction.IsActive())
    {
        return false;
    }

    const int64_t WriteTime = Now();

    for (const FTCard& Card : Cards)
    {
        if (Card.Id.empty() || Card.TargetWord.empty() || IsBlank(Card.TargetWord))
        {
            return false;
        }

        FTStatement Write = Target.Prepare(
            "INSERT INTO WordForms (Id, TargetWord, Translation, TargetLanguage, NativeLanguage, "
            "                       LearnedDate, CreatedDate, LastUpdatedDate, Deleted) "
            "VALUES (?1, ?2, ?3, ?90, ?91, NULL, ?4, ?4, 0) "
            "ON CONFLICT(Id) DO NOTHING");

        if (!Write.IsValid())
        {
            return false;
        }

        Write.BindText(1, Card.Id);
        Write.BindText(2, Card.TargetWord);
        Write.BindText(3, ClampFieldLength(Card.Translation));
        Write.BindInt64(4, WriteTime);
        Pair.Bind(Write);

        if (!Write.Run())
        {
            return false;
        }
    }

    return Transaction.Commit();
}

bool FTCardRepository::MarkFormsCardsLearned(const std::vector<std::string>& CardIds, FTDayNumber Today)
{
    const FTQueryTimer Timer("FTCardRepository::MarkFormsCardsLearned");

    TransactionGuard Transaction(Target);

    if (!Transaction.IsActive())
    {
        return false;
    }

    const int64_t WriteTime = Now();

    for (const std::string& CardId : CardIds)
    {
        FTStatement Update = Target.Prepare(
            "UPDATE WordForms SET LearnedDate = ?2, LastUpdatedDate = ?3 "
            "WHERE Id = ?1 AND Deleted = 0 AND LearnedDate IS NULL");

        if (!Update.IsValid())
        {
            return false;
        }

        Update.BindText(1, CardId);
        BindOptionalDay(Update, 2, Today);
        Update.BindInt64(3, WriteTime);

        if (!Update.Run())
        {
            return false;
        }
    }

    return Transaction.Commit();
}

std::optional<FTCardHandle> FTCardRepository::LoadFormsCard(const std::string& CardId) const
{
    const FTQueryTimer Timer("FTCardRepository::LoadFormsCard");

    FTStatement Query = Target.Prepare(
        std::string("SELECT ") + FormsCardColumns + " FROM WordForms "
        "WHERE Id = ?1 AND Deleted = 0 AND " + Pair.Clause("WordForms"));

    if (!Query.IsValid())
    {
        return std::nullopt;
    }

    Query.BindText(1, CardId);
    Pair.Bind(Query);

    if (!Query.Step())
    {
        return std::nullopt;
    }

    return MakeCardHandle(WithStudiedTranslation(ReadFormsCard(Query)));
}

FTCard FTCardRepository::WithStudiedTranslation(FTCard Card) const
{
    FTStatement Query = Target.Prepare(
        "SELECT Translation FROM Cards "
        "WHERE Deleted = 0 AND TargetWord = ?1 AND PartOfSpeech = ?2 "
        "  AND (LeitnerStage > 0 OR CompletedDate IS NOT NULL) "
        "  AND " + Pair.Clause("Cards") + " "
        "ORDER BY CreatedDate ASC LIMIT 1");

    if (!Query.IsValid())
    {
        return Card;
    }

    Query.BindText(1, Card.TargetWord);
    Query.BindText(2, GetFormsKind(Card.Id));
    Pair.Bind(Query);

    if (Query.Step())
    {
        Card.Translation = Query.GetText(0);
    }

    return Card;
}

std::vector<FTCard> FTCardRepository::LoadFormsRows() const
{
    std::vector<FTCard> Rows;

    FTStatement Query = Target.Prepare(
        std::string("SELECT ") + FormsCardColumns + " FROM WordForms "
        "WHERE Deleted = 0 AND " + Pair.Clause("WordForms") + " "
        "ORDER BY CreatedDate ASC, rowid ASC");

    if (!Query.IsValid())
    {
        return Rows;
    }

    Pair.Bind(Query);

    while (Query.Step())
    {
        Rows.push_back(ReadFormsCard(Query));
    }

    return Rows;
}

std::vector<FTCardHandle> FTCardRepository::LoadFormsCards() const
{
    const FTQueryTimer Timer("FTCardRepository::LoadFormsCards");

    std::vector<FTCardHandle> Result;

    for (FTCard& Row : LoadFormsRows())
    {
        Result.push_back(MakeCardHandle(WithStudiedTranslation(std::move(Row))));
    }

    return Result;
}

bool FTCardRepository::UpdateCardContent(const std::string& CardId,
                                        const std::string& TargetWord,
                                        const std::string& Translation,
                                        const std::optional<int>& Gender,
                                        const std::string& PartOfSpeech,
                                        const std::string& MeaningGist, const std::string& Hint)
{
    const FTQueryTimer Timer("FTCardRepository::UpdateCardContent");

    const std::string TrimmedWord = ClampFieldLength(Trim(TargetWord));
    const std::string TrimmedTranslation = ClampFieldLength(Trim(Translation));

    if (CardId.empty() || TrimmedWord.empty() || TrimmedTranslation.empty())
    {
        return false;
    }

    FTStatement Update = Target.Prepare(
        "UPDATE Cards SET TargetWord = ?2, Translation = ?3, Gender = ?5, PartOfSpeech = ?6, "
        "                 UserEditedFields = UserEditedFields | ?7, LastUpdatedDate = ?4, MeaningGist = ?8, Hint = ?9 "
        "WHERE Id = ?1 AND Deleted = 0");

    if (!Update.IsValid())
    {
        return false;
    }

    Update.BindText(1, CardId);
    Update.BindText(2, TrimmedWord);
    Update.BindText(3, TrimmedTranslation);
    Update.BindInt64(4, Now());
    BindOptionalInt(Update, 5, Gender);
    Update.BindText(6, PartOfSpeech);
    Update.BindInt(7, static_cast<int>(EFTCardField::Gender) | static_cast<int>(EFTCardField::PartOfSpeech));
    Update.BindText(8, ClampFieldLength(MeaningGist));
    Update.BindText(9, Hint);

    if (!Update.Run() || Update.GetChangeCount() == 0)
    {
        return false;
    }

    RefreshStoredCard(CardId);

    return true;
}

bool FTCardRepository::UpdateCardSchedule(const FTCard& Word)
{
    const FTQueryTimer Timer("FTCardRepository::UpdateCardSchedule");

    if (Word.Id.empty())
    {
        return false;
    }

    FTStatement Update = Target.Prepare(
        "UPDATE Cards SET LeitnerStage = ?2, DueDate = ?3, BoxChangedDate = ?4, CompletedDate = ?5 "
        "WHERE Id = ?1 AND Deleted = 0");

    if (!Update.IsValid())
    {
        return false;
    }

    Update.BindText(1, Word.Id);
    Update.BindInt(2, Word.LeitnerStage);
    BindOptionalDay(Update, 3, Word.DueDate);
    BindOptionalDay(Update, 4, Word.BoxChangedDate);
    BindOptionalDay(Update, 5, Word.CompletedDate);

    if (!Update.Run() || Update.GetChangeCount() == 0)
    {
        return false;
    }

    RefreshStoredCard(Word.Id);

    return true;
}

bool FTCardRepository::ReturnCardToPool(const std::string& CardId)
{
    return RecordProgressEvent(CardId, JournalPooled, 0, GetTodayDayNumber());
}

bool FTCardRepository::MarkCardCompleted(const std::string& CardId)
{
    return RecordProgressEvent(CardId, JournalCompleted, 0, GetTodayDayNumber());
}

bool FTCardRepository::RecordProgressEvent(const std::string& CardId, const char* EventId, int EventValue,
                                           FTDayNumber Day)
{
    const FTQueryTimer Timer("FTCardRepository::RecordProgressEvent");

    const std::optional<FTCardHandle> Loaded = LoadCard(CardId);

    if (!Loaded.has_value())
    {
        return false;
    }

    return ApplyProgressEvent(**Loaded, EventId, EventValue, Day);
}

bool FTCardRepository::ApplyProgressEvent(const FTCard& Loaded, const char* EventId, int EventValue, FTDayNumber Day)
{
    const FTCard Before = Loaded;
    FTCard Word = Before;

    FTJournalEvent Event = MakeJournalEvent(Before.Id, EventId, EventValue, Day);
    ApplyJournalEvent(Word, Event);
    Event.DueDate = Word.DueDate;

    if (!AppendJournalEvent(Target, Event) || !UpdateCardSchedule(Word))
    {
        return false;
    }

    LogCardMove(Before, Word, QStringLiteral("%1 %2").arg(QLatin1String(EventId)).arg(EventValue));

    return true;
}

bool FTCardRepository::PromoteCards(const std::vector<std::string>& CardIds)
{
    const FTQueryTimer Timer("FTCardRepository::PromoteCards");

    if (CardIds.empty())
    {
        return false;
    }

    const FTDayNumber Today = GetTodayDayNumber();

    TransactionGuard Transaction(Target);

    if (!Transaction.IsActive())
    {
        return false;
    }

    for (const std::string& CardId : CardIds)
    {
        const std::optional<FTCardHandle> Loaded = LoadCard(CardId);

        if (Loaded.has_value() && !ApplyProgressEvent(**Loaded, JournalLearned, 0, Today))
        {
            return false;
        }
    }

    return Transaction.Commit();
}

std::optional<std::string> FTCardRepository::AddCard(const FTCard& Word)
{
    const FTQueryTimer Timer("FTCardRepository::AddCard");

    if (Word.TargetWord.empty() || IsBlank(Word.TargetWord))
    {
        return std::nullopt;
    }

    const std::string CardId = MakeUuid();

    FTStatement Insert = Target.Prepare(
        "INSERT INTO Cards (Id, TargetWord, Translation, Definition, Gender, UserEditedFields, "
        "                   LeitnerStage, DueDate, BoxChangedDate, CompletedDate, "
        "                   CreatedDate, LastUpdatedDate, Deleted, PartOfSpeech, MeaningGist, Hint, "
        "                   TargetLanguage, NativeLanguage) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, 0, NULL, NULL, NULL, ?7, ?7, 0, ?8, ?9, ?10, ?90, ?91)");

    if (!Insert.IsValid())
    {
        return std::nullopt;
    }

    Insert.BindText(1, CardId);
    Insert.BindText(2, ClampFieldLength(Word.TargetWord));
    Insert.BindText(3, ClampFieldLength(Word.Translation));
    Insert.BindText(4, Word.Definition);
    BindOptionalInt(Insert, 5, Word.Gender);
    Insert.BindInt(6, static_cast<int>(Word.UserEditedFields));
    Insert.BindInt64(7, Now());
    Insert.BindText(8, Word.PartOfSpeech);
    Insert.BindText(9, Word.MeaningGist);
    Insert.BindText(10, Word.Hint);
    Insert.BindText(TargetLanguageParameter, Pair.GetTargetLanguage());
    Insert.BindText(NativeLanguageParameter, Pair.GetNativeLanguage());

    if (!Insert.Run())
    {
        return std::nullopt;
    }

    return CardId;
}

bool FTCardRepository::DeleteCards(const std::vector<std::string>& CardIds)
{
    const FTQueryTimer Timer("FTCardRepository::DeleteCards");

    TransactionGuard Transaction(Target);

    if (!Transaction.IsActive())
    {
        return false;
    }

    const int64_t WriteTime = Now();

    for (const std::string& CardId : CardIds)
    {
        FTStatement Update = Target.Prepare(
            "UPDATE Cards SET Deleted = 1, LastUpdatedDate = ?2 WHERE Id = ?1 AND Deleted = 0");

        if (!Update.IsValid())
        {
            return false;
        }

        Update.BindText(1, CardId);
        Update.BindInt64(2, WriteTime);

        if (!Update.Run())
        {
            return false;
        }
    }

    return Transaction.Commit();
}

namespace
{
    /** -1 stands for "no gender" in the triage table, since a NULL in a primary key would let the
        same reading be recorded twice. */
    int GenderKey(const std::optional<int>& Gender)
    {
        return Gender.value_or(-1);
    }
}

bool FTCardRepository::MarkWordTriaged(const std::string& TargetWord,
                                      const std::string& PartOfSpeech,
                                      const std::optional<int>& Gender)
{
    const FTQueryTimer Timer("FTCardRepository::MarkWordTriaged");

    const std::string Trimmed = Trim(TargetWord);

    if (Trimmed.empty())
    {
        return false;
    }

    // Replaced rather than refused on a repeat: being asked twice about one reading is not an error,
    // and the later date is the more useful one.
    FTStatement Insert = Target.Prepare(
        "INSERT INTO TriagedWords "
        "(TargetWord, TargetLanguage, PartOfSpeech, Gender, MarkedDate) "
        "VALUES (?1, ?90, ?2, ?3, ?4) "
        "ON CONFLICT(TargetWord, TargetLanguage, PartOfSpeech, Gender) "
        "DO UPDATE SET MarkedDate = ?4");

    if (!Insert.IsValid())
    {
        return false;
    }

    Insert.BindText(1, Trimmed);
    Insert.BindText(2, PartOfSpeech);
    Insert.BindInt(3, GenderKey(Gender));
    Insert.BindInt64(4, Now());

    // Only the language being learned. Whether the user knows a word does not depend on which
    // language they would translate it into, so the native language is not part of this.
    Insert.BindText(TargetLanguageParameter, Pair.GetTargetLanguage());

    return Insert.Run();
}

bool FTCardRepository::IsWordTriaged(const std::string& TargetWord,
                                    const std::string& PartOfSpeech,
                                    const std::optional<int>& Gender) const
{
    const FTQueryTimer Timer("FTCardRepository::IsWordTriaged");

    const std::string Trimmed = Trim(TargetWord);

    if (Trimmed.empty())
    {
        return false;
    }

    FTStatement Query = Target.Prepare(
        "SELECT 1 FROM TriagedWords "
        "WHERE TargetWord = ?1 AND PartOfSpeech = ?2 AND Gender = ?3 "
        "  AND TargetLanguage = ?90");

    if (!Query.IsValid())
    {
        return false;
    }

    Query.BindText(1, Trimmed);
    Query.BindText(2, PartOfSpeech);
    Query.BindInt(3, GenderKey(Gender));
    Query.BindText(TargetLanguageParameter, Pair.GetTargetLanguage());

    return Query.Step();
}

int FTCardRepository::CountTriagedWords() const
{
    const FTQueryTimer Timer("FTCardRepository::CountTriagedWords");

    FTStatement Query = Target.Prepare(
        "SELECT COUNT(*) FROM TriagedWords WHERE TargetLanguage = ?90");

    if (!Query.IsValid())
    {
        return 0;
    }

    Query.BindText(TargetLanguageParameter, Pair.GetTargetLanguage());

    if (!Query.Step())
    {
        return 0;
    }

    return Query.GetInt(0);
}

std::vector<FTKnownWord> FTCardRepository::ListKnownWords() const
{
    const FTQueryTimer Timer("FTCardRepository::ListKnownWords");

    std::vector<FTKnownWord> Result;

    FTStatement Query = Target.Prepare(
        "SELECT DISTINCT TargetWord, PartOfSpeech FROM TriagedWords "
        "WHERE TargetLanguage = ?90 "
        "  AND NOT EXISTS (SELECT 1 FROM Cards "
        "                  WHERE Cards.Deleted = 1 "
        "                    AND Cards.TargetWord = TriagedWords.TargetWord COLLATE NOCASE "
        "                    AND Cards.PartOfSpeech = TriagedWords.PartOfSpeech "
        "                    AND Cards.LastUpdatedDate >= TriagedWords.MarkedDate "
        "                    AND " + Pair.Clause("Cards") + ")");

    if (!Query.IsValid())
    {
        return Result;
    }

    Pair.Bind(Query);
    Query.BindText(TargetLanguageParameter, Pair.GetTargetLanguage());

    while (Query.Step())
    {
        Result.push_back(FTKnownWord{Query.GetText(0), Query.GetText(1)});
    }

    return Result;
}

bool FTCardRepository::IsWordUnseen(const std::string& TargetWord,
                                    const std::string& PartOfSpeech,
                                    const std::optional<int>& Gender,
                                    const std::string& Translation) const
{
    if (IsWordTriaged(TargetWord, PartOfSpeech, Gender))
    {
        return false;
    }

    // The spelling, the reading and the translation together are what tell us this exact reading
    // is already being learned, not merely that some reading of the same spelling is.
    return !FindWordTranslationStage(TargetWord, PartOfSpeech, Gender, Translation).has_value();
}

bool FTCardRepository::IsWordOwnedOrTriaged(const std::string& TargetWord,
                                            const std::string& PartOfSpeech) const
{
    const FTQueryTimer Timer("FTCardRepository::IsWordOwnedOrTriaged");

    const std::string Trimmed = Trim(TargetWord);

    if (Trimmed.empty())
    {
        return false;
    }

    FTStatement Query = Target.Prepare(
        "SELECT 1 FROM TriagedWords "
        "WHERE TargetWord = ?1 AND PartOfSpeech = ?2 AND TargetLanguage = ?90 "
        "UNION ALL "
        "SELECT 1 FROM Cards "
        "WHERE Deleted = 0 AND TargetWord = ?1 COLLATE NOCASE AND PartOfSpeech = ?2 "
        "  AND " + Pair.Clause("Cards") + " "
        "LIMIT 1");

    if (!Query.IsValid())
    {
        return false;
    }

    Query.BindText(1, Trimmed);
    Query.BindText(2, PartOfSpeech);

    Pair.Bind(Query);
    Query.BindText(TargetLanguageParameter, Pair.GetTargetLanguage());

    return Query.Step();
}

bool FTCardRepository::SetPlacementCutoff(const std::string& TargetLanguage, int CutoffRank)
{
    const FTQueryTimer Timer("FTCardRepository::SetPlacementCutoff");

    if (TargetLanguage.empty())
    {
        return false;
    }

    FTStatement Upsert = Target.Prepare(
        "INSERT INTO Placements (TargetLanguage, CutoffRank, LastUpdatedDate) "
        "VALUES (?1, ?2, ?3) "
        "ON CONFLICT(TargetLanguage) DO UPDATE SET CutoffRank = ?2, LastUpdatedDate = ?3");

    if (!Upsert.IsValid())
    {
        return false;
    }

    Upsert.BindText(1, TargetLanguage);
    Upsert.BindInt(2, std::max(CutoffRank, 0));
    Upsert.BindInt64(3, Now());

    return Upsert.Run();
}

std::optional<int> FTCardRepository::FindPlacementCutoff(const std::string& TargetLanguage) const
{
    const FTQueryTimer Timer("FTCardRepository::FindPlacementCutoff");

    FTStatement Query = Target.Prepare("SELECT CutoffRank FROM Placements WHERE TargetLanguage = ?1");

    if (!Query.IsValid())
    {
        return std::nullopt;
    }

    Query.BindText(1, TargetLanguage);

    if (!Query.Step())
    {
        return std::nullopt;
    }

    return Query.GetInt(0);
}

std::optional<FTStudiedLanguage> FTCardRepository::FindLastStudiedLanguage() const
{
    const FTQueryTimer Timer("FTCardRepository::FindLastStudiedLanguage");

    FTStatement Query = Target.Prepare(
        "SELECT TargetLanguage, NativeLanguage FROM Cards "
        "WHERE Deleted = 0 AND TargetLanguage <> '' AND NativeLanguage <> '' "
        "ORDER BY LastUpdatedDate DESC LIMIT 1");

    if (!Query.IsValid() || !Query.Step())
    {
        return std::nullopt;
    }

    return FTStudiedLanguage{Query.GetText(0), Query.GetText(1)};
}

int FTCardRepository::GetPlacementCutoff() const
{
    return FindPlacementCutoff(Pair.GetTargetLanguage()).value_or(0);
}

bool FTCardRepository::RecordSearch(const std::string& Term, const std::string& PartOfSpeech,
                                    int64_t WordId)
{
    const FTQueryTimer Timer("FTCardRepository::RecordSearch");

    if (Term.empty() || IsBlank(Term))
    {
        return false;
    }

    FTStatement Upsert = Target.Prepare(
        "INSERT INTO SearchHistory (Term, TargetLanguage, PartOfSpeech, WordId, SearchedAt) "
        "VALUES (?1, ?90, ?3, ?4, ?2) "
        "ON CONFLICT (Term, TargetLanguage) DO UPDATE SET "
        "  SearchedAt = ?2, PartOfSpeech = ?3, WordId = ?4");

    if (!Upsert.IsValid())
    {
        return false;
    }

    Upsert.BindText(1, Term);
    Upsert.BindInt64(2, Now());
    Upsert.BindText(3, PartOfSpeech);
    Upsert.BindInt64(4, WordId);
    Upsert.BindText(TargetLanguageParameter, Pair.GetTargetLanguage());

    return Upsert.Run();
}

std::vector<FTSearchHistoryEntry> FTCardRepository::LoadRecentSearches(int Limit) const
{
    const FTQueryTimer Timer("FTCardRepository::LoadRecentSearches");

    std::vector<FTSearchHistoryEntry> Result;

    if (Limit <= 0)
    {
        return Result;
    }

    FTStatement Query = Target.Prepare(
        "SELECT Term, PartOfSpeech, WordId FROM SearchHistory WHERE TargetLanguage = ?90 "
        "ORDER BY SearchedAt DESC, Term ASC LIMIT ?1");

    if (!Query.IsValid())
    {
        return Result;
    }

    Query.BindText(TargetLanguageParameter, Pair.GetTargetLanguage());

    Query.BindInt(1, Limit);

    while (Query.Step())
    {
        FTSearchHistoryEntry Entry;
        Entry.Term = Query.GetText(0);
        Entry.PartOfSpeech = Query.GetText(1);
        Entry.WordId = Query.GetInt64(2);

        Result.push_back(std::move(Entry));
    }

    return Result;
}

bool FTCardRepository::RecordDailyProgress(FTDayNumber Day, int LearnedCount, int ReviewedCount,
                                           int DailyGoal)
{
    const FTQueryTimer Timer("FTCardRepository::RecordDailyProgress");

    const int Learned = std::max(LearnedCount, 0);
    const int Reviewed = std::max(ReviewedCount, 0);

    if (Learned == 0 && Reviewed == 0)
    {
        return false;
    }

    FTStatement Upsert = Target.Prepare(
        "INSERT INTO DailyStats (Day, TargetLanguage, NativeLanguage, DeviceId, LearnedCount, "
        "                        ReviewedCount, DueCount, GoalCount, LastUpdatedDate) "
        "VALUES (?1, ?90, ?91, ?7, ?2, ?3, ?5, ?6, ?4) "
        "ON CONFLICT (Day, TargetLanguage, NativeLanguage, DeviceId) DO UPDATE SET "
        "    LearnedCount = LearnedCount + ?2, "
        "    ReviewedCount = ReviewedCount + ?3, "
        "    DueCount = ?5, "
        "    GoalCount = ?6, "
        "    LastUpdatedDate = ?4");

    if (!Upsert.IsValid())
    {
        return false;
    }

    Upsert.BindInt(1, static_cast<int>(Day));
    Upsert.BindInt(2, Learned);
    Upsert.BindInt(3, Reviewed);
    Upsert.BindInt64(4, Now());
    Upsert.BindInt(5, CountDueCards(Day));
    Upsert.BindInt(6, std::max(DailyGoal, 0));
    Upsert.BindText(7, ReadLocalDeviceId(Target));
    Pair.Bind(Upsert);

    return Upsert.Run();
}

std::vector<FTDailyStats> FTCardRepository::LoadDailyStats(FTDayNumber FirstDay,
                                                           FTDayNumber LastDay) const
{
    const FTQueryTimer Timer("FTCardRepository::LoadDailyStats");

    std::vector<FTDailyStats> Result;

    // A day is the sum of its devices' rows. The two counts that are replaced on every write, what
    // is still owed and the goal, come from the device that wrote last: with MAX(), SQLite takes
    // every other bare column from the row that holds the maximum.
    FTStatement Query = Target.Prepare(
        "SELECT Day, SUM(LearnedCount), SUM(ReviewedCount), DueCount, GoalCount, "
        "       MAX(LastUpdatedDate) FROM DailyStats "
        "WHERE Day BETWEEN ?1 AND ?2 AND TargetLanguage = ?90 AND NativeLanguage = ?91 "
        "GROUP BY Day ORDER BY Day ASC");

    if (!Query.IsValid())
    {
        return Result;
    }

    Query.BindInt(1, static_cast<int>(FirstDay));
    Query.BindInt(2, static_cast<int>(LastDay));
    Pair.Bind(Query);

    while (Query.Step())
    {
        FTDailyStats Entry;
        Entry.Day = static_cast<FTDayNumber>(Query.GetInt(0));
        Entry.LearnedCount = Query.GetInt(1);
        Entry.ReviewedCount = Query.GetInt(2);
        Entry.DueCount = Query.GetInt(3);
        Entry.GoalCount = Query.GetInt(4);

        Result.push_back(Entry);
    }

    return Result;
}

std::vector<FTDueCountForDay> FTCardRepository::CountDueCardsByDay(FTDayNumber FirstDay,
                                                                   FTDayNumber LastDay) const
{
    const FTQueryTimer Timer("FTCardRepository::CountDueCardsByDay");

    std::vector<FTDueCountForDay> Result;

    // The same conditions LoadDueCards uses, so a day's figure cannot promise cards a session would
    // not then work through.
    FTStatement Query = Target.Prepare(
        "SELECT DueDate, COUNT(*) FROM Cards "
        "WHERE Deleted = 0 AND CompletedDate IS NULL AND LeitnerStage > 0 "
        "  AND DueDate IS NOT NULL AND DueDate BETWEEN ?1 AND ?2 "
        "  AND " + Pair.Clause("Cards") + " "
        "GROUP BY DueDate ORDER BY DueDate ASC");

    if (!Query.IsValid())
    {
        return Result;
    }

    Query.BindInt(1, static_cast<int>(FirstDay));
    Query.BindInt(2, static_cast<int>(LastDay));
    Pair.Bind(Query);

    while (Query.Step())
    {
        FTDueCountForDay Entry;
        Entry.Day = static_cast<FTDayNumber>(Query.GetInt(0));
        Entry.Count = Query.GetInt(1);

        Result.push_back(Entry);
    }

    return Result;
}

std::vector<FTStageCount> FTCardRepository::CountCardsByStage() const
{
    const FTQueryTimer Timer("FTCardRepository::CountCardsByStage");

    std::vector<FTStageCount> Result;

    FTStatement Query = Target.Prepare(
        "SELECT CompletedDate IS NOT NULL, LeitnerStage, COUNT(*) FROM Cards "
        "WHERE Deleted = 0 AND " + Pair.Clause("Cards") + " "
        "GROUP BY CompletedDate IS NOT NULL, LeitnerStage ORDER BY 1, 2");

    if (!Query.IsValid())
    {
        return Result;
    }

    Pair.Bind(Query);

    while (Query.Step())
    {
        FTStageCount Entry;
        Entry.bCompleted = Query.GetInt(0) != 0;
        Entry.Stage = Query.GetInt(1);
        Entry.Count = Query.GetInt(2);

        Result.push_back(Entry);
    }

    return Result;
}

bool FTCardRepository::DeleteEverything()
{
    const FTQueryTimer Timer("FTCardRepository::DeleteEverything");

    TransactionGuard Transaction(Target);

    if (!Transaction.IsActive())
    {
        return false;
    }

    if (!Target.Execute("DELETE FROM Journal")
        || !Target.Execute("DELETE FROM Cards")
        || !Target.Execute("DELETE FROM WordForms")
        || !Target.Execute("DELETE FROM SearchHistory")
        || !Target.Execute("DELETE FROM TriagedWords")
        || !Target.Execute("DELETE FROM Placements")
        || !Target.Execute("DELETE FROM DailyStats"))
    {
        return false;
    }

    FTStatement Erasure = Target.Prepare(
        "INSERT INTO Erasures (Scope, ErasedDate) VALUES ('everything', ?1) "
        "ON CONFLICT (Scope) DO UPDATE SET ErasedDate = MAX(ErasedDate, excluded.ErasedDate)");

    if (!Erasure.IsValid())
    {
        return false;
    }

    Erasure.BindInt64(1, Now());

    if (!Erasure.Run())
    {
        return false;
    }

    return Transaction.Commit();
}

std::string FTCardRepository::GetLastError() const
{
    return Target.GetLastError();
}

void AdoptLastStudiedLanguage(const FTCardRepository& Cards, FTSettings& Settings)
{
    if (Settings.HasChosenLanguage())
    {
        return;
    }

    const std::optional<FTStudiedLanguage> Studied = Cards.FindLastStudiedLanguage();

    if (!Studied.has_value())
    {
        return;
    }

    qCInfo(LogDatabase) << "No language was chosen, adopting the one studied last:"
                        << QString::fromStdString(Studied->TargetLanguage) << "->"
                        << QString::fromStdString(Studied->NativeLanguage);

    Settings.AdoptLanguagePair(FTLanguageRules{Studied->TargetLanguage, Studied->NativeLanguage});
}
