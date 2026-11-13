#include "core/FTDictionaryRepository.h"

#include "core/FTDatabase.h"
#include "core/FTDictionaryPackage.h"
#include "core/FTLog.h"
#include "core/FTTextNormalization.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QString>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>

FTDictionaryRepository::FTDictionaryRepository(FTDatabase& InDatabase,
                                               std::string InNativeLanguage)
    : Target(InDatabase)
    , NativeLanguage(std::move(InNativeLanguage))
{
}

const std::string& FTDictionaryRepository::GetNativeLanguage() const
{
    return NativeLanguage;
}

std::optional<int64_t> FTDictionaryRepository::GetNativeLanguageFilter() const
{
    const FTQueryTimer Timer("FTDictionaryRepository::GetNativeLanguageFilter");

    if (NativeLanguage.empty())
    {
        return std::nullopt;
    }

    if (ResolvedNativeLanguageId.has_value())
    {
        return ResolvedNativeLanguageId;
    }

    ResolvedNativeLanguageId = -1;

    FTStatement Query = Target.Prepare("SELECT id FROM languages WHERE code = ?1");

    if (Query.IsValid())
    {
        Query.BindText(1, NativeLanguage);

        if (Query.Step())
        {
            ResolvedNativeLanguageId = Query.GetInt64(0);
        }
    }

    return ResolvedNativeLanguageId;
}

namespace
{
    std::string FoldedExpression(const std::string& Column)
    {
        std::string Expression = Column;

        for (const FAccentFold& Fold : AccentFoldTable())
        {
            Expression = "REPLACE(" + Expression + ", '" + Fold.Utf8 + "', '" + Fold.Base + "')";
        }

        return "LOWER(" + Expression + ")";
    }

    std::string MakeFoldedGlob(const std::string& Folded)
    {
        static const std::unordered_map<char, const char*> Classes = {
            {'a', "[aàáâãäåAÀÁÂÃÄÅ]"},
            {'c', "[cçCÇ]"},
            {'e', "[eèéêëEÈÉÊË]"},
            {'i', "[iìíîïIÌÍÎÏ]"},
            {'n', "[nñNÑ]"},
            {'o', "[oòóôõöOÒÓÔÕÖ]"},
            {'u', "[uùúûüUÙÚÛÜ]"},
            {'y', "[yýÿYÝ]"}
        };

        std::string Pattern;
        Pattern.reserve(Folded.size() * 4);

        for (const char Character : Folded)
        {
            const auto Found = Classes.find(Character);

            if (Found != Classes.end())
            {
                Pattern += Found->second;

                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(Character)) != 0)
            {
                Pattern.push_back('[');
                Pattern.push_back(static_cast<char>(
                    std::tolower(static_cast<unsigned char>(Character))));
                Pattern.push_back(static_cast<char>(
                    std::toupper(static_cast<unsigned char>(Character))));
                Pattern.push_back(']');

                continue;
            }

            if (Character == '*' || Character == '?' || Character == '[' || Character == ']')
            {
                continue;
            }

            Pattern.push_back(Character);
        }

        return Pattern;
    }

    std::vector<FTDictionaryExample> LoadExamples(FTDatabase& Target, int64_t WordId)
    {
        std::vector<FTDictionaryExample> Result;

        FTStatement Query = Target.Prepare(
            "SELECT examples.example, examples.translation, languages.code "
            "FROM examples "
            "LEFT JOIN languages ON languages.id = examples.language_id "
            "WHERE examples.word_id = ?1 "
            "ORDER BY examples.id");

        if (!Query.IsValid())
        {
            return Result;
        }

        Query.BindInt64(1, WordId);

        while (Query.Step())
        {
            FTDictionaryExample Example;
            Example.Text = Query.GetText(0);
            Example.Translation = Query.IsNull(1) ? std::string() : Query.GetText(1);
            Example.LanguageCode = Query.IsNull(2) ? std::string() : Query.GetText(2);

            Result.push_back(std::move(Example));
        }

        return Result;
    }

    std::vector<FTDictionaryForm> LoadForms(FTDatabase& Target, int64_t WordId)
    {
        std::vector<FTDictionaryForm> Result;

        FTStatement Query = Target.Prepare(
            "SELECT form, mood, tense, person, gender, number FROM word_forms "
            "WHERE word_id = ?1 ORDER BY id");

        if (!Query.IsValid())
        {
            return Result;
        }

        Query.BindInt64(1, WordId);

        while (Query.Step())
        {
            FTDictionaryForm Form;
            Form.Text = Query.GetText(0);
            Form.Mood = Query.GetInt(1);
            Form.Tense = Query.GetInt(2);
            Form.Person = Query.GetInt(3);
            Form.Gender = Query.GetInt(4);
            Form.Number = Query.GetInt(5);

            Result.push_back(std::move(Form));
        }

        return Result;
    }

    std::vector<FTDictionaryTranslation> LoadTranslations(FTDatabase& Target,
                                                          const std::string& OwnerCondition,
                                                          int64_t OwnerId,
                                                          const std::optional<int64_t>& LanguageId)
    {
        std::vector<FTDictionaryTranslation> Result;

        const std::string Sql =
            "SELECT translation, probability FROM translations WHERE " + OwnerCondition
            + (LanguageId.has_value() ? " AND language_id = ?2 " : " ")
            + "ORDER BY probability IS NULL, probability DESC, id ASC";

        FTStatement Query = Target.Prepare(Sql);

        if (!Query.IsValid())
        {
            return Result;
        }

        Query.BindInt64(1, OwnerId);

        if (LanguageId.has_value())
        {
            Query.BindInt64(2, *LanguageId);
        }

        while (Query.Step())
        {
            FTDictionaryTranslation Translation;
            Translation.Text = Query.GetText(0);

            if (!Query.IsNull(1))
            {
                Translation.Probability = Query.GetDouble(1);
            }

            Result.push_back(std::move(Translation));
        }

        return Result;
    }

    std::vector<FTDictionarySense> LoadSenses(FTDatabase& Target, int64_t WordId,
                                              const std::optional<int64_t>& LanguageId,
                                              bool bHasMeaningExamples)
    {
        std::vector<FTDictionarySense> Result;

        FTStatement Query = Target.Prepare(
            std::string("SELECT id, definition, gloss, gist, gender")
            + (bHasMeaningExamples ? ", native_definition, example, example_translation" : "")
            + " FROM senses WHERE word_id = ?1 ORDER BY id");

        if (!Query.IsValid())
        {
            return Result;
        }

        Query.BindInt64(1, WordId);

        std::vector<int64_t> SenseIds;

        while (Query.Step())
        {
            FTDictionarySense Sense;
            Sense.Definition = Query.IsNull(1) ? std::string() : Query.GetText(1);
            Sense.Gloss = Query.IsNull(2) ? std::string() : Query.GetText(2);
            Sense.Gist = Query.IsNull(3) ? std::string() : Query.GetText(3);
            Sense.Gender = Query.IsNull(4) ? std::nullopt : std::optional(Query.GetInt(4));

            if (bHasMeaningExamples)
            {
                Sense.NativeDefinition = Query.IsNull(5) ? std::string() : Query.GetText(5);
                Sense.Example = Query.IsNull(6) ? std::string() : Query.GetText(6);
                Sense.ExampleTranslation = Query.IsNull(7) ? std::string() : Query.GetText(7);
            }

            SenseIds.push_back(Query.GetInt64(0));
            Result.push_back(std::move(Sense));
        }

        for (size_t Index = 0; Index < SenseIds.size(); ++Index)
        {
            Result[Index].Translations =
                LoadTranslations(Target, "sense_id = ?1", SenseIds[Index], LanguageId);
        }

        std::stable_sort(Result.begin(), Result.end(),
                         [](const FTDictionarySense& Left, const FTDictionarySense& Right)
                         {
                             const auto Total = [](const FTDictionarySense& Sense)
                             {
                                 double Sum = 0.0;

                                 for (const FTDictionaryTranslation& Translation : Sense.Translations)
                                 {
                                     Sum += Translation.Probability.value_or(0.0);
                                 }

                                 return Sum;
                             };

                             const double LeftTotal = Total(Left);
                             const double RightTotal = Total(Right);

                             if (LeftTotal != RightTotal)
                             {
                                 return LeftTotal > RightTotal;
                             }

                             return Left.Translations.size() > Right.Translations.size();
                         });

        return Result;
    }

    bool HasNativeTranslation(const FTDictionaryEntry& Entry)
    {
        if (!Entry.Translations.empty())
        {
            return true;
        }

        for (const FTDictionarySense& Sense : Entry.Senses)
        {
            if (!Sense.Translations.empty())
            {
                return true;
            }
        }

        return false;
    }

    int TeachingRankOrLast(const FTDictionaryEntry& Entry)
    {
        return Entry.TeachingRank.value_or(std::numeric_limits<int>::max());
    }

    void SortByUsefulness(std::vector<FTDictionaryEntry>& Entries,
                          const std::unordered_set<std::string>& ExactHeadwords)
    {
        const auto IsExact = [&ExactHeadwords](const FTDictionaryEntry& Entry)
        {
            return ExactHeadwords.count(Entry.Headword + "/" + Entry.PartOfSpeech) > 0;
        };

        std::stable_sort(Entries.begin(), Entries.end(),
                         [&IsExact](const FTDictionaryEntry& Left, const FTDictionaryEntry& Right)
                         {
                             const bool bLeftTranslated = HasNativeTranslation(Left);
                             const bool bRightTranslated = HasNativeTranslation(Right);

                             if (bLeftTranslated != bRightTranslated)
                             {
                                 return bLeftTranslated;
                             }

                             const bool bLeftExact = IsExact(Left);
                             const bool bRightExact = IsExact(Right);

                             if (bLeftExact != bRightExact)
                             {
                                 return bLeftExact;
                             }

                             const int LeftRank = TeachingRankOrLast(Left);
                             const int RightRank = TeachingRankOrLast(Right);

                             if (LeftRank != RightRank)
                             {
                                 return LeftRank < RightRank;
                             }

                             if (Left.Headword != Right.Headword)
                             {
                                 return Left.Headword < Right.Headword;
                             }

                             return Left.PartOfSpeech < Right.PartOfSpeech;
                         });
    }

    std::vector<std::string> SplitTags(const std::string& Packed)
    {
        std::vector<std::string> Tags;
        std::string::size_type Start = 0;

        while (Start < Packed.size())
        {
            const std::string::size_type Comma = Packed.find(',', Start);
            const std::string::size_type End =
                Comma == std::string::npos ? Packed.size() : Comma;

            if (End > Start)
            {
                Tags.push_back(Packed.substr(Start, End - Start));
            }

            if (Comma == std::string::npos)
            {
                break;
            }

            Start = Comma + 1;
        }

        return Tags;
    }

    std::vector<FTDictionaryEntry> RunWordQuery(FTDatabase& Target, FTStatement& Query,
                                                const std::optional<int64_t>& LanguageId,
                                                bool bHasMeaningExamples)
    {
        std::vector<FTDictionaryEntry> Result;
        std::vector<int64_t> WordIds;

        while (Query.Step())
        {
            FTDictionaryEntry Entry;
            Entry.Id = Query.GetInt64(0);
            Entry.Headword = Query.GetText(1);
            Entry.PartOfSpeech = Query.GetText(2);
            Entry.Phonetic = Query.IsNull(3) ? std::string() : Query.GetText(3);
            Entry.FrequencyRank = Query.IsNull(4) ? std::nullopt : std::optional(Query.GetInt(4));
            Entry.CefrLevel = Query.IsNull(5) ? std::nullopt : std::optional(Query.GetInt(5));
            Entry.bLlmComplete = Query.GetInt(6) != 0;
            Entry.TeachingRank = Query.IsNull(7) ? std::nullopt : std::optional(Query.GetInt(7));
            Entry.Tags = SplitTags(Query.IsNull(8) ? std::string() : Query.GetText(8));

            WordIds.push_back(Query.GetInt64(0));
            Result.push_back(std::move(Entry));
        }

        for (size_t Index = 0; Index < WordIds.size(); ++Index)
        {
            Result[Index].Senses = LoadSenses(Target, WordIds[Index], LanguageId, bHasMeaningExamples);
            Result[Index].Translations = LoadTranslations(
                Target, "word_id = ?1 AND sense_id IS NULL", WordIds[Index], LanguageId);
            Result[Index].Examples = LoadExamples(Target, WordIds[Index]);
            Result[Index].Forms = LoadForms(Target, WordIds[Index]);
        }

        return Result;
    }
}

std::vector<FTDictionaryEntry> FTDictionaryRepository::LookUp(const std::string& Headword) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LookUp");

    FTStatement ExactQuery = Target.Prepare(
        "SELECT words.id, words.headword, parts_of_speech.code, words.phonetic, "
        "       words.frequency_rank, words.cefr_level, words.llm_complete, "
        "       words.teaching_rank, words.tags "
        "FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE words.headword = ?1");

    FTStatement FormQuery = Target.Prepare(
        "SELECT words.id, words.headword, parts_of_speech.code, words.phonetic, "
        "       words.frequency_rank, words.cefr_level, words.llm_complete, "
        "       words.teaching_rank, words.tags "
        "FROM word_forms "
        "JOIN words ON words.id = word_forms.word_id "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE word_forms.form = ?1");

    if (!ExactQuery.IsValid() || !FormQuery.IsValid())
    {
        return {};
    }

    ExactQuery.BindText(1, Headword);
    FormQuery.BindText(1, Headword);

    std::vector<FTDictionaryEntry> Result =
        RunWordQuery(Target, ExactQuery, GetNativeLanguageFilter(), HasMeaningExamples());

    std::unordered_set<std::string> Seen;
    Seen.reserve(Result.size());

    for (const FTDictionaryEntry& Entry : Result)
    {
        Seen.insert(Entry.Headword + "/" + Entry.PartOfSpeech);
    }

    const std::unordered_set<std::string> ExactHeadwords = Seen;

    for (FTDictionaryEntry& Form : RunWordQuery(Target, FormQuery, GetNativeLanguageFilter(), HasMeaningExamples()))
    {
        if (Seen.insert(Form.Headword + "/" + Form.PartOfSpeech).second)
        {
            Result.push_back(std::move(Form));
        }
    }

    if (!Result.empty())
    {
        SortByUsefulness(Result, ExactHeadwords);

        return Result;
    }

    return LookUpIgnoringAccents(Headword);
}

std::optional<FTDictionaryEntry> FTDictionaryRepository::LookUpForForms(
    const std::string& Headword, const std::string& PartOfSpeech) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LookUpForForms");

    struct FMatch
    {
        int64_t Id = 0;
        std::string Headword;
        std::vector<std::string> Tags;
        std::optional<int> TeachingRank;
        std::optional<int> FrequencyRank;
        bool bExact = false;
        bool bTranslated = false;
    };

    const std::optional<int64_t> LanguageId = GetNativeLanguageFilter();

    const std::string Queries[] = {
        "SELECT words.id, words.headword, parts_of_speech.code, words.teaching_rank, words.frequency_rank, words.tags "
        "FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE words.headword = ?1",

        "SELECT words.id, words.headword, parts_of_speech.code, words.teaching_rank, words.frequency_rank, words.tags "
        "FROM word_forms "
        "JOIN words ON words.id = word_forms.word_id "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE word_forms.form = ?1"
    };

    std::vector<FMatch> Matches;
    std::unordered_set<std::string> Seen;

    for (size_t Index = 0; Index < std::size(Queries); ++Index)
    {
        FTStatement Query = Target.Prepare(Queries[Index]);

        if (!Query.IsValid())
        {
            return std::nullopt;
        }

        Query.BindText(1, Headword);

        while (Query.Step())
        {
            const std::string Found = Query.GetText(1);
            const std::string Part = Query.GetText(2);

            if (!Seen.insert(Found + "/" + Part).second || Part != PartOfSpeech)
            {
                continue;
            }

            FMatch Match;
            Match.Id = Query.GetInt64(0);
            Match.Headword = Found;
            Match.TeachingRank = Query.IsNull(3) ? std::nullopt : std::optional(Query.GetInt(3));
            Match.FrequencyRank = Query.IsNull(4) ? std::nullopt : std::optional(Query.GetInt(4));
            Match.Tags = SplitTags(Query.IsNull(5) ? std::string() : Query.GetText(5));
            Match.bExact = Index == 0;

            Matches.push_back(std::move(Match));
        }
    }

    if (Seen.empty())
    {
        for (FTDictionaryEntry& Entry : LookUpIgnoringAccents(Headword))
        {
            if (Entry.PartOfSpeech == PartOfSpeech)
            {
                return std::move(Entry);
            }
        }

        return std::nullopt;
    }

    if (Matches.empty())
    {
        return std::nullopt;
    }

    for (FMatch& Match : Matches)
    {
        FTStatement Query = Target.Prepare(
            std::string("SELECT 1 FROM translations WHERE ")
            + (LanguageId.has_value() ? "language_id = ?2 AND " : "")
            + "((word_id = ?1 AND sense_id IS NULL) OR sense_id IN (SELECT id FROM senses WHERE word_id = ?1)) "
              "LIMIT 1");

        if (!Query.IsValid())
        {
            return std::nullopt;
        }

        Query.BindInt64(1, Match.Id);

        if (LanguageId.has_value())
        {
            Query.BindInt64(2, *LanguageId);
        }

        Match.bTranslated = Query.Step();
    }

    const auto Best = std::min_element(Matches.begin(), Matches.end(), [](const FMatch& Left, const FMatch& Right)
    {
        if (Left.bTranslated != Right.bTranslated)
        {
            return Left.bTranslated;
        }

        if (Left.bExact != Right.bExact)
        {
            return Left.bExact;
        }

        const int LeftRank = Left.TeachingRank.value_or(std::numeric_limits<int>::max());
        const int RightRank = Right.TeachingRank.value_or(std::numeric_limits<int>::max());

        if (LeftRank != RightRank)
        {
            return LeftRank < RightRank;
        }

        return Left.Headword < Right.Headword;
    });

    FTDictionaryEntry Entry;
    Entry.Id = Best->Id;
    Entry.Headword = Best->Headword;
    Entry.PartOfSpeech = PartOfSpeech;
    Entry.TeachingRank = Best->TeachingRank;
    Entry.FrequencyRank = Best->FrequencyRank;
    Entry.Tags = Best->Tags;
    Entry.Forms = LoadForms(Target, Best->Id);

    FTStatement Senses = Target.Prepare(
        std::string("SELECT gender, "
                    "COALESCE((SELECT SUM(COALESCE(probability, 0.0)) FROM translations WHERE sense_id = senses.id")
        + (LanguageId.has_value() ? " AND language_id = ?2" : "")
        + "), 0.0), "
          "(SELECT COUNT(*) FROM translations WHERE sense_id = senses.id"
        + (LanguageId.has_value() ? " AND language_id = ?2" : "")
        + ") "
          "FROM senses WHERE word_id = ?1 ORDER BY id");

    if (!Senses.IsValid())
    {
        return std::nullopt;
    }

    Senses.BindInt64(1, Best->Id);

    if (LanguageId.has_value())
    {
        Senses.BindInt64(2, *LanguageId);
    }

    std::vector<std::pair<double, int>> Weights;

    while (Senses.Step())
    {
        FTDictionarySense Sense;
        Sense.Gender = Senses.IsNull(0) ? std::nullopt : std::optional(Senses.GetInt(0));

        Weights.emplace_back(Senses.GetDouble(1), Senses.GetInt(2));
        Entry.Senses.push_back(std::move(Sense));
    }

    std::vector<size_t> Order(Entry.Senses.size());

    std::iota(Order.begin(), Order.end(), size_t{0});

    std::stable_sort(Order.begin(), Order.end(), [&Weights](size_t Left, size_t Right)
    {
        if (Weights[Left].first != Weights[Right].first)
        {
            return Weights[Left].first > Weights[Right].first;
        }

        return Weights[Left].second > Weights[Right].second;
    });

    std::vector<FTDictionarySense> Sorted;
    Sorted.reserve(Order.size());

    for (const size_t Index : Order)
    {
        Sorted.push_back(std::move(Entry.Senses[Index]));
    }

    Entry.Senses = std::move(Sorted);

    return Entry;
}

std::optional<FTDictionaryEntry> FTDictionaryRepository::LookUpById(int64_t WordId) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LookUpById");

    FTStatement Query = Target.Prepare(
        "SELECT words.id, words.headword, parts_of_speech.code, words.phonetic, "
        "       words.frequency_rank, words.cefr_level, words.llm_complete, "
        "       words.teaching_rank, words.tags "
        "FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE words.id = ?1");

    if (!Query.IsValid())
    {
        return std::nullopt;
    }

    Query.BindInt64(1, WordId);

    std::vector<FTDictionaryEntry> Result = RunWordQuery(Target, Query, GetNativeLanguageFilter(), HasMeaningExamples());

    if (Result.empty())
    {
        return std::nullopt;
    }

    return std::move(Result.front());
}

namespace
{
    std::string ReadingTranslations(const std::string& Conditions)
    {
        return
            "SELECT other.translation AS Text FROM translations other "
            "LEFT JOIN senses reading ON reading.id = other.sense_id "
            "WHERE other.word_id = words.id "
            "  AND (other.sense_id IS NULL OR reading.gender IS senses.gender) "
            + Conditions +
            "ORDER BY other.probability IS NULL, other.probability DESC, other.id ASC ";
    }

    struct FTFilterColumns
    {
        const char* Level;
        const char* PartOfSpeech;
        const char* TeachingRank;
    };

    constexpr FTFilterColumns WordColumns{"words.cefr_level", "parts_of_speech.code", "words.teaching_rank"};
    constexpr FTFilterColumns ReadingColumns{"cefr_level", "part_of_speech", "teaching_rank"};

    std::string FilterClause(const FTLearnableWordFilter& Filter,
                             bool bSkipCategoryCondition = false,
                             const FTFilterColumns& Columns = WordColumns)
    {
        std::string Clause;

        if (Filter.CefrLevel.has_value())
        {
            Clause += std::string("  AND ") + Columns.Level + " = ?4 ";
        }
        else if (Filter.bOnlyUngraded)
        {
            Clause += std::string("  AND ") + Columns.Level + " IS NULL ";
        }

        if (!Filter.PartOfSpeech.empty())
        {
            Clause += std::string("  AND ") + Columns.PartOfSpeech + " = ?5 ";
        }

        if (Filter.AboveTeachingRank > 0)
        {
            Clause += std::string("  AND ") + Columns.TeachingRank + " > ?7 ";
        }

        if (!Filter.Category.empty() && !bSkipCategoryCondition)
        {
            Clause +=
                "  AND EXISTS (SELECT 1 FROM sense_categories "
                "              JOIN categories ON categories.id = sense_categories.category_id "
                "              WHERE sense_categories.sense_id = senses.id "
                "                AND categories.code = ?6) ";
        }

        return Clause;
    }

    void BindFilter(FTStatement& Query, const FTLearnableWordFilter& Filter)
    {
        if (Filter.CefrLevel.has_value())
        {
            Query.BindInt(4, *Filter.CefrLevel);
        }

        if (!Filter.PartOfSpeech.empty())
        {
            Query.BindText(5, Filter.PartOfSpeech);
        }

        if (!Filter.Category.empty())
        {
            Query.BindText(6, Filter.Category);
        }

        if (Filter.AboveTeachingRank > 0)
        {
            Query.BindInt(7, Filter.AboveTeachingRank);
        }
    }

    const char* LearnableWordConditions()
    {
        return
            "WHERE words.frequency_rank IS NOT NULL "
            "  AND (parts_of_speech.code IN ('noun', 'verb', 'adj', 'adv', 'pron', 'prep', 'conj') "
            "       OR (parts_of_speech.code = 'intj' AND words.headword NOT LIKE '% %')) ";
    }
}

std::vector<FTLearnableWord> FTDictionaryRepository::LoadLearnableWords(
    int Limit, int Offset, const FTLearnableWordFilter& Filter) const
{
    std::vector<FTLearnableWord> Words;

    if (Limit <= 0)
    {
        return Words;
    }

    const bool bPlainCategoryFilter = !Filter.Category.empty() && !Filter.CefrLevel.has_value()
        && !Filter.bOnlyUngraded && Filter.PartOfSpeech.empty() && Filter.AboveTeachingRank <= 0;

    if (bPlainCategoryFilter)
    {
        auto Cached = CategoryOfferCache.find(Filter.Category);

        if (Cached == CategoryOfferCache.end())
        {
            Cached = CategoryOfferCache.emplace(
                Filter.Category, LoadAllLearnableWords(Filter)).first;
        }

        const std::vector<FTLearnableWord>& Offers = Cached->second;
        const int ClampedOffset = std::max(Offset, 0);

        if (ClampedOffset >= static_cast<int>(Offers.size()))
        {
            return Words;
        }

        const auto Begin = Offers.begin() + ClampedOffset;
        const auto End = Offers.begin() + std::min(Offers.size(),
            static_cast<size_t>(ClampedOffset) + static_cast<size_t>(Limit));

        Words.assign(Begin, End);

        return Words;
    }

    return LoadAllLearnableWords(Filter, Limit, Offset);
}

std::vector<FTLearnableWord> FTDictionaryRepository::LoadAllLearnableWords(
    const FTLearnableWordFilter& Filter, int Limit, int Offset) const
{
    const std::optional<int64_t> LanguageId = GetNativeLanguageFilter();

    if (Filter.Category.empty() && LanguageId.has_value() && HasLearnableReadings())
    {
        return LoadLearnableReadings(Filter, *LanguageId, Limit, Offset);
    }

    const FTQueryTimer Timer("FTDictionaryRepository::LoadAllLearnableWords");

    std::vector<FTLearnableWord> Words;

    const std::string LanguageCondition =
        LanguageId.has_value() ? "AND other.language_id = ?3 " : "";

    const std::string CategoryReadingCondition = Filter.Category.empty() ? "" :
        "AND EXISTS (SELECT 1 FROM sense_categories "
        "            JOIN categories ON categories.id = sense_categories.category_id "
        "            WHERE sense_categories.sense_id = reading.id "
        "              AND categories.code = ?6) ";

    const std::string FromClause = Filter.Category.empty() ?
        "FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "LEFT JOIN senses ON senses.word_id = words.id "
        :
        "FROM sense_categories "
        "JOIN categories ON categories.id = sense_categories.category_id "
        "                AND categories.code = ?6 "
        "JOIN senses ON senses.id = sense_categories.sense_id "
        "JOIN words ON words.id = senses.word_id "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id ";

    FTStatement Query = Target.Prepare(
        "SELECT words.headword, parts_of_speech.code, senses.gender, "
        "       MIN(words.frequency_rank) AS Rank, "
        "       MIN(words.teaching_rank) AS TeachingRank, "
        "       (" + ReadingTranslations(CategoryReadingCondition + LanguageCondition)
        + "LIMIT 1) AS Best "
        + FromClause
        + std::string(LearnableWordConditions())
        + FilterClause(Filter, !Filter.Category.empty()) +
        "GROUP BY words.headword, parts_of_speech.code, senses.gender "
        "HAVING Best IS NOT NULL "
        "ORDER BY TeachingRank ASC, words.headword ASC, parts_of_speech.code ASC, "
        "         senses.gender ASC "
        "LIMIT ?1 OFFSET ?2");

    if (!Query.IsValid())
    {
        return Words;
    }

    Query.BindInt(1, Limit);
    Query.BindInt(2, std::max(Offset, 0));

    if (LanguageId.has_value())
    {
        Query.BindInt64(3, *LanguageId);
    }

    BindFilter(Query, Filter);

    while (Query.Step())
    {
        FTLearnableWord Word;
        Word.Headword = Query.GetText(0);
        Word.PartOfSpeech = Query.GetText(1);
        Word.Gender = Query.IsNull(2) ? std::nullopt : std::optional(Query.GetInt(2));
        Word.FrequencyRank = Query.GetInt(3);
        Word.Translation = Query.GetText(5);

        Words.push_back(std::move(Word));
    }

    return Words;
}

std::vector<FTPartOfSpeechCount> FTDictionaryRepository::CountLearnableWords(
    const FTLearnableWordFilter& Filter) const
{
    const std::optional<int64_t> LanguageId = GetNativeLanguageFilter();

    if (Filter.Category.empty() && LanguageId.has_value() && HasLearnableReadings())
    {
        return CountLearnableReadings(Filter, *LanguageId);
    }

    const FTQueryTimer Timer("FTDictionaryRepository::CountLearnableWords");

    std::vector<FTPartOfSpeechCount> Counts;

    const std::string LanguageCondition =
        LanguageId.has_value() ? "AND other.language_id = ?3 " : "";

    FTStatement Query = Target.Prepare(
        "SELECT PartOfSpeech, COUNT(*) FROM ("
        "  SELECT parts_of_speech.code AS PartOfSpeech, "
        "         (" + ReadingTranslations(LanguageCondition) + "LIMIT 1) AS Best "
        "  FROM words "
        "  JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "  LEFT JOIN senses ON senses.word_id = words.id "
        + std::string(LearnableWordConditions())
        + FilterClause(Filter) +
        "  GROUP BY words.headword, parts_of_speech.code, senses.gender "
        "  HAVING Best IS NOT NULL"
        ") GROUP BY PartOfSpeech");

    if (!Query.IsValid())
    {
        return Counts;
    }

    if (LanguageId.has_value())
    {
        Query.BindInt64(3, *LanguageId);
    }

    BindFilter(Query, Filter);

    while (Query.Step())
    {
        FTPartOfSpeechCount Entry;
        Entry.PartOfSpeech = Query.GetText(0);
        Entry.Count = Query.GetInt(1);

        Counts.push_back(std::move(Entry));
    }

    return Counts;
}

bool FTDictionaryRepository::HasLearnableReadings() const
{
    if (!ResolvedLearnableReadings.has_value())
    {
        FTStatement Table = Target.Prepare(
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'learnable_readings'");

        bool bFilled = false;

        if (Table.IsValid() && Table.Step() && Table.GetInt(0) > 0)
        {
            FTStatement Row = Target.Prepare("SELECT 1 FROM learnable_readings LIMIT 1");

            bFilled = Row.IsValid() && Row.Step();
        }

        ResolvedLearnableReadings = bFilled;

        qCInfo(LogDatabase) << (bFilled ? "Learnable words come from the dictionary's learnable readings table"
                                        : "The dictionary has no learnable readings table, learnable words are queried");
    }

    return *ResolvedLearnableReadings;
}

std::vector<FTLearnableWord> FTDictionaryRepository::LoadLearnableReadings(
    const FTLearnableWordFilter& Filter, int64_t LanguageId, int Limit, int Offset) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LoadLearnableReadings");

    std::vector<FTLearnableWord> Words;

    FTStatement Query = Target.Prepare(
        "SELECT headword, part_of_speech, gender, frequency_rank, translation FROM learnable_readings "
        "WHERE language_id = ?3 "
        + FilterClause(Filter, true, ReadingColumns) +
        "ORDER BY teaching_rank ASC, headword ASC, part_of_speech ASC, gender ASC "
        "LIMIT ?1 OFFSET ?2");

    if (!Query.IsValid())
    {
        return Words;
    }

    Query.BindInt(1, Limit);
    Query.BindInt(2, std::max(Offset, 0));
    Query.BindInt64(3, LanguageId);

    BindFilter(Query, Filter);

    while (Query.Step())
    {
        FTLearnableWord Word;
        Word.Headword = Query.GetText(0);
        Word.PartOfSpeech = Query.GetText(1);
        Word.Gender = Query.IsNull(2) ? std::nullopt : std::optional(Query.GetInt(2));
        Word.FrequencyRank = Query.GetInt(3);
        Word.Translation = Query.GetText(4);

        Words.push_back(std::move(Word));
    }

    return Words;
}

std::vector<FTPartOfSpeechCount> FTDictionaryRepository::CountLearnableReadings(
    const FTLearnableWordFilter& Filter, int64_t LanguageId) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::CountLearnableReadings");

    std::vector<FTPartOfSpeechCount> Counts;

    FTStatement Query = Target.Prepare(
        "SELECT part_of_speech, COUNT(*) FROM learnable_readings "
        "WHERE language_id = ?3 "
        + FilterClause(Filter, true, ReadingColumns) +
        "GROUP BY part_of_speech");

    if (!Query.IsValid())
    {
        return Counts;
    }

    Query.BindInt64(3, LanguageId);

    BindFilter(Query, Filter);

    while (Query.Step())
    {
        FTPartOfSpeechCount Entry;
        Entry.PartOfSpeech = Query.GetText(0);
        Entry.Count = Query.GetInt(1);

        Counts.push_back(std::move(Entry));
    }

    return Counts;
}

std::vector<FTCategory> FTDictionaryRepository::ListCategories() const
{
    const FTQueryTimer Timer("FTDictionaryRepository::ListCategories");

    std::vector<FTCategory> Categories;

    FTStatement Query = Target.Prepare(
        "SELECT code, word_count FROM categories "
        "WHERE word_count >= ?1 "
        "ORDER BY word_count DESC, code ASC");

    if (!Query.IsValid())
    {
        return Categories;
    }

    Query.BindInt(1, MinimumCategoryWords);

    while (Query.Step())
    {
        FTCategory Category;
        Category.Code = Query.GetText(0);
        Category.WordCount = Query.GetInt(1);

        Categories.push_back(std::move(Category));
    }

    return Categories;
}

bool FTDictionaryRepository::HasCategory(const std::string& Code) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::HasCategory");

    if (Code.empty())
    {
        return false;
    }

    FTStatement Query = Target.Prepare("SELECT 1 FROM categories WHERE code = ?1 LIMIT 1");

    if (!Query.IsValid())
    {
        return false;
    }

    Query.BindText(1, Code);

    return Query.Step();
}

bool FTDictionaryRepository::HasReading(const std::string& Headword,
                                       const std::string& PartOfSpeech) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::HasReading");

    if (Headword.empty() || PartOfSpeech.empty())
    {
        return false;
    }

    FTStatement Query = Target.Prepare(
        "SELECT 1 FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE words.headword = ?1 AND parts_of_speech.code = ?2 "
        "LIMIT 1");

    if (!Query.IsValid())
    {
        return false;
    }

    Query.BindText(1, Headword);
    Query.BindText(2, PartOfSpeech);

    return Query.Step();
}

std::unordered_set<std::string> FTDictionaryRepository::FindHeadwordsWithReading(
    const std::vector<std::string>& Headwords, const std::string& PartOfSpeech) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::FindHeadwordsWithReading");

    std::unordered_set<std::string> Result;

    if (Headwords.empty() || PartOfSpeech.empty())
    {
        return Result;
    }

    constexpr size_t MaxHeadwordsPerStatement = 500;

    for (size_t Start = 0; Start < Headwords.size(); Start += MaxHeadwordsPerStatement)
    {
        const size_t End = std::min(Start + MaxHeadwordsPerStatement, Headwords.size());

        std::string Sql =
            "SELECT DISTINCT words.headword FROM words "
            "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
            "WHERE parts_of_speech.code = ?1 AND words.headword IN (?2";

        for (size_t Index = Start + 1; Index < End; ++Index)
        {
            Sql += ", ?" + std::to_string(Index - Start + 2);
        }

        Sql += ")";

        FTStatement Query = Target.Prepare(Sql);

        if (!Query.IsValid())
        {
            continue;
        }

        Query.BindText(1, PartOfSpeech);

        for (size_t Index = Start; Index < End; ++Index)
        {
            Query.BindText(static_cast<int>(Index - Start + 2), Headwords[Index]);
        }

        while (Query.Step())
        {
            Result.insert(Query.GetText(0));
        }
    }

    return Result;
}

bool FTDictionaryRepository::HasFormsFlags() const
{
    if (!ResolvedFormsFlags.has_value())
    {
        FTStatement Column = Target.Prepare(
            "SELECT COUNT(*) FROM pragma_table_info('words') WHERE name = 'has_forms'");

        bool bFlagged = false;

        if (Column.IsValid() && Column.Step() && Column.GetInt(0) > 0)
        {
            FTStatement Marker = Target.Prepare("SELECT 1 FROM metadata WHERE key = 'forms_flags'");

            bFlagged = Marker.IsValid() && Marker.Step();
        }

        ResolvedFormsFlags = bFlagged;

        qCInfo(LogDatabase) << (bFlagged ? "Forms eligibility comes from the dictionary's has_forms flags"
                                         : "The dictionary has no forms flags, eligibility is looked up per word");
    }

    return *ResolvedFormsFlags;
}

std::vector<FTFormsFlag> FTDictionaryRepository::LoadFormsFlags(const std::vector<std::string>& Headwords,
                                                                const std::string& PartOfSpeech) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LoadFormsFlags");

    std::vector<FTFormsFlag> Result;

    if (Headwords.empty() || PartOfSpeech.empty() || !HasFormsFlags())
    {
        return Result;
    }

    constexpr size_t MaxHeadwordsPerStatement = 500;

    for (size_t Start = 0; Start < Headwords.size(); Start += MaxHeadwordsPerStatement)
    {
        const size_t End = std::min(Start + MaxHeadwordsPerStatement, Headwords.size());

        std::string Sql =
            "SELECT headword, has_forms FROM words "
            "WHERE part_of_speech_id = (SELECT id FROM parts_of_speech WHERE code = ?1) "
            "AND has_forms IS NOT NULL AND headword IN (?2";

        for (size_t Index = Start + 1; Index < End; ++Index)
        {
            Sql += ", ?" + std::to_string(Index - Start + 2);
        }

        Sql += ")";

        FTStatement Query = Target.Prepare(Sql);

        if (!Query.IsValid())
        {
            continue;
        }

        Query.BindText(1, PartOfSpeech);

        for (size_t Index = Start; Index < End; ++Index)
        {
            Query.BindText(static_cast<int>(Index - Start + 2), Headwords[Index]);
        }

        while (Query.Step())
        {
            Result.push_back({Query.GetText(0), Query.GetInt(1) != 0});
        }
    }

    return Result;
}

std::unordered_set<std::string> FTDictionaryRepository::FindHeadwordsWithTag(
    const std::vector<std::string>& Headwords, const std::string& Tag) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::FindHeadwordsWithTag");

    std::unordered_set<std::string> Result;

    if (Headwords.empty() || Tag.empty())
    {
        return Result;
    }

    constexpr size_t MaxHeadwordsPerStatement = 500;

    for (size_t Start = 0; Start < Headwords.size(); Start += MaxHeadwordsPerStatement)
    {
        const size_t End = std::min(Start + MaxHeadwordsPerStatement, Headwords.size());

        std::string Sql = "SELECT headword, tags FROM words WHERE headword IN (?1";

        for (size_t Index = Start + 1; Index < End; ++Index)
        {
            Sql += ", ?" + std::to_string(Index - Start + 1);
        }

        Sql += ")";

        FTStatement Query = Target.Prepare(Sql);

        if (!Query.IsValid())
        {
            continue;
        }

        for (size_t Index = Start; Index < End; ++Index)
        {
            Query.BindText(static_cast<int>(Index - Start + 1), Headwords[Index]);
        }

        while (Query.Step())
        {
            if (Query.IsNull(1))
            {
                continue;
            }

            const std::vector<std::string> Tags = SplitTags(Query.GetText(1));

            if (std::find(Tags.begin(), Tags.end(), Tag) != Tags.end())
            {
                Result.insert(Query.GetText(0));
            }
        }
    }

    return Result;
}

std::vector<FTDictionaryTranslation> FTDictionaryRepository::LoadTranslationsForReading(
    const std::string& Headword,
    const std::string& PartOfSpeech,
    const std::optional<int>& Gender,
    int Limit) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LoadTranslationsForReading");

    std::vector<FTDictionaryTranslation> Translations;

    if (Headword.empty() || Limit <= 0)
    {
        return Translations;
    }

    const std::optional<int64_t> LanguageId = GetNativeLanguageFilter();

    const std::string LanguageCondition =
        LanguageId.has_value() ? "AND translations.language_id = ?5 " : "";

    FTStatement Query = Target.Prepare(
        "SELECT translations.translation, MAX(translations.probability) "
        "FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "LEFT JOIN senses ON senses.word_id = words.id "
        "JOIN translations ON translations.word_id = words.id "
        "        AND (translations.sense_id IS NULL OR translations.sense_id = senses.id) "
        "WHERE words.headword = ?1 AND parts_of_speech.code = ?2 "
        "  AND senses.gender IS ?3 "
        + LanguageCondition +
        "GROUP BY translations.translation "
        "ORDER BY MAX(translations.probability) IS NULL, MAX(translations.probability) DESC, "
        "         MIN(translations.id) ASC "
        "LIMIT ?4");

    if (!Query.IsValid())
    {
        return Translations;
    }

    Query.BindText(1, Headword);
    Query.BindText(2, PartOfSpeech);

    if (Gender.has_value())
    {
        Query.BindInt(3, *Gender);
    }
    else
    {
        Query.BindNull(3);
    }

    Query.BindInt(4, Limit);

    if (LanguageId.has_value())
    {
        Query.BindInt64(5, *LanguageId);
    }

    while (Query.Step())
    {
        FTDictionaryTranslation Translation;
        Translation.Text = Query.GetText(0);

        if (!Query.IsNull(1))
        {
            Translation.Probability = Query.GetDouble(1);
        }

        Translations.push_back(std::move(Translation));
    }

    return Translations;
}

std::vector<FTDictionaryEntry> FTDictionaryRepository::LookUpIgnoringAccents(
    const std::string& Headword) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LookUpIgnoringAccents");

    const std::string Folded = FoldAccents(Headword);

    if (Folded.empty())
    {
        return {};
    }

    const std::string Queries[] = {
        "SELECT words.id, words.headword, parts_of_speech.code, words.phonetic, "
        "       words.frequency_rank, words.cefr_level, words.llm_complete, "
        "       words.teaching_rank, words.tags "
        "FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE " + FoldedExpression("headword") + " = ?1",

        "SELECT words.id, words.headword, parts_of_speech.code, words.phonetic, "
        "       words.frequency_rank, words.cefr_level, words.llm_complete, "
        "       words.teaching_rank, words.tags "
        "FROM word_forms "
        "JOIN words ON words.id = word_forms.word_id "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE " + FoldedExpression("form") + " = ?1"
    };

    for (size_t Index = 0; Index < std::size(Queries); ++Index)
    {
        FTStatement Query = Target.Prepare(Queries[Index]);

        if (!Query.IsValid())
        {
            continue;
        }

        Query.BindText(1, Folded);

        std::vector<FTDictionaryEntry> Candidates = RunWordQuery(Target, Query, GetNativeLanguageFilter(), HasMeaningExamples());
        std::vector<FTDictionaryEntry> Confirmed;

        std::unordered_set<std::string> Seen;

        for (FTDictionaryEntry& Candidate : Candidates)
        {
            if (FoldAccents(Candidate.Headword) != Folded)
            {
                continue;
            }

            if (!Seen.insert(Candidate.Headword + "/" + Candidate.PartOfSpeech).second)
            {
                continue;
            }

            Confirmed.push_back(std::move(Candidate));
        }

        if (!Confirmed.empty())
        {
            const std::unordered_set<std::string> ExactHeadwords =
                Index == 0 ? Seen : std::unordered_set<std::string>();

            SortByUsefulness(Confirmed, ExactHeadwords);

            return Confirmed;
        }
    }

    return {};
}

std::vector<FTReverseLookupMatch> FTDictionaryRepository::LookUpByTranslation(
    const std::string& NativeWord, int Limit) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LookUpByTranslation");

    std::vector<FTReverseLookupMatch> Matches;

    if (NativeWord.empty() || Limit <= 0)
    {
        return Matches;
    }

    const std::optional<int64_t> LanguageId = GetNativeLanguageFilter();

    const std::string LanguageCondition =
        LanguageId.has_value() ? "AND translations.language_id = ?2 " : "";

    FTStatement Query = Target.Prepare(
        "SELECT words.headword, parts_of_speech.code, MIN(senses.gender), "
        "       MIN(words.teaching_rank), MIN(words.frequency_rank), MIN(words.cefr_level) "
        "FROM translations "
        "JOIN words ON words.id = translations.word_id "
        "LEFT JOIN senses ON senses.word_id = words.id "
        "        AND (translations.sense_id IS NULL OR senses.id = translations.sense_id) "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE translations.translation = ?1 "
        + LanguageCondition +
        "  AND parts_of_speech.code IN ('noun', 'verb', 'adj', 'adv') "
        "GROUP BY words.headword, parts_of_speech.code, senses.gender "
        "ORDER BY MIN(words.teaching_rank) ASC, words.headword ASC, "
        "         parts_of_speech.code ASC, senses.gender ASC "
        "LIMIT ?3");

    if (!Query.IsValid())
    {
        return Matches;
    }

    Query.BindText(1, NativeWord);

    if (LanguageId.has_value())
    {
        Query.BindInt64(2, *LanguageId);
    }

    Query.BindInt(3, Limit);

    while (Query.Step())
    {
        FTReverseLookupMatch Match;
        Match.Headword = Query.GetText(0);
        Match.PartOfSpeech = Query.GetText(1);
        Match.Gender = Query.IsNull(2) ? std::nullopt : std::optional(Query.GetInt(2));
        Match.TeachingRank = Query.GetInt(3);
        Match.FrequencyRank = Query.IsNull(4) ? std::nullopt : std::optional(Query.GetInt(4));
        Match.CefrLevel = Query.IsNull(5) ? std::nullopt : std::optional(Query.GetInt(5));

        Matches.push_back(std::move(Match));
    }

    return Matches;
}

namespace
{
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

    std::string CapitalizeFirstLetter(const std::string& Value)
    {
        QString Result = QString::fromStdString(Value);

        if (!Result.isEmpty())
        {
            Result[0] = Result[0].toUpper();
        }

        return Result.toStdString();
    }

    std::string Uppercase(const std::string& Value)
    {
        return QString::fromStdString(Value).toUpper().toStdString();
    }
}

std::vector<FTReverseLookupMatch> FTDictionaryRepository::LookUpPrefix(
    const std::string& Prefix, int Limit) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LookUpPrefix");

    std::vector<FTReverseLookupMatch> Matches;

    if (Prefix.empty() || Limit <= 0)
    {
        return Matches;
    }

    FTStatement Query = Target.Prepare(
        "SELECT words.headword, parts_of_speech.code, MIN(senses.gender), "
        "       MIN(words.teaching_rank), MIN(words.frequency_rank), MIN(words.cefr_level) "
        "FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "LEFT JOIN senses ON senses.word_id = words.id "
        "WHERE words.headword LIKE ?1 ESCAPE '\\' "
        "  AND parts_of_speech.code IN ('noun', 'verb', 'adj', 'adv') "
        "GROUP BY words.headword, parts_of_speech.code, senses.gender "
        "ORDER BY MIN(words.teaching_rank) ASC, words.headword ASC, "
        "         parts_of_speech.code ASC, senses.gender ASC "
        "LIMIT ?2");

    if (!Query.IsValid())
    {
        return Matches;
    }

    Query.BindText(1, EscapeLikePattern(Prefix) + "%");
    Query.BindInt(2, Limit);

    while (Query.Step())
    {
        FTReverseLookupMatch Match;
        Match.Headword = Query.GetText(0);
        Match.PartOfSpeech = Query.GetText(1);
        Match.Gender = Query.IsNull(2) ? std::nullopt : std::optional(Query.GetInt(2));
        Match.TeachingRank = Query.GetInt(3);
        Match.FrequencyRank = Query.IsNull(4) ? std::nullopt : std::optional(Query.GetInt(4));
        Match.CefrLevel = Query.IsNull(5) ? std::nullopt : std::optional(Query.GetInt(5));

        Matches.push_back(std::move(Match));
    }

    return Matches;
}

namespace
{
    constexpr int LastExactTier = 1;
    constexpr int FormTier = 3;
    constexpr int NativeTranslationTier = -1;
    constexpr int ForeignTranslationTier = 6;

    std::string SearchMatchKey(const FTDictionarySearchMatch& Match)
    {
        return Match.Headword + '|' + Match.PartOfSpeech;
    }

    std::string ToggleInfinitiveMarker(const std::string& Text)
    {
        const std::string Marker = "to ";

        if (Text.starts_with(Marker))
        {
            return Text.substr(Marker.size());
        }

        return Marker + Text;
    }

    std::string EntryTranslations(const std::string& Conditions)
    {
        return
            "SELECT other.translation AS Text FROM translations other "
            "WHERE other.word_id = words.id "
            + Conditions +
            "GROUP BY other.translation "
            "ORDER BY MAX(other.probability) IS NULL, MAX(other.probability) DESC, MIN(other.id) ASC ";
    }
}

std::optional<FTDictionarySearchMatch> FTDictionaryRepository::LookUpExact(
    const std::string& Headword, const std::string& PartOfSpeech) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LookUpExact");

    const std::optional<int64_t> LanguageId = GetNativeLanguageFilter();
    const std::string TranslationSubqueryCondition =
        LanguageId.has_value() ? "AND other.language_id = ?3 " : "";

    FTStatement Query = Target.Prepare(
        "SELECT parts_of_speech.code, MIN(words.cefr_level), "
        "       (SELECT GROUP_CONCAT(Best.Text, ', ') FROM ("
        + EntryTranslations(TranslationSubqueryCondition) +
        "           LIMIT 3"
        "       ) AS Best), MIN(words.frequency_rank) "
        "FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE words.headword = ?1 AND (?2 = '' OR parts_of_speech.code = ?2) "
        "GROUP BY parts_of_speech.code "
        "ORDER BY MIN(words.cefr_level) IS NULL, MIN(words.cefr_level) ASC, "
        "         parts_of_speech.code ASC "
        "LIMIT 1");

    if (!Query.IsValid())
    {
        return std::nullopt;
    }

    Query.BindText(1, Headword);
    Query.BindText(2, PartOfSpeech);

    if (LanguageId.has_value())
    {
        Query.BindInt64(3, *LanguageId);
    }

    if (!Query.Step())
    {
        return std::nullopt;
    }

    FTDictionarySearchMatch Match;
    Match.Headword = Headword;
    Match.PartOfSpeech = Query.GetText(0);
    Match.CefrLevel = Query.IsNull(1) ? std::nullopt : std::optional(Query.GetInt(1));
    Match.Translation = Query.IsNull(2) ? std::string() : Query.GetText(2);
    Match.FrequencyRank = Query.IsNull(3) ? std::nullopt : std::optional(Query.GetInt(3));

    return Match;
}

std::vector<FTDictionarySearchMatch> FTDictionaryRepository::Search(const std::string& SearchText, int Limit,
                                                                    bool bNativeQuery) const
{
    const int TranslationTier = bNativeQuery ? NativeTranslationTier : ForeignTranslationTier;

    const FTQueryTimer Timer("FTDictionaryRepository::Search");

    std::vector<std::pair<int, FTDictionarySearchMatch>> Tiered;

    if (SearchText.empty() || Limit <= 0)
    {
        return {};
    }

    const std::optional<int64_t> LanguageId = GetNativeLanguageFilter();
    const std::string TranslationSubqueryCondition =
        LanguageId.has_value() ? "AND other.language_id = ?2 " : "";
    const std::string HeadwordTranslationSubqueryCondition =
        LanguageId.has_value() ? "AND other.language_id = ?5 " : "";

    const std::string ExactPattern = MakeFoldedGlob(FoldAccents(SearchText));
    const std::string PrefixPattern = ExactPattern + "*";
    const std::string InsidePhrasePattern = "*[ '’-]" + PrefixPattern;

    FTStatement HeadwordQuery = Target.Prepare(
        "WITH MatchedWords AS ("
        "    SELECT id, "
        "           CASE WHEN headword = ?1 THEN 0 "
        "                WHEN headword GLOB ?2 THEN 1 "
        "                ELSE 4 END AS Rank "
        "    FROM words WHERE headword GLOB ?3 "
        "    UNION ALL "
        "    SELECT word_id AS id, "
        "           CASE WHEN form = ?1 OR form GLOB ?2 THEN " + std::to_string(FormTier) + " "
        "                ELSE 4 END AS Rank "
        "    FROM word_forms WHERE form GLOB ?3 "
        "    UNION ALL "
        "    SELECT id, 5 AS Rank "
        "    FROM words WHERE headword GLOB ?6"
        ") "
        "SELECT words.headword, parts_of_speech.code, "
        "       MIN(words.cefr_level), "
        "       (SELECT GROUP_CONCAT(Best.Text, ', ') FROM ("
        + EntryTranslations(HeadwordTranslationSubqueryCondition) +
        "           LIMIT 3"
        "       ) AS Best), "
        "       MIN(MatchedWords.Rank) AS MatchTier, "
        "       MIN(words.teaching_rank) AS TeachingRank, MIN(words.frequency_rank) "
        "FROM MatchedWords "
        "JOIN words ON words.id = MatchedWords.id "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "GROUP BY words.headword, parts_of_speech.code "
        "ORDER BY MatchTier ASC, "
        "         CASE WHEN MatchTier <= " + std::to_string(LastExactTier) + " THEN TeachingRank IS NULL ELSE 0 END ASC, "
        "         CASE WHEN MatchTier <= " + std::to_string(LastExactTier) + " THEN TeachingRank END ASC, "
        "         CASE WHEN MatchTier = " + std::to_string(FormTier) + " THEN parts_of_speech.code <> 'verb' ELSE 0 END ASC, "
        "         MIN(words.cefr_level) IS NULL, MIN(words.cefr_level) ASC, "
        "         words.headword ASC, parts_of_speech.code ASC "
        "LIMIT ?4");

    if (HeadwordQuery.IsValid())
    {
        const FTQueryTimer HeadwordTimer("FTDictionaryRepository::Search HeadwordQuery");

        HeadwordQuery.BindText(1, SearchText);
        HeadwordQuery.BindText(2, ExactPattern);
        HeadwordQuery.BindText(3, PrefixPattern);
        HeadwordQuery.BindInt(4, Limit);
        HeadwordQuery.BindText(6, InsidePhrasePattern);

        if (LanguageId.has_value())
        {
            HeadwordQuery.BindInt64(5, *LanguageId);
        }

        while (HeadwordQuery.Step())
        {
            FTDictionarySearchMatch Match;
            Match.Headword = HeadwordQuery.GetText(0);
            Match.PartOfSpeech = HeadwordQuery.GetText(1);
            Match.CefrLevel = HeadwordQuery.IsNull(2) ? std::nullopt
                                                       : std::optional(HeadwordQuery.GetInt(2));
            Match.Translation = HeadwordQuery.IsNull(3) ? std::string() : HeadwordQuery.GetText(3);

            Match.TeachingRank = HeadwordQuery.IsNull(5) ? std::nullopt
                                                          : std::optional(HeadwordQuery.GetInt(5));
            Match.FrequencyRank = HeadwordQuery.IsNull(6) ? std::nullopt : std::optional(HeadwordQuery.GetInt(6));

            const int Tier = HeadwordQuery.GetInt(4);

            Match.bExactMatch = Tier <= LastExactTier || Tier == FormTier;

            Tiered.emplace_back(Tier, std::move(Match));
        }
    }

    std::unordered_map<std::string, size_t> FoundIndices;
    for (size_t Index = 0; Index < Tiered.size(); ++Index)
    {
        FoundIndices.emplace(SearchMatchKey(Tiered[Index].second), Index);
    }

    FTStatement TranslationQuery = Target.Prepare(
        "SELECT words.headword, parts_of_speech.code, "
        "       MIN(words.cefr_level), "
        "       (SELECT GROUP_CONCAT(Best.Text, ', ') FROM ("
        + EntryTranslations(TranslationSubqueryCondition) +
        "           LIMIT 3"
        "       ) AS Best), MIN(words.frequency_rank), MAX(translations.probability) "
        "FROM translations "
        "JOIN words ON words.id = translations.word_id "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE translations.translation IN (?1, ?4, ?5, ?6) "
        + (LanguageId.has_value() ? std::string("AND translations.language_id = ?2 ") : std::string()) +
        "GROUP BY words.headword, parts_of_speech.code "
        "ORDER BY MAX(translations.probability) IS NULL, MAX(translations.probability) DESC, "
        "         MIN(words.cefr_level) IS NULL, MIN(words.cefr_level) ASC, words.headword ASC, "
        "         parts_of_speech.code ASC "
        "LIMIT ?3");

    if (TranslationQuery.IsValid())
    {
        const FTQueryTimer TranslationTimer("FTDictionaryRepository::Search TranslationQuery");

        TranslationQuery.BindText(1, SearchText);

        if (LanguageId.has_value())
        {
            TranslationQuery.BindInt64(2, *LanguageId);
        }

        TranslationQuery.BindInt(3, Limit);
        TranslationQuery.BindText(4, CapitalizeFirstLetter(SearchText));
        TranslationQuery.BindText(5, Uppercase(SearchText));
        TranslationQuery.BindText(6, NativeLanguage == "en" ? ToggleInfinitiveMarker(SearchText) : SearchText);

        while (TranslationQuery.Step())
        {
            FTDictionarySearchMatch Match;
            Match.Headword = TranslationQuery.GetText(0);
            Match.PartOfSpeech = TranslationQuery.GetText(1);
            Match.CefrLevel = TranslationQuery.IsNull(2) ? std::nullopt
                                                          : std::optional(TranslationQuery.GetInt(2));
            Match.Translation =
                TranslationQuery.IsNull(3) ? std::string() : TranslationQuery.GetText(3);
            Match.FrequencyRank = TranslationQuery.IsNull(4) ? std::nullopt : std::optional(TranslationQuery.GetInt(4));
            Match.TranslationProbability = TranslationQuery.IsNull(5) ? std::nullopt
                                                                        : std::optional(TranslationQuery.GetDouble(5));

            const auto Existing = FoundIndices.find(SearchMatchKey(Match));

            if (Existing == FoundIndices.end())
            {
                FoundIndices.emplace(SearchMatchKey(Match), Tiered.size());
                Tiered.emplace_back(TranslationTier, std::move(Match));

                continue;
            }

            if (bNativeQuery)
            {
                Tiered[Existing->second].first = TranslationTier;
                Tiered[Existing->second].second.TranslationProbability = Match.TranslationProbability;
            }
        }
    }

    std::stable_sort(Tiered.begin(), Tiered.end(),
        [TranslationTier](const auto& Left, const auto& Right)
        {
            if (Left.first != Right.first)
            {
                return Left.first < Right.first;
            }

            if (Left.first == TranslationTier)
            {
                const std::optional<double>& LeftProbability = Left.second.TranslationProbability;
                const std::optional<double>& RightProbability = Right.second.TranslationProbability;

                if (LeftProbability.has_value() != RightProbability.has_value())
                {
                    return LeftProbability.has_value();
                }

                if (LeftProbability.has_value() && *LeftProbability != *RightProbability)
                {
                    return *LeftProbability > *RightProbability;
                }
            }

            if (Left.first <= LastExactTier)
            {
                const bool LeftHasRank = Left.second.TeachingRank.has_value();
                const bool RightHasRank = Right.second.TeachingRank.has_value();

                if (LeftHasRank != RightHasRank)
                {
                    return LeftHasRank;
                }

                if (LeftHasRank && *Left.second.TeachingRank != *Right.second.TeachingRank)
                {
                    return *Left.second.TeachingRank < *Right.second.TeachingRank;
                }
            }

            if (Left.first == FormTier)
            {
                const bool bLeftVerb = Left.second.PartOfSpeech == "verb";
                const bool bRightVerb = Right.second.PartOfSpeech == "verb";

                if (bLeftVerb != bRightVerb)
                {
                    return bLeftVerb;
                }
            }

            const bool LeftHasCefr = Left.second.CefrLevel.has_value();
            const bool RightHasCefr = Right.second.CefrLevel.has_value();

            if (LeftHasCefr != RightHasCefr)
            {
                return LeftHasCefr;
            }

            if (LeftHasCefr && *Left.second.CefrLevel != *Right.second.CefrLevel)
            {
                return *Left.second.CefrLevel < *Right.second.CefrLevel;
            }

            return Left.second.Headword < Right.second.Headword;
        });

    std::vector<FTDictionarySearchMatch> Matches;
    Matches.reserve(Tiered.size());

    for (auto& Entry : Tiered)
    {
        Matches.push_back(std::move(Entry.second));
    }

    if (Matches.size() > static_cast<size_t>(Limit))
    {
        Matches.resize(static_cast<size_t>(Limit));
    }

    return Matches;
}

namespace
{
    constexpr size_t MaxIdsPerStatement = 500;

    std::string MakePlaceholders(size_t Count)
    {
        std::string Placeholders;

        for (size_t Index = 1; Index <= Count; ++Index)
        {
            Placeholders += (Index == 1 ? "?" : ", ?") + std::to_string(Index);
        }

        return Placeholders;
    }

    const std::string TreeSentenceColumns{"id, text, translation, gap_start, gap_length, level, changed_count"};

    FTTreeSentence ReadTreeSentence(FTStatement& Query)
    {
        FTTreeSentence Sentence;
        Sentence.Id = static_cast<uint32_t>(Query.GetInt64(0));
        Sentence.Text = Query.GetText(1);
        Sentence.Translation = Query.IsNull(2) ? std::string() : Query.GetText(2);
        Sentence.GapStart = Query.IsNull(3) ? -1 : Query.GetInt(3);
        Sentence.GapLength = Query.IsNull(4) ? 0 : Query.GetInt(4);
        Sentence.Level = Query.GetInt(5);
        Sentence.ChangedCount = Query.GetInt(6);

        return Sentence;
    }

    bool IsApostrophe(QChar Character)
    {
        return Character == QLatin1Char('\'') || Character == QChar(0x2019);
    }

    QString FoldPhraseText(const std::string& Text)
    {
        QString Folded = QString::fromStdString(Text).toCaseFolded();

        for (QChar& Character : Folded)
        {
            if (IsApostrophe(Character))
            {
                Character = QLatin1Char('\'');
            }
        }

        return Folded;
    }

    bool ContainsPhrase(const QString& Text, const QString& Phrase)
    {
        for (qsizetype At = Text.indexOf(Phrase); At >= 0; At = Text.indexOf(Phrase, At + 1))
        {
            const qsizetype End = At + Phrase.size();
            const bool bStartsWord = At == 0 || !Text[At - 1].isLetterOrNumber();
            const bool bEndsWord = End == Text.size() || !Text[End].isLetterOrNumber();

            if (bStartsWord && bEndsWord)
            {
                return true;
            }
        }

        return false;
    }

    std::string MakePhrasePattern(const std::string& Phrase)
    {
        QString Pattern{QLatin1Char('%')};

        for (const QChar Character : QString::fromStdString(Phrase))
        {
            if (IsApostrophe(Character))
            {
                Pattern += QLatin1Char('_');

                continue;
            }

            if (Character == QLatin1Char('%') || Character == QLatin1Char('_') || Character == QLatin1Char('\\'))
            {
                Pattern += QLatin1Char('\\');
            }

            Pattern += Character;
        }

        Pattern += QLatin1Char('%');

        return Pattern.toStdString();
    }
}

FTWordIdSet FTDictionaryRepository::ResolveWordIds(const std::vector<std::string>& Headwords) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::ResolveWordIds");

    FTWordIdSet Result;

    for (size_t Start = 0; Start < Headwords.size(); Start += MaxIdsPerStatement)
    {
        const size_t End = std::min(Start + MaxIdsPerStatement, Headwords.size());

        FTStatement Query = Target.Prepare(
            "SELECT id FROM words WHERE headword IN (" + MakePlaceholders(End - Start) + ")");

        if (!Query.IsValid())
        {
            qCWarning(LogDatabase) << "ResolveWordIds: the headword query failed";

            return Result;
        }

        for (size_t Index = Start; Index < End; ++Index)
        {
            Query.BindText(static_cast<int>(Index - Start + 1), Headwords[Index]);
        }

        while (Query.Step())
        {
            Result.Add(static_cast<uint32_t>(Query.GetInt64(0)));
        }
    }

    return Result;
}

std::vector<uint32_t> FTDictionaryRepository::ListWordIds(const std::string& Headword) const
{
    std::vector<uint32_t> Result;

    FTStatement Query = Target.Prepare("SELECT id FROM words WHERE headword = ?1");

    if (!Query.IsValid())
    {
        return Result;
    }

    Query.BindText(1, Headword);

    while (Query.Step())
    {
        Result.push_back(static_cast<uint32_t>(Query.GetInt64(0)));
    }

    return Result;
}

std::vector<FTHeadwordForm> FTDictionaryRepository::ListHeadwordForms(const std::vector<std::string>& Headwords) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::ListHeadwordForms");

    std::vector<FTHeadwordForm> Result;

    for (size_t Start = 0; Start < Headwords.size(); Start += MaxIdsPerStatement)
    {
        const size_t End = std::min(Start + MaxIdsPerStatement, Headwords.size());

        FTStatement Query = Target.Prepare(
            "SELECT words.headword, parts_of_speech.code, word_forms.form "
            "FROM words "
            "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
            "JOIN word_forms ON word_forms.word_id = words.id "
            "WHERE words.headword IN (" + MakePlaceholders(End - Start) + ")");

        if (!Query.IsValid())
        {
            qCWarning(LogDatabase) << "ListHeadwordForms: the forms query failed";

            return Result;
        }

        for (size_t Index = Start; Index < End; ++Index)
        {
            Query.BindText(static_cast<int>(Index - Start + 1), Headwords[Index]);
        }

        while (Query.Step())
        {
            Result.push_back(FTHeadwordForm{Query.GetText(0), Query.GetText(1), Query.GetText(2)});
        }
    }

    return Result;
}

std::vector<FTRankedWord> FTDictionaryRepository::ListGrammarWords(const std::vector<std::string>& EssentialVerbs,
                                                                   int FallbackVerbCount) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::ListGrammarWords");

    std::vector<FTRankedWord> Result;

    const std::string VerbFilter = EssentialVerbs.empty()
        ? "SELECT id, 1 FROM ("
          "  SELECT words.id AS id "
          "  FROM words "
          "  JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
          "  WHERE parts_of_speech.code = 'verb' AND words.teaching_rank > 0 "
          "  ORDER BY words.teaching_rank "
          "  LIMIT ?1)"
        : "SELECT words.id, 1 "
          "FROM words "
          "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
          "WHERE parts_of_speech.code = 'verb' AND words.headword IN (" + MakePlaceholders(EssentialVerbs.size()) + ")";

    FTStatement Query = Target.Prepare(
        "SELECT words.id, 0 "
        "FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE parts_of_speech.code IN ('pron', 'prep', 'conj', 'det', 'article', 'contraction') "
        "UNION ALL " + VerbFilter);

    if (!Query.IsValid())
    {
        qCWarning(LogDatabase) << "ListGrammarWords: the grammar word query failed";

        return Result;
    }

    if (EssentialVerbs.empty())
    {
        Query.BindInt(1, std::max(FallbackVerbCount, 0));
    }

    for (size_t Index = 0; Index < EssentialVerbs.size(); ++Index)
    {
        Query.BindText(static_cast<int>(Index + 1), EssentialVerbs[Index]);
    }

    while (Query.Step())
    {
        FTRankedWord Word;
        Word.WordId = static_cast<uint32_t>(Query.GetInt64(0));
        Word.bVerb = Query.GetInt(1) != 0;
        Word.Headword = Query.GetText(2);
        Word.PartOfSpeech = Query.GetText(3);

        Result.push_back(Word);
    }

    return Result;
}

int FTDictionaryRepository::GetTeachingRankCeiling(int LearnableWordsAbove) const
{
    const auto Cached = RankCeilingCache.find(LearnableWordsAbove);

    if (Cached != RankCeilingCache.end())
    {
        return Cached->second;
    }

    const std::string Stored = ReadDictionaryMetadata(Target, "teaching_rank_ceiling_" + std::to_string(LearnableWordsAbove));

    if (!Stored.empty())
    {
        const int StoredCeiling = std::atoi(Stored.c_str());

        RankCeilingCache.emplace(LearnableWordsAbove, StoredCeiling);

        return StoredCeiling;
    }

    const FTQueryTimer Timer("FTDictionaryRepository::GetTeachingRankCeiling");

    FTStatement Query = Target.Prepare(
        "SELECT words.teaching_rank FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        + std::string(LearnableWordConditions()) +
        "  AND words.teaching_rank IS NOT NULL "
        "ORDER BY words.teaching_rank DESC LIMIT 1 OFFSET ?1");

    if (!Query.IsValid())
    {
        qCWarning(LogDatabase) << "GetTeachingRankCeiling: the rank query failed";

        return 0;
    }

    Query.BindInt(1, std::max(LearnableWordsAbove, 0));

    const int Ceiling = Query.Step() ? Query.GetInt(0) : 0;

    RankCeilingCache.emplace(LearnableWordsAbove, Ceiling);

    return Ceiling;
}

std::vector<FTLevelRank> FTDictionaryRepository::LoadLevelTeachingRanks() const
{
    if (ResolvedLevelTeachingRanks.has_value())
    {
        return *ResolvedLevelTeachingRanks;
    }

    std::vector<FTLevelRank> Result;

    const std::string Stored = ReadDictionaryMetadata(Target, "level_teaching_ranks");

    if (!Stored.empty())
    {
        const QJsonDocument Document = QJsonDocument::fromJson(QByteArray::fromStdString(Stored));

        if (Document.isArray())
        {
            for (const QJsonValue& Value : Document.array())
            {
                FTLevelRank Entry;
                Entry.Level = Value[QLatin1String("level")].toInt(-1);
                Entry.TeachingRank = Value[QLatin1String("rank")].toInt();

                if (Entry.Level >= 0)
                {
                    Result.push_back(Entry);
                }
            }

            ResolvedLevelTeachingRanks = Result;

            return Result;
        }

        qCWarning(LogDatabase) << "LoadLevelTeachingRanks: the stored level ranks are not a list, querying them";
    }

    const FTQueryTimer Timer("FTDictionaryRepository::LoadLevelTeachingRanks");

    FTStatement Query = Target.Prepare(
        "SELECT cefr_level, teaching_rank FROM words "
        "WHERE cefr_level IS NOT NULL AND teaching_rank IS NOT NULL "
        "ORDER BY cefr_level, teaching_rank");

    if (!Query.IsValid())
    {
        qCWarning(LogDatabase) << "LoadLevelTeachingRanks: the level query failed";

        return Result;
    }

    std::vector<std::vector<int>> RanksByLevel;

    while (Query.Step())
    {
        const int Level = Query.GetInt(0);

        if (Level < 0)
        {
            continue;
        }

        if (static_cast<size_t>(Level) >= RanksByLevel.size())
        {
            RanksByLevel.resize(static_cast<size_t>(Level) + 1);
        }

        RanksByLevel[static_cast<size_t>(Level)].push_back(Query.GetInt(1));
    }

    for (size_t Level = 0; Level < RanksByLevel.size(); ++Level)
    {
        const std::vector<int>& Ranks = RanksByLevel[Level];

        if (Ranks.empty())
        {
            continue;
        }

        FTLevelRank Entry;
        Entry.Level = static_cast<int>(Level);
        Entry.TeachingRank = Ranks[Ranks.size() / 2];

        Result.push_back(Entry);
    }

    ResolvedLevelTeachingRanks = Result;

    return Result;
}

std::vector<FTRankedWord> FTDictionaryRepository::ListWordsUpToTeachingRank(int TeachingRank) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::ListWordsUpToTeachingRank");

    std::vector<FTRankedWord> Result;

    if (TeachingRank <= 0)
    {
        return Result;
    }

    FTStatement Query = Target.Prepare(
        "SELECT words.id, parts_of_speech.code = 'verb', words.headword, parts_of_speech.code "
        "FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE words.teaching_rank <= ?1");

    if (!Query.IsValid())
    {
        qCWarning(LogDatabase) << "ListWordsUpToTeachingRank: the rank query failed";

        return Result;
    }

    Query.BindInt(1, TeachingRank);

    while (Query.Step())
    {
        FTRankedWord Word;
        Word.WordId = static_cast<uint32_t>(Query.GetInt64(0));
        Word.bVerb = Query.GetInt(1) != 0;
        Word.Headword = Query.GetText(2);
        Word.PartOfSpeech = Query.GetText(3);

        Result.push_back(Word);
    }

    return Result;
}

std::vector<FTTreeSentence> FTDictionaryRepository::LoadTreeSentences(
    const std::vector<uint32_t>& SentenceIds) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LoadTreeSentences");

    std::vector<FTTreeSentence> Result;
    Result.reserve(SentenceIds.size());

    for (size_t Start = 0; Start < SentenceIds.size(); Start += MaxIdsPerStatement)
    {
        const size_t End = std::min(Start + MaxIdsPerStatement, SentenceIds.size());

        FTStatement Query = Target.Prepare(
            "SELECT " + TreeSentenceColumns + " FROM sentences WHERE id IN ("
            + MakePlaceholders(End - Start) + ") ORDER BY id");

        if (!Query.IsValid())
        {
            qCWarning(LogDatabase) << "LoadTreeSentences: the dictionary has no usable sentences table";

            return Result;
        }

        for (size_t Index = Start; Index < End; ++Index)
        {
            Query.BindInt64(static_cast<int>(Index - Start + 1), SentenceIds[Index]);
        }

        while (Query.Step())
        {
            Result.push_back(ReadTreeSentence(Query));
        }
    }

    return Result;
}

bool FTDictionaryRepository::HasFormSentences() const
{
    if (!ResolvedFormSentences.has_value())
    {
        FTStatement Query = Target.Prepare(
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'form_sentences'");

        ResolvedFormSentences = Query.IsValid() && Query.Step() && Query.GetInt(0) > 0;

        qCInfo(LogDatabase) << (*ResolvedFormSentences ? "Forms sentences come from the form sentences table"
                                                       : "The dictionary has no form sentences table, "
                                                         "forms sentences are searched");
    }

    return *ResolvedFormSentences;
}

std::vector<FTFormSentence> FTDictionaryRepository::FindFormSentences(const std::string& Headword,
                                                                      const std::string& Kind,
                                                                      const std::string& Tense) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::FindFormSentences");

    std::vector<FTFormSentence> Result;

    FTStatement Query = Target.Prepare(
        "SELECT first, number, sentence_id FROM form_sentences WHERE headword = ?1 AND kind = ?2 AND tense = ?3 "
        "ORDER BY number, first, rank");

    if (!Query.IsValid())
    {
        qCWarning(LogDatabase) << "FindFormSentences: the dictionary has no usable form sentences table";

        return Result;
    }

    Query.BindText(1, Headword);
    Query.BindText(2, Kind);
    Query.BindText(3, Tense);

    while (Query.Step())
    {
        Result.push_back({Query.GetInt(0), Query.GetInt(1), static_cast<uint32_t>(Query.GetInt64(2))});
    }

    return Result;
}

std::vector<std::vector<FTTreeSentence>> FTDictionaryRepository::FindSentencesWithPhrases(
    const std::vector<std::string>& Phrases, size_t LimitPerPhrase) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::FindSentencesWithPhrases");

    std::vector<std::vector<FTTreeSentence>> Found(Phrases.size());

    if (Phrases.empty() || Phrases.size() > MaxIdsPerStatement)
    {
        return Found;
    }

    std::string Conditions;

    for (size_t Index = 0; Index < Phrases.size(); ++Index)
    {
        Conditions += (Index == 0 ? "text LIKE ?" : " OR text LIKE ?") + std::to_string(Index + 1) + " ESCAPE '\\'";
    }

    FTStatement Query = Target.Prepare("SELECT " + TreeSentenceColumns + " FROM sentences WHERE " + Conditions
                                       + " ORDER BY id");

    if (!Query.IsValid())
    {
        qCWarning(LogDatabase) << "FindSentencesWithPhrases: the dictionary has no usable sentences table";

        return Found;
    }

    std::vector<QString> FoldedPhrases;

    for (size_t Index = 0; Index < Phrases.size(); ++Index)
    {
        Query.BindText(static_cast<int>(Index + 1), MakePhrasePattern(Phrases[Index]));
        FoldedPhrases.push_back(FoldPhraseText(Phrases[Index]));
    }

    while (Query.Step())
    {
        const FTTreeSentence Sentence = ReadTreeSentence(Query);
        const QString Text = FoldPhraseText(Sentence.Text);

        for (size_t Index = 0; Index < Phrases.size(); ++Index)
        {
            if (Found[Index].size() < LimitPerPhrase && ContainsPhrase(Text, FoldedPhrases[Index]))
            {
                Found[Index].push_back(Sentence);
            }
        }
    }

    return Found;
}

std::string FTDictionaryRepository::MakeFoldedExpression(const std::string& Column)
{
    return FoldedExpression(Column);
}

bool FTDictionaryRepository::HasMeaningExamples() const
{
    if (!ResolvedMeaningExamples.has_value())
    {
        FTStatement Query = Target.Prepare(
            "SELECT COUNT(*) FROM pragma_table_info('senses') WHERE name = 'example'");

        ResolvedMeaningExamples = Query.IsValid() && Query.Step() && Query.GetInt(0) > 0;
    }

    return *ResolvedMeaningExamples;
}

bool FTDictionaryRepository::HasDecks() const
{
    if (!ResolvedDecks.has_value())
    {
        FTStatement Query = Target.Prepare(
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name IN ('decks', 'deck_words')");

        constexpr int DeckTableCount = 2;

        ResolvedDecks = Query.IsValid() && Query.Step() && Query.GetInt(0) == DeckTableCount;
    }

    return *ResolvedDecks;
}

bool FTDictionaryRepository::HasDeckCategories() const
{
    if (!ResolvedDeckCategories.has_value())
    {
        FTStatement Query = Target.Prepare(
            "SELECT COUNT(*) FROM pragma_table_info('decks') WHERE name = 'category'");

        ResolvedDeckCategories = Query.IsValid() && Query.Step() && Query.GetInt(0) > 0;
    }

    return *ResolvedDeckCategories;
}

std::vector<FTDeck> FTDictionaryRepository::ListDecks() const
{
    const FTQueryTimer Timer("FTDictionaryRepository::ListDecks");

    std::vector<FTDeck> Decks;

    if (!HasDecks())
    {
        return Decks;
    }

    const std::string CategoryColumn = HasDeckCategories() ? "decks.category" : "''";

    FTStatement Query = Target.Prepare(
        "SELECT decks.id, decks.name, " + CategoryColumn + ", COUNT(deck_words.id) FROM decks "
        "JOIN deck_words ON deck_words.deck_id = decks.id "
        "GROUP BY decks.id "
        "ORDER BY decks.id");

    if (!Query.IsValid())
    {
        return Decks;
    }

    while (Query.Step())
    {
        FTDeck Deck;
        Deck.Id = Query.GetInt64(0);
        Deck.Name = Query.GetText(1);
        Deck.Category = Query.GetText(2);
        Deck.WordCount = Query.GetInt(3);

        Decks.push_back(std::move(Deck));
    }

    return Decks;
}

std::vector<FTDeckWord> FTDictionaryRepository::LoadDeckWords(int64_t DeckId) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LoadDeckWords");

    std::vector<FTDeckWord> Words;

    if (!HasDecks())
    {
        return Words;
    }

    const std::optional<int64_t> LanguageId = GetNativeLanguageFilter();

    const std::string LanguageCondition =
        LanguageId.has_value() ? "AND other.language_id = ?2 " : "";

    FTStatement Query = Target.Prepare(
        "SELECT deck_words.headword, deck_words.part_of_speech, deck_words.gist, deck_words.context, "
        "       words.cefr_level, "
        "       words.teaching_rank, "
        "       (SELECT senses.gender FROM senses "
        "        WHERE senses.word_id = words.id "
        "          AND (deck_words.gist IS NULL OR senses.gist = deck_words.gist) "
        "        ORDER BY senses.id LIMIT 1), "
        "       COALESCE("
        "           (SELECT other.translation FROM translations other "
        "            JOIN senses meaning ON meaning.id = other.sense_id "
        "            WHERE other.word_id = words.id AND meaning.gist = deck_words.gist "
        + LanguageCondition +
        "            ORDER BY other.probability IS NULL, other.probability DESC, other.id ASC "
        "            LIMIT 1), "
        "           (SELECT other.translation FROM translations other "
        "            WHERE other.word_id = words.id "
        + LanguageCondition +
        "            ORDER BY other.probability IS NULL, other.probability DESC, other.id ASC "
        "            LIMIT 1)) "
        "FROM deck_words "
        "JOIN parts_of_speech ON parts_of_speech.code = deck_words.part_of_speech "
        "JOIN words ON words.headword = deck_words.headword "
        "          AND words.part_of_speech_id = parts_of_speech.id "
        "WHERE deck_words.deck_id = ?1 "
        "ORDER BY deck_words.id");

    if (!Query.IsValid())
    {
        return Words;
    }

    Query.BindInt64(1, DeckId);

    if (LanguageId.has_value())
    {
        Query.BindInt64(2, *LanguageId);
    }

    while (Query.Step())
    {
        FTDeckWord Word;
        Word.Headword = Query.GetText(0);
        Word.PartOfSpeech = Query.GetText(1);
        Word.Gist = Query.IsNull(2) ? std::string() : Query.GetText(2);
        Word.Context = Query.IsNull(3) ? std::string() : Query.GetText(3);
        Word.CefrLevel = Query.IsNull(4) ? std::nullopt : std::optional(Query.GetInt(4));
        Word.TeachingRank = Query.IsNull(5) ? std::nullopt : std::optional(Query.GetInt(5));
        Word.Gender = Query.IsNull(6) ? std::nullopt : std::optional(Query.GetInt(6));
        Word.Translation = Query.IsNull(7) ? std::string() : Query.GetText(7);

        Words.push_back(std::move(Word));
    }

    return Words;
}

std::map<int64_t, std::vector<FTDeckWord>> FTDictionaryRepository::LoadDeckReadings() const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LoadDeckReadings");

    std::map<int64_t, std::vector<FTDeckWord>> Readings;

    if (!HasDecks())
    {
        return Readings;
    }

    FTStatement Query = Target.Prepare(
        "SELECT deck_words.deck_id, deck_words.headword, deck_words.part_of_speech, deck_words.gist "
        "FROM deck_words "
        "JOIN parts_of_speech ON parts_of_speech.code = deck_words.part_of_speech "
        "JOIN words ON words.headword = deck_words.headword "
        "          AND words.part_of_speech_id = parts_of_speech.id "
        "ORDER BY deck_words.id");

    if (!Query.IsValid())
    {
        return Readings;
    }

    while (Query.Step())
    {
        FTDeckWord Word;
        Word.Headword = Query.GetText(1);
        Word.PartOfSpeech = Query.GetText(2);
        Word.Gist = Query.IsNull(3) ? std::string() : Query.GetText(3);

        Readings[Query.GetInt64(0)].push_back(std::move(Word));
    }

    return Readings;
}

std::vector<FTDictionarySense> FTDictionaryRepository::LoadMeanings(const std::string& Headword,
                                                                    const std::string& PartOfSpeech) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::LoadMeanings");

    std::vector<FTDictionarySense> Meanings;

    if (Headword.empty() || PartOfSpeech.empty())
    {
        return Meanings;
    }

    FTStatement Query = Target.Prepare(
        "SELECT words.id FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE words.headword = ?1 AND parts_of_speech.code = ?2");

    if (!Query.IsValid())
    {
        return Meanings;
    }

    Query.BindText(1, Headword);
    Query.BindText(2, PartOfSpeech);

    if (!Query.Step())
    {
        return Meanings;
    }

    for (FTDictionarySense& Sense :
         LoadSenses(Target, Query.GetInt64(0), GetNativeLanguageFilter(), HasMeaningExamples()))
    {
        if (!Sense.Gist.empty())
        {
            Meanings.push_back(std::move(Sense));
        }
    }

    return Meanings;
}

std::string FTDictionaryRepository::FindPhonetic(const std::string& Headword, const std::string& PartOfSpeech) const
{
    const FTQueryTimer Timer("FTDictionaryRepository::FindPhonetic");

    if (Headword.empty() || PartOfSpeech.empty())
    {
        return {};
    }

    FTStatement Query = Target.Prepare(
        "SELECT words.phonetic FROM words "
        "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id "
        "WHERE words.headword = ?1 AND parts_of_speech.code = ?2 AND words.phonetic <> ''");

    if (!Query.IsValid())
    {
        return {};
    }

    Query.BindText(1, Headword);
    Query.BindText(2, PartOfSpeech);

    if (!Query.Step())
    {
        return {};
    }

    return Query.GetText(0);
}

bool FTDictionaryRepository::HasSearchIndices(FTDatabase& Database)
{
    FTStatement Query = Database.Prepare(
        "SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' AND name IN "
        "('idx_translations_translation', 'idx_words_headword_folded', 'idx_word_forms_form_folded')");

    constexpr int SearchIndexCount = 3;

    return Query.IsValid() && Query.Step() && Query.GetInt(0) == SearchIndexCount;
}
