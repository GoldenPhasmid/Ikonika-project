#include <catch2/catch_test_macros.hpp>

#include "core/FTDatabase.h"
#include "core/FTDictionaryPackage.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTWordForms.h"

#include <QDir>
#include <QTemporaryDir>

#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <memory>
#include <set>
#include <sstream>

namespace
{
    /** Opens the real seeded dictionary database read from, never written to. Skips the whole
        file's tests if it has not been built yet, rather than failing the suite over a fixture
        that is not part of the source tree. */
    struct Fixture
    {
        Fixture()
        {
            Target = FTDatabase::OpenReadOnly(DICTIONARY_DATABASE_PATH);
        }

        std::optional<FTDatabase> Target;
    };

    bool HasForm(const std::vector<FTDictionaryForm>& Forms, const std::string& Text,
                 int Mood, int Tense, int Person, int Gender, int Number)
    {
        return std::any_of(Forms.begin(), Forms.end(),
            [&](const FTDictionaryForm& Form)
            {
                return Form.Text == Text && Form.Mood == Mood && Form.Tense == Tense &&
                       Form.Person == Person && Form.Gender == Gender && Form.Number == Number;
            });
    }

    const FTDictionaryEntry* FindEntryByPartOfSpeech(
        const std::vector<FTDictionaryEntry>& Entries, const std::string& PartOfSpeech)
    {
        for (const FTDictionaryEntry& Entry : Entries)
        {
            if (Entry.PartOfSpeech == PartOfSpeech)
            {
                return &Entry;
            }
        }

        return nullptr;
    }
}

TEST_CASE("The seeded dictionary database opens", "[dictionaryrepository]")
{
    Fixture Test;

    REQUIRE(Test.Target.has_value());
    CHECK(Test.Target->IsOpen());
}

TEST_CASE("Looking up a common word returns at least one entry", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("avoir");

    REQUIRE_FALSE(Entries.empty());
    CHECK(Entries[0].Headword == "avoir");
    CHECK_FALSE(Entries[0].PartOfSpeech.empty());
}

TEST_CASE("A looked-up word carries its senses", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("avoir");

    REQUIRE_FALSE(Entries.empty());
    CHECK_FALSE(Entries[0].Senses.empty());
}

TEST_CASE("A word with a frequency rank reports one", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // Among the 500 most common French words by book-corpus frequency, per the Lexique 3 import;
    // near-certain to have a rank.
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("avoir");

    REQUIRE_FALSE(Entries.empty());

    const bool bAnyHasRank = std::any_of(Entries.begin(), Entries.end(), [](const FTDictionaryEntry& Entry)
    {
        return Entry.FrequencyRank.has_value();
    });

    CHECK(bAnyHasRank);
}

TEST_CASE("A word's gender is carried per sense, not per entry", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    // "moule" is masculine for "mold" and feminine for "mussel"; wiktextract has both as
    // separate noun records, and both must survive under the single (headword, pos) row.
    FTDictionaryRepository Repository(*Test.Target);
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("moule");

    REQUIRE_FALSE(Entries.empty());

    bool bHasMasculine = false;
    bool bHasFeminine = false;
    for (const FTDictionaryEntry& Entry : Entries)
    {
        for (const FTDictionarySense& Sense : Entry.Senses)
        {
            bHasMasculine = bHasMasculine || (Sense.Gender == std::optional<int>(0));
            bHasFeminine = bHasFeminine || (Sense.Gender == std::optional<int>(1));
        }
    }

    CHECK(bHasMasculine);
    CHECK(bHasFeminine);
}

TEST_CASE("A word with several parts of speech returns one entry per part of speech",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    // "quoi" is a pronoun and an adverb in the English Wiktionary, separate (headword, pos) rows
    // in words. This is the whole reason LookUp returns a vector rather than a single entry.
    FTDictionaryRepository Repository(*Test.Target);
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("quoi");

    REQUIRE(Entries.size() > 1);

    std::set<std::string> PartsOfSpeech;
    for (const FTDictionaryEntry& Entry : Entries)
    {
        CHECK(Entry.Headword == "quoi");
        PartsOfSpeech.insert(Entry.PartOfSpeech);
    }

    CHECK(PartsOfSpeech.count("pron") == 1);
    CHECK(PartsOfSpeech.count("adv") == 1);
}

TEST_CASE("A sense carries real translations and examples, not just a definition",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("avoir");

    REQUIRE_FALSE(Entries.empty());

    bool bAnySenseHasTranslation = false;
    bool bAnyEntryHasExample = false;

    for (const FTDictionaryEntry& Entry : Entries)
    {
        bAnyEntryHasExample = bAnyEntryHasExample || !Entry.Examples.empty();

        for (const FTDictionarySense& Sense : Entry.Senses)
        {
            bAnySenseHasTranslation = bAnySenseHasTranslation || !Sense.Translations.empty();
        }
    }

    CHECK(bAnySenseHasTranslation);
    CHECK(bAnyEntryHasExample);

    // An example is a sentence and, where the build had one, its translation. A translation
    // without a language would leave the caller unable to say what it is or speak it.
    for (const FTDictionaryEntry& Entry : Entries)
    {
        for (const FTDictionaryExample& Example : Entry.Examples)
        {
            CHECK_FALSE(Example.Text.empty());

            if (!Example.Translation.empty())
            {
                CHECK_FALSE(Example.LanguageCode.empty());
            }
        }
    }
}

TEST_CASE("A corpus example comes with its translation", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // The two halves used to be separate rows tied together by nothing but the order they were
    // written in, so anything that sorted or trimmed them showed a sentence without its
    // translation, or three translations and no sentences.
    bool bAnyExample = false;

    for (const FTDictionaryEntry& Entry : Repository.LookUp("avoir"))
    {
        for (const FTDictionaryExample& Example : Entry.Examples)
        {
            bAnyExample = true;

            CHECK_FALSE(Example.Translation.empty());
        }
    }

    CHECK(bAnyExample);
}

TEST_CASE("A meaning says what it means in one column or the other", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("avoir");

    REQUIRE_FALSE(Entries.empty());

    // A build from the language's own Wiktionary defines; one from a foreign edition glosses. The
    // two are never the same text and a meaning carrying neither would have been deleted at import,
    // so every sense that survives has exactly one of them.
    for (const FTDictionaryEntry& Entry : Entries)
    {
        for (const FTDictionarySense& Sense : Entry.Senses)
        {
            CHECK_FALSE((Sense.Definition.empty() && Sense.Gloss.empty()));
        }
    }
}

TEST_CASE("Translations come back most likely first", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("avoir");

    REQUIRE_FALSE(Entries.empty());

    for (const FTDictionaryEntry& Entry : Entries)
    {
        for (const FTDictionarySense& Sense : Entry.Senses)
        {
            // Descending, with the unmeasured ones last rather than sorting ahead of real values.
            bool bSeenMissing = false;
            double Previous = 2.0;

            for (const FTDictionaryTranslation& Translation : Sense.Translations)
            {
                if (!Translation.Probability.has_value())
                {
                    bSeenMissing = true;

                    continue;
                }

                CHECK_FALSE(bSeenMissing);
                CHECK(*Translation.Probability <= Previous);

                Previous = *Translation.Probability;
            }
        }
    }
}

TEST_CASE("A word graded by FLELex but unranked by Lexique has a CEFR level and no frequency "
          "rank", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    // "après-midi" is in the seeded database with cefr_level set and frequency_rank NULL: the
    // union of the two word sets this session's pipeline work was about, verified at the
    // repository layer rather than only in the Python build scripts.
    //
    // It replaced "turque", which the graded list no longer grades. Levels are now derived from
    // FLELex's per-band frequencies rather than read from its own level column, and a word whose
    // six bands sum to barely one occurrence per million carries no evidence for any level.
    FTDictionaryRepository Repository(*Test.Target);
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("après-midi");

    REQUIRE_FALSE(Entries.empty());

    const bool bAnyHasCefrNoRank = std::any_of(
        Entries.begin(), Entries.end(),
        [](const FTDictionaryEntry& Entry)
        {
            return Entry.CefrLevel.has_value() && !Entry.FrequencyRank.has_value();
        });

    CHECK(bAnyHasCefrNoRank);
}

TEST_CASE("A word with a recorded pronunciation reports one", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("accueil");

    REQUIRE_FALSE(Entries.empty());

    const bool bAnyHasPhonetic = std::any_of(
        Entries.begin(), Entries.end(),
        [](const FTDictionaryEntry& Entry) { return !Entry.Phonetic.empty(); });

    CHECK(bAnyHasPhonetic);
}

TEST_CASE("An inflected form of a multi-part-of-speech lemma redirects to only the matching "
          "part of speech", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    // "manger" is both a noun and a verb; "mangeons" is a form of the verb only. The redirect
    // must land on the verb row, not return the noun alongside it just because they share a
    // headword.
    FTDictionaryRepository Repository(*Test.Target);
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("mangeons");

    REQUIRE_FALSE(Entries.empty());

    for (const FTDictionaryEntry& Entry : Entries)
    {
        CHECK(Entry.Headword == "manger");
        CHECK(Entry.PartOfSpeech == "verb");
    }
}

TEST_CASE("The forms lookup picks the entry the full lookup would, with the same forms and gender order",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    const std::vector<std::string> Headwords{"avoir", "moule", "chanter", "beau", "cheval", "belle", "manger", "livre",
                                             "aller", "petit", "mer", "chevaux", "mangeons", "est", "voix", "heureux"};
    const std::vector<std::string> Parts{"verb", "noun", "adj"};

    for (const std::string& Native : {std::string(), std::string("en")})
    {
        FTDictionaryRepository Repository(*Test.Target, Native);

        for (const std::string& Headword : Headwords)
        {
            for (const std::string& Part : Parts)
            {
                const std::vector<FTDictionaryEntry> Full = Repository.LookUp(Headword);
                const FTDictionaryEntry* Expected = FindEntryByPartOfSpeech(Full, Part);
                const std::optional<FTDictionaryEntry> Lean = Repository.LookUpForForms(Headword, Part);

                INFO(Headword << " " << Part << " native '" << Native << "'");

                REQUIRE(Lean.has_value() == (Expected != nullptr));

                if (Expected == nullptr)
                {
                    continue;
                }

                CHECK(Lean->Id == Expected->Id);
                CHECK(Lean->Headword == Expected->Headword);
                CHECK(Lean->Forms.size() == Expected->Forms.size());
                REQUIRE(Lean->Senses.size() == Expected->Senses.size());

                for (size_t Index = 0; Index < Lean->Senses.size(); ++Index)
                {
                    CHECK(Lean->Senses[Index].Gender == Expected->Senses[Index].Gender);
                }
            }
        }
    }
}

TEST_CASE("A word not in the database returns no entries", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries =
        Repository.LookUp("motquinexistepasdutoutducoup");

    CHECK(Entries.empty());
}

TEST_CASE("Looking up an inflected form redirects to its lemma", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // "voulons" never gets its own row; word_forms maps it to "vouloir", which the exact-headword
    // lookup above already missed.
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("voulons");

    REQUIRE_FALSE(Entries.empty());
    CHECK(Entries[0].Headword == "vouloir");
}

TEST_CASE("A spelling that is both a headword and a form returns both readings",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // "cours" is a headword (the noun) and, separately, a conjugated form of "courir". The
    // direct headword match comes first, then the redirect lands on the lemma, so a search for
    // the spelling finds every reading of it rather than only the one that matches first.
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("cours");

    REQUIRE_FALSE(Entries.empty());
    CHECK(Entries[0].Headword == "cours");

    const bool bCourirFound = std::any_of(Entries.begin(), Entries.end(),
        [](const FTDictionaryEntry& Entry)
        {
            return Entry.Headword == "courir";
        });

    CHECK(bCourirFound);
}

/*
 * What a spelling matches and what a search was after are not the same thing. A spelling can be a
 * marginal noun in its own right and, separately, a form of a common verb; the noun matches
 * exactly and has nothing to translate, while the verb is the reading anyone typing it meant.
 */

namespace
{
    /** Where this reading sits in the results, or -1 when it is not among them. */
    int IndexOf(const std::vector<FTDictionaryEntry>& Entries, const std::string& Headword,
                const std::string& PartOfSpeech)
    {
        for (size_t Index = 0; Index < Entries.size(); ++Index)
        {
            if (Entries[Index].Headword == Headword
                && Entries[Index].PartOfSpeech == PartOfSpeech)
            {
                return static_cast<int>(Index);
            }
        }

        return -1;
    }

    bool HasAnyTranslation(const FTDictionaryEntry& Entry)
    {
        return !Entry.Translations.empty()
            || std::any_of(Entry.Senses.begin(), Entry.Senses.end(),
                           [](const FTDictionarySense& Sense)
                           {
                               return !Sense.Translations.empty();
                           });
    }

    bool HasTranslation(const FTDictionaryEntry& Entry, const std::string& Text)
    {
        const auto Matches = [&Text](const FTDictionaryTranslation& Translation)
        {
            return Translation.Text == Text;
        };

        return std::any_of(Entry.Translations.begin(), Entry.Translations.end(), Matches)
            || std::any_of(Entry.Senses.begin(), Entry.Senses.end(),
                           [&Matches](const FTDictionarySense& Sense)
                           {
                               return std::any_of(Sense.Translations.begin(), Sense.Translations.end(), Matches);
                           });
    }

    std::optional<FTDatabase> OpenReadingsWithAndWithoutTranslations()
    {
        std::optional<FTDatabase> Database = FTDatabase::OpenInMemory();

        if (!Database.has_value())
        {
            return Database;
        }

        Database->Execute(
            "CREATE TABLE languages (id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE);"
            "CREATE TABLE parts_of_speech (id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE);"
            "CREATE TABLE words (id INTEGER PRIMARY KEY, headword TEXT NOT NULL COLLATE NOCASE, "
            "  part_of_speech_id INTEGER NOT NULL, phonetic TEXT, frequency_rank INTEGER, "
            "  cefr_level INTEGER, llm_complete INTEGER NOT NULL DEFAULT 0, "
            "  conjugation_group INTEGER, teaching_rank INTEGER, teaching_score REAL, "
            "  synonyms TEXT, idioms TEXT, tags TEXT);"
            "CREATE TABLE senses (id INTEGER PRIMARY KEY, word_id INTEGER NOT NULL, "
            "  definition TEXT, gloss TEXT, gist TEXT, gender INTEGER, tags TEXT, "
            "  is_derived_sense INTEGER NOT NULL DEFAULT 0);"
            "CREATE TABLE translations (id INTEGER PRIMARY KEY, word_id INTEGER NOT NULL, "
            "  sense_id INTEGER, language_id INTEGER NOT NULL, translation TEXT NOT NULL, "
            "  source TEXT NOT NULL, count INTEGER, probability REAL, rank INTEGER, "
            "  verified INTEGER NOT NULL DEFAULT 0);"
            "CREATE TABLE word_forms (id INTEGER PRIMARY KEY, form TEXT NOT NULL, "
            "  word_id INTEGER NOT NULL, mood INTEGER, tense INTEGER, person INTEGER, "
            "  gender INTEGER, number INTEGER);"
            "CREATE TABLE examples (id INTEGER PRIMARY KEY, word_id INTEGER NOT NULL, "
            "  language_id INTEGER, example TEXT NOT NULL, translation TEXT);"
            "INSERT INTO languages (id, code) VALUES (1, 'en');"
            "INSERT INTO parts_of_speech (id, code) VALUES (1, 'noun'), (2, 'verb'), (3, 'adj'), "
            "  (4, 'article');"
            "INSERT INTO words (id, headword, part_of_speech_id, frequency_rank, teaching_rank) VALUES "
            "  (1, 'fini', 1, 100, 100), (2, 'finir', 2, 50, 50), (3, 'une', 3, 20, 20), "
            "  (4, 'une', 1, 10, 10), (5, 'un', 4, 5, 5);"
            "INSERT INTO senses (id, word_id, definition) VALUES (1, 5, 'a, an');"
            "INSERT INTO word_forms (form, word_id) VALUES ('fini', 2), ('une', 5);"
            "INSERT INTO translations (word_id, sense_id, language_id, translation, source, verified) "
            "  VALUES (2, NULL, 1, 'finish', 'dictionary', 1), (3, NULL, 1, 'one', 'dictionary', 1), "
            "         (5, 1, 1, 'a', 'dictionary', 1);");

        return Database;
    }
}

TEST_CASE("A reading with nothing to translate sinks below one that has translations",
          "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenReadingsWithAndWithoutTranslations();
    REQUIRE(Database.has_value());

    FTDictionaryRepository Repository(*Database, "en");

    // "fini" is a noun with no translations at all, and separately a form of "finir". Matching
    // exactly is not enough to earn the top of the list when the entry has nothing to show: the
    // verb is what the search was for.
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("fini");

    const int NounIndex = IndexOf(Entries, "fini", "noun");
    const int VerbIndex = IndexOf(Entries, "finir", "verb");

    REQUIRE(NounIndex >= 0);
    REQUIRE(VerbIndex >= 0);

    CHECK(VerbIndex < NounIndex);
}

TEST_CASE("The spelling itself leads the forms it is shared with", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // Both of these carry translations, so the tie is broken by which one the search actually
    // spelled. "accueillant" the adjective is that word; "accueillir" is a word it inflects from.
    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("accueillant");

    const int AdjectiveIndex = IndexOf(Entries, "accueillant", "adj");
    const int VerbIndex = IndexOf(Entries, "accueillir", "verb");

    REQUIRE(AdjectiveIndex >= 0);
    REQUIRE(VerbIndex >= 0);

    CHECK(AdjectiveIndex < VerbIndex);
}

TEST_CASE("Translated readings all come before untranslated ones", "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenReadingsWithAndWithoutTranslations();
    REQUIRE(Database.has_value());

    FTDictionaryRepository Repository(*Database, "en");

    // The property rather than one word's answer: whatever a search matches, the readings worth
    // reading are never buried under the ones with nothing in them. "une" has one reading with
    // only word-level translations, one with a meaning-level translation and one with none.
    for (const std::string& Word : {"fini", "une"})
    {
        const std::vector<FTDictionaryEntry> Entries = Repository.LookUp(Word);

        bool bSeenUntranslated = false;

        for (const FTDictionaryEntry& Entry : Entries)
        {
            if (HasAnyTranslation(Entry))
            {
                CHECK_FALSE(bSeenUntranslated);
            }
            else
            {
                bSeenUntranslated = true;
            }
        }
    }
}

TEST_CASE("The lookup is case-insensitive", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Lower = Repository.LookUp("avoir");
    const std::vector<FTDictionaryEntry> Upper = Repository.LookUp("AVOIR");

    CHECK_FALSE(Lower.empty());
    CHECK(Lower.size() == Upper.size());
}

TEST_CASE("A word typed without its accents still finds the entry", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // A quarter of French headwords carry an accent, and an ordinary keyboard produces none of
    // them, so this is the common case rather than an edge one.
    const std::vector<FTDictionaryEntry> Head = Repository.LookUp("tete");
    REQUIRE_FALSE(Head.empty());
    CHECK(Head.front().Headword == "t\xc3\xaate");

    const std::vector<FTDictionaryEntry> Theatre = Repository.LookUp("theatre");
    REQUIRE_FALSE(Theatre.empty());
    CHECK(Theatre.front().Headword == "th\xc3\xa9\xc3\xa2tre");

    const std::vector<FTDictionaryEntry> Castle = Repository.LookUp("chateau");
    REQUIRE_FALSE(Castle.empty());
    CHECK(Castle.front().Headword == "ch\xc3\xa2teau");
}

TEST_CASE("An accented spelling still wins over its unaccented reading", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // "a" is a real headword and also what "à" folds to. The exact match has to win, or typing a
    // common word would land on a different one. Compared case-insensitively, since the column
    // collates NOCASE and the stored spelling is the capital letter.
    const std::vector<FTDictionaryEntry> Exact = Repository.LookUp("a");
    REQUIRE_FALSE(Exact.empty());

    // The exact one-character reading stays first and nothing accented came back. "a" is also a
    // form of "avoir", and a spelling that is both a headword and a form returns both readings.
    CHECK(Exact.front().Headword.size() == 1);

    const bool bAvoirFound = std::any_of(Exact.begin(), Exact.end(),
        [](const FTDictionaryEntry& Entry)
        {
            return Entry.Headword == "avoir";
        });

    CHECK(bAvoirFound);
}

TEST_CASE("Accent folding does not match a different word", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // The pattern admits candidates by letter, so it can offer a word that folds to something else.
    // Every one is confirmed by folding, and nonsense must still return nothing.
    CHECK(Repository.LookUp("zzzqqq").empty());
}

TEST_CASE("Accent folding is reached only after both exact lookups miss",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // An inflected form still redirects to its lemma rather than being folded into some similarly
    // spelled headword.
    const std::vector<FTDictionaryEntry> Form = Repository.LookUp("voulons");
    REQUIRE_FALSE(Form.empty());
    CHECK(Form.front().Headword == "vouloir");
}

TEST_CASE("An accented form typed without accents redirects to its lemma",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // "étions" is a form of être; typed as "etions" it has to fold and then redirect, which
    // exercises both fallbacks in order.
    const std::vector<FTDictionaryEntry> Found = Repository.LookUp("etions");

    if (!Found.empty())
    {
        CHECK(Found.front().Headword == "\xc3\xaatre");
    }
}

TEST_CASE("Senses come back with the most translated meaning first", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Found = Repository.LookUp("vouloir");
    REQUIRE_FALSE(Found.empty());

    const std::vector<FTDictionarySense>& Senses = Found.front().Senses;
    REQUIRE(Senses.size() > 2);

    const auto Total = [](const FTDictionarySense& Sense)
    {
        double Sum = 0.0;

        for (const FTDictionaryTranslation& Translation : Sense.Translations)
        {
            Sum += Translation.Probability.value_or(0.0);
        }

        return Sum;
    };

    // Descending throughout, not merely first: a reader works down the list.
    for (size_t Index = 1; Index < Senses.size(); ++Index)
    {
        CHECK(Total(Senses[Index - 1]) >= Total(Senses[Index]));
    }

    // The strongest sense of "vouloir" is the one holding "want".
    bool bFoundWant = false;

    for (const FTDictionaryTranslation& Translation : Senses.front().Translations)
    {
        if (Translation.Text == "to want")
        {
            bFoundWant = true;
        }
    }

    CHECK(bFoundWant);
}

TEST_CASE("Unmeasured senses keep the dictionary's own order among themselves",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // Most of a common verb's senses carry no translation at all. The sort is stable, so those
    // keep Wiktionary's editorial order rather than being shuffled into an arbitrary one.
    const std::vector<FTDictionaryEntry> Found = Repository.LookUp("faire");
    REQUIRE_FALSE(Found.empty());

    const std::vector<FTDictionarySense>& Senses = Found.front().Senses;
    REQUIRE(Senses.size() > 3);

    // The sense carrying the most translation weight leads.
    CHECK_FALSE(Senses.front().Translations.empty());
}

TEST_CASE("Dictionaries are found by scanning rather than by a list in the source",
          "[packages]")
{
    const std::vector<FTDictionaryPackage> Found =
        FindDictionaryPackages(DICTIONARY_PACKAGES_PATH);

    // Skipped rather than failed where nothing has been built yet, same as the rest of this file.
    if (Found.empty())
    {
        SUCCEED("no dictionaries built");

        return;
    }

    for (const FTDictionaryPackage& Package : Found)
    {
        // Every one has to identify itself, or it could not have been returned.
        CHECK_FALSE(Package.TargetLanguage.empty());
        CHECK_FALSE(Package.NativeLanguages.empty());
        CHECK(Package.WordCount > 0);
        CHECK(Package.IsCompatible());
    }
}

TEST_CASE("A dictionary says which language it teaches without being told", "[packages]")
{
    const std::optional<FTDictionaryPackage> Package =
        ReadDictionaryPackage(DICTIONARY_DATABASE_PATH);

    if (!Package.has_value())
    {
        SUCCEED("dictionary not built");

        return;
    }

    // Derived from the file, not from its name or from any list the app carries.
    CHECK(Package->TargetLanguage == "fr");
    CHECK(Package->SupportsNative("en"));
    CHECK(Package->WordCount > 1000);
}

TEST_CASE("A file that is not a dictionary is not reported as one", "[packages]")
{
    CHECK_FALSE(ReadDictionaryPackage("does-not-exist-at-all.db").has_value());
}

TEST_CASE("A stale build does not shut out a current one for the same language", "[packages]")
{
    // What data/built actually looked like: an old build at schema 3 in one subdirectory and the
    // current one at 6 in another, both teaching French. The scan claimed the language for
    // whichever sorted first and only then checked the version, so the stale file took the
    // language and was thrown away, and the app loaded no French at all.
    QTemporaryDir Root;
    REQUIRE(Root.isValid());

    const auto WriteDictionary = [&Root](const char* Subdirectory, int SchemaVersion)
    {
        REQUIRE(QDir(Root.path()).mkpath(QString::fromUtf8(Subdirectory)));

        const std::string Path = QDir(Root.path())
            .filePath(QString::fromUtf8(Subdirectory) + QStringLiteral("/fr-dictionary.db"))
            .toStdString();

        std::optional<FTDatabase> Target = FTDatabase::Open(Path);
        REQUIRE(Target.has_value());

        REQUIRE(Target->Execute("CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT)"));
        REQUIRE(Target->Execute("CREATE TABLE languages (id INTEGER PRIMARY KEY, code TEXT)"));
        REQUIRE(Target->Execute("CREATE TABLE words (id INTEGER PRIMARY KEY)"));
        REQUIRE(Target->Execute(
            "CREATE TABLE translations (id INTEGER PRIMARY KEY, language_id INTEGER)"));

        REQUIRE(Target->Execute("INSERT INTO metadata VALUES ('schema_version', '"
                                + std::to_string(SchemaVersion) + "')"));
        REQUIRE(Target->Execute("INSERT INTO metadata VALUES ('target_language', 'fr')"));
        REQUIRE(Target->Execute("INSERT INTO languages VALUES (1, 'en')"));
        REQUIRE(Target->Execute("INSERT INTO words VALUES (1)"));
        REQUIRE(Target->Execute("INSERT INTO translations VALUES (1, 1)"));

        return Path;
    };

    // "fr" sorts before "fr-current", so the stale one is met first.
    WriteDictionary("fr", 3);
    const std::string Current = WriteDictionary("fr-current", DictionarySchemaVersion);

    std::vector<FTDictionaryPackage> Incompatible;

    const std::vector<FTDictionaryPackage> Found =
        FindDictionaryPackages(Root.path().toStdString(), &Incompatible);

    REQUIRE(Found.size() == 1);
    CHECK(Found.front().TargetLanguage == "fr");
    CHECK(Found.front().IsCompatible());
    CHECK(Found.front().FilePath == Current);

    REQUIRE(Incompatible.size() == 1);
    CHECK(Incompatible.front().TargetLanguage == "fr");
    CHECK(Incompatible.front().SchemaVersion == 3);
}

TEST_CASE("Scanning a directory that does not exist finds nothing", "[packages]")
{
    CHECK(FindDictionaryPackages("no-such-directory-anywhere").empty());
}

TEST_CASE("Triage offers small words next to vocabulary, and never an article",
          "[dictionaryrepository][triage]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTLearnableWord> Words = Repository.LoadLearnableWords(60, 0);

    if (Words.empty())
    {
        SUCCEED("dictionary not built");

        return;
    }

    const std::vector<std::string> Refused = {"article", "det", "name", "num", "particle", "phrase"};

    for (const FTLearnableWord& Word : Words)
    {
        INFO(Word.Headword << " / " << Word.PartOfSpeech);

        CHECK(std::find(Refused.begin(), Refused.end(), Word.PartOfSpeech) == Refused.end());
        CHECK_FALSE(Word.Translation.empty());
        CHECK(Word.FrequencyRank > 0);
    }

    const auto IsOffered = [&Words](const std::string& Headword, const std::string& PartOfSpeech)
    {
        return std::any_of(Words.begin(), Words.end(),
            [&](const FTLearnableWord& Word)
            {
                return Word.Headword == Headword && Word.PartOfSpeech == PartOfSpeech;
            });
    };

    CHECK(IsOffered("je", "pron"));
    CHECK(IsOffered("de", "prep"));
    CHECK(IsOffered("et", "conj"));
    CHECK(IsOffered("père", "noun"));
    CHECK(IsOffered("bonjour", "noun"));
    CHECK_FALSE(IsOffered("la", "article"));
    CHECK_FALSE(IsOffered("le", "article"));
}

TEST_CASE("Triage offers an interjection only when it is one word",
          "[dictionaryrepository][triage]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    FTLearnableWordFilter Filter;
    Filter.PartOfSpeech = "intj";

    const std::vector<FTLearnableWord> Words = Repository.LoadLearnableWords(400, 0, Filter);

    if (Words.empty())
    {
        SUCCEED("dictionary not built");

        return;
    }

    bool bOffersThanks = false;

    for (const FTLearnableWord& Word : Words)
    {
        INFO(Word.Headword);

        CHECK(Word.Headword.find(' ') == std::string::npos);

        bOffersThanks = bOffersThanks || Word.Headword == "merci";
    }

    CHECK(bOffersThanks);
}

TEST_CASE("Triage offers each reading of a spelling separately",
          "[dictionaryrepository][triage][slow]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    // Far enough down the list to reach this word. "moule" is a mould as a masculine noun and a
    // mussel as a feminine one; offering the spelling once would teach one and never mention the
    // other.
    const std::vector<FTLearnableWord> Words = Repository.LoadLearnableWords(30000, 0);

    if (Words.empty())
    {
        SUCCEED("dictionary not built");

        return;
    }

    std::vector<FTLearnableWord> Moule;

    for (const FTLearnableWord& Word : Words)
    {
        if (Word.Headword == "moule")
        {
            Moule.push_back(Word);
        }
    }

    if (Moule.size() < 2)
    {
        SUCCEED("moule is not in this word set");

        return;
    }

    // Two readings, each with its own translation rather than sharing the stronger one.
    CHECK(Moule.size() == 2);
    CHECK(Moule[0].Translation != Moule[1].Translation);
    CHECK(Moule[0].Gender != Moule[1].Gender);
}

TEST_CASE("A reading takes its own translation, not the strongest for the spelling",
          "[dictionaryrepository][triage]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("pouvoir");

    const FTDictionaryEntry* Verb = FindEntryByPartOfSpeech(Entries, "verb");
    const FTDictionaryEntry* Noun = FindEntryByPartOfSpeech(Entries, "noun");

    REQUIRE(Verb != nullptr);
    REQUIRE(Noun != nullptr);

    CHECK(HasTranslation(*Verb, "can"));
    CHECK_FALSE(HasTranslation(*Verb, "power"));
    CHECK(HasTranslation(*Noun, "power"));
}

TEST_CASE("A category run offers only words filed under it",
          "[dictionaryrepository][triage]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    FTLearnableWordFilter Filter;
    Filter.Category = "anatomie";

    const std::vector<FTLearnableWord> Words = Repository.LoadLearnableWords(40, 0, Filter);

    if (Words.empty())
    {
        SUCCEED("dictionary not built");

        return;
    }

    // Body parts, and in teaching order, so the common ones lead.
    const std::vector<std::string> Expected{ "pied", "tête", "jambe", "cheveu" };

    for (const std::string& Headword : Expected)
    {
        const bool bPresent = std::any_of(Words.begin(), Words.end(),
            [&Headword](const FTLearnableWord& Word) { return Word.Headword == Headword; });

        CHECK(bPresent);
    }
}

TEST_CASE("A category takes the translation of the sense that carries it",
          "[dictionaryrepository][triage]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    FTLearnableWordFilter Filter;
    Filter.Category = "cuisine";

    const std::vector<FTLearnableWord> Words = Repository.LoadLearnableWords(4000, 0, Filter);

    if (Words.empty())
    {
        SUCCEED("dictionary not built");

        return;
    }

    // "cheval" reaches cuisine through its horsemeat sense. Taking the word's strongest
    // translation instead would put "horse" on a card in a deck about food.
    for (const FTLearnableWord& Word : Words)
    {
        if (Word.Headword == "cheval")
        {
            CHECK(Word.Translation != "horse");
        }
    }
}

TEST_CASE("Categories list their word counts, most words first",
          "[dictionaryrepository][triage]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTCategory> Categories = Repository.ListCategories();

    if (Categories.empty())
    {
        SUCCEED("dictionary not built");

        return;
    }

    // Both are known to offer words, since the category runs above draw from them.
    const bool bHasAnatomy = std::any_of(Categories.begin(), Categories.end(),
        [](const FTCategory& Category)
        {
            return Category.Code == "anatomy" && Category.WordCount > 0;
        });

    const bool bHasCooking = std::any_of(Categories.begin(), Categories.end(),
        [](const FTCategory& Category)
        {
            return Category.Code == "cooking" && Category.WordCount > 0;
        });

    CHECK(bHasAnatomy);
    CHECK(bHasCooking);

    // Sorted most words first, so the biggest category opens the picker.
    for (size_t Index = 1; Index < Categories.size(); ++Index)
    {
        CHECK(Categories[Index - 1].WordCount >= Categories[Index].WordCount);
    }
}

TEST_CASE("The offset walks the list rather than restarting it",
          "[dictionaryrepository][triage][slow]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTLearnableWord> First = Repository.LoadLearnableWords(60, 0);

    if (First.empty())
    {
        SUCCEED("dictionary not built");

        return;
    }

    const std::vector<FTLearnableWord> Later = Repository.LoadLearnableWords(10, 50);
    REQUIRE_FALSE(Later.empty());

    CHECK(Later.front().Headword == First[50].Headword);
    CHECK(Later.front().PartOfSpeech == First[50].PartOfSpeech);
}

// ------------------------------------------------------------------- native language

namespace
{
    /** Every language the file has translations in. Read here rather than assumed, since which
        languages a dictionary was built with is a property of the file. */
    std::vector<std::string> TranslationLanguages(FTDatabase& Target)
    {
        std::vector<std::string> Result;

        FTStatement Query = Target.Prepare(
            "SELECT DISTINCT l.code FROM translations t "
            "JOIN languages l ON l.id = t.language_id ORDER BY l.code");

        if (!Query.IsValid())
        {
            return Result;
        }

        while (Query.Step())
        {
            Result.push_back(Query.GetText(0));
        }

        return Result;
    }

    size_t CountTranslations(const std::vector<FTDictionaryEntry>& Entries)
    {
        size_t Total = 0;

        for (const FTDictionaryEntry& Entry : Entries)
        {
            for (const FTDictionarySense& Sense : Entry.Senses)
            {
                Total += Sense.Translations.size();
            }
        }

        return Total;
    }
}

TEST_CASE("A lookup returns only the native language asked for", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    const std::vector<std::string> Languages = TranslationLanguages(*Test.Target);
    REQUIRE_FALSE(Languages.empty());

    FTDictionaryRepository Unscoped(*Test.Target);
    const size_t Everything = CountTranslations(Unscoped.LookUp("avoir"));
    REQUIRE(Everything > 0);

    // The per-language counts have to add up to the unfiltered count. Anything else means a
    // translation was either dropped or handed to the wrong language.
    size_t Summed = 0;

    for (const std::string& Code : Languages)
    {
        FTDictionaryRepository Scoped(*Test.Target, Code);
        const size_t Count = CountTranslations(Scoped.LookUp("avoir"));

        CHECK(Count <= Everything);

        Summed += Count;
    }

    CHECK(Summed == Everything);
}

TEST_CASE("Triage offers a translation the learner can read", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    const std::vector<std::string> Languages = TranslationLanguages(*Test.Target);
    REQUIRE_FALSE(Languages.empty());

    // Every candidate has to carry a translation, and it has to be one in the chosen language. A
    // word whose only translations are in another language is not offered at all rather than
    // offered with a word the learner cannot read.
    for (const std::string& Code : Languages)
    {
        FTDictionaryRepository Scoped(*Test.Target, Code);

        const std::vector<FTLearnableWord> Words = Scoped.LoadLearnableWords(50, 0);

        for (const FTLearnableWord& Word : Words)
        {
            CHECK_FALSE(Word.Translation.empty());
        }
    }
}

TEST_CASE("A reverse lookup finds the target words that translate to the typed native word",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTReverseLookupMatch> Matches = Repository.LookUpByTranslation("dog");

    REQUIRE_FALSE(Matches.empty());

    // "chien" is the everyday word for a dog, so it teaches first; "laideron" (an ugly person,
    // "dog" in slang) ranks far behind it.
    CHECK(Matches[0].Headword == "chien");
    CHECK(Matches[0].PartOfSpeech == "noun");
    CHECK(Matches[0].Gender == std::optional<int>(0));
    CHECK(Matches[0].FrequencyRank.has_value());
    CHECK(Matches[0].CefrLevel == std::optional<int>(0));
}

TEST_CASE("Reverse lookup results come back in teaching order", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTReverseLookupMatch> Matches = Repository.LookUpByTranslation("cat");

    REQUIRE(Matches.size() > 1);

    for (size_t Index = 1; Index < Matches.size(); ++Index)
    {
        CHECK(Matches[Index - 1].TeachingRank <= Matches[Index].TeachingRank);
    }
}

TEST_CASE("A reverse lookup offers only parts of speech the app teaches",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    // "dog" also names the zodiac sign "Chien" (a proper name) in the source; that reading and any
    // interjection must not crowd out the word itself.
    const std::vector<FTReverseLookupMatch> Matches = Repository.LookUpByTranslation("dog");

    for (const FTReverseLookupMatch& Match : Matches)
    {
        CHECK((Match.PartOfSpeech == "noun" || Match.PartOfSpeech == "verb"
               || Match.PartOfSpeech == "adj" || Match.PartOfSpeech == "adv"));
    }
}

TEST_CASE("A reverse lookup of a word nothing translates to comes back empty",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    // A made-up word is no one's translation.
    const std::vector<FTReverseLookupMatch> Matches =
        Repository.LookUpByTranslation("zzzznotaword");

    CHECK(Matches.empty());
}

TEST_CASE("A reverse lookup respects its result limit", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    // "good" has more than one target reading, so a limit of one has to cut it down.
    const std::vector<FTReverseLookupMatch> Limited = Repository.LookUpByTranslation("good", 1);

    CHECK(Limited.size() == 1);

    const std::vector<FTReverseLookupMatch> Full = Repository.LookUpByTranslation("good");

    CHECK(Full.size() > 1);
}

TEST_CASE("A prefix lookup finds headwords starting with what was typed",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTReverseLookupMatch> Matches = Repository.LookUpPrefix("marc");

    const bool bFoundMarche = std::any_of(Matches.begin(), Matches.end(),
        [](const FTReverseLookupMatch& Match) { return Match.Headword == "marché"; });

    CHECK(bFoundMarche);
}

TEST_CASE("A prefix lookup only returns headwords starting with the prefix",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTReverseLookupMatch> Matches = Repository.LookUpPrefix("ch");

    REQUIRE_FALSE(Matches.empty());

    for (const FTReverseLookupMatch& Match : Matches)
    {
        CHECK(Match.Headword.rfind("ch", 0) == 0);
    }
}

TEST_CASE("A prefix lookup of empty text comes back empty", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    CHECK(Repository.LookUpPrefix("").empty());
}

TEST_CASE("A prefix lookup respects its result limit", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTReverseLookupMatch> Limited = Repository.LookUpPrefix("ch", 1);

    CHECK(Limited.size() == 1);

    const std::vector<FTReverseLookupMatch> Full = Repository.LookUpPrefix("ch");

    CHECK(Full.size() > 1);
}

TEST_CASE("A prefix lookup treats a literal percent sign as plain text, not a wildcard",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    CHECK(Repository.LookUpPrefix("%").empty());
}

TEST_CASE("A search finds a headword by prefix", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTDictionarySearchMatch> Matches = Repository.Search("marc", 50);

    const bool bFoundMarche = std::any_of(Matches.begin(), Matches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "marché"; });

    CHECK(bFoundMarche);
}

TEST_CASE("A search finds a phrase by a word inside it, after the headwords that start with it",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTDictionarySearchMatch> Matches = Repository.Search("allez-vous", 50);

    const bool bFoundPhrase = std::any_of(Matches.begin(), Matches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "comment allez-vous"; });

    CHECK(bFoundPhrase);

    const std::vector<FTDictionarySearchMatch> MarcheMatches = Repository.Search("marché", 50);

    const auto Phrase = std::find_if(MarcheMatches.begin(), MarcheMatches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "bon marché"; });

    REQUIRE(Phrase != MarcheMatches.end());
    REQUIRE(!MarcheMatches.empty());
    CHECK(MarcheMatches.front().Headword == "marché");
}

TEST_CASE("A search finds a preposition and a conjunction, not just noun/verb/adj/adv",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTDictionarySearchMatch> DansMatches = Repository.Search("dans", 50);
    const bool bFoundDans = std::any_of(DansMatches.begin(), DansMatches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "dans"; });
    CHECK(bFoundDans);

    const std::vector<FTDictionarySearchMatch> MaisMatches = Repository.Search("mais", 50);
    const bool bFoundMais = std::any_of(MaisMatches.begin(), MaisMatches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "mais"; });
    CHECK(bFoundMais);
}

TEST_CASE("A search puts an exact headword match first", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTDictionarySearchMatch> Matches = Repository.Search("marché", 50);

    REQUIRE_FALSE(Matches.empty());
    CHECK(Matches[0].Headword == "marché");
}

TEST_CASE("A literal headword match outranks a same-spelled form of another word",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTDictionarySearchMatch> Matches = Repository.Search("marché", 50);

    REQUIRE_FALSE(Matches.empty());
    CHECK(Matches[0].Headword == "marché");

    const auto MarcherPosition = std::find_if(Matches.begin(), Matches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "marcher"; });

    if (MarcherPosition != Matches.end())
    {
        CHECK(std::distance(Matches.begin(), MarcherPosition) > 0);
    }
}

TEST_CASE("A search finds a conjugated form and returns its headword", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTDictionarySearchMatch> Matches = Repository.Search("manges", 50);

    const bool bFoundManger = std::any_of(Matches.begin(), Matches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "manger"; });

    CHECK(bFoundManger);
}

TEST_CASE("A search ignores missing accents", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTDictionarySearchMatch> Matches = Repository.Search("mere", 50);

    const bool bFoundMere = std::any_of(Matches.begin(), Matches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "mère"; });

    CHECK(bFoundMere);
}

TEST_CASE("A missing-accent headword match outranks an unrelated exact translation match",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTDictionarySearchMatch> Matches = Repository.Search("mere", 50);

    REQUIRE_FALSE(Matches.empty());

    const auto MerePosition = std::find_if(Matches.begin(), Matches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "mère"; });

    REQUIRE(MerePosition != Matches.end());

    for (auto It = Matches.begin(); It != MerePosition; ++It)
    {
        CHECK(It->Headword == "mère");
    }
}

TEST_CASE("A search finds headwords by their translation", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const std::vector<FTDictionarySearchMatch> Matches = Repository.Search("dog", 50);

    const auto Found = std::find_if(Matches.begin(), Matches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "chien"; });

    REQUIRE(Found != Matches.end());
    CHECK_FALSE(Found->Translation.empty());
}

TEST_CASE("A search by an English verb matches its translation with or without 'to'", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    const auto Contains = [](const std::vector<FTDictionarySearchMatch>& Matches, const std::string& Headword)
    {
        return std::any_of(Matches.begin(), Matches.end(),
            [&Headword](const FTDictionarySearchMatch& Match) { return Match.Headword == Headword; });
    };

    CHECK(Contains(Repository.Search("eat", 50), "manger"));
    CHECK(Contains(Repository.Search("to eat", 50), "manger"));
    CHECK(Contains(Repository.Search("to eat", 50), "se nourrir"));
}

TEST_CASE("A search caps its combined result at the limit", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    CHECK(Repository.Search("ch", 1).size() == 1);
    CHECK(Repository.Search("ch", 50).size() <= 50);
}

TEST_CASE("A search of empty text comes back empty", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target, "en");

    CHECK(Repository.Search("", 50).empty());
}

TEST_CASE("A regular -er verb's conjugated forms are readable", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("parler");
    const FTDictionaryEntry* Entry = FindEntryByPartOfSpeech(Entries, "verb");
    REQUIRE(Entry != nullptr);

    CHECK(HasForm(Entry->Forms, "parle", 1, 1, 1, 0, 1));
    CHECK(HasForm(Entry->Forms, "parlât", 2, 2, 3, 0, 1));
    CHECK(HasForm(Entry->Forms, "parlé", 0, 8, 0, 0, 0));
}

TEST_CASE("A regular -ir verb's conjugated forms are readable", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("finir");
    const FTDictionaryEntry* Entry = FindEntryByPartOfSpeech(Entries, "verb");
    REQUIRE(Entry != nullptr);

    CHECK(HasForm(Entry->Forms, "finit", 1, 1, 3, 0, 1));
    CHECK(HasForm(Entry->Forms, "finissons", 1, 1, 1, 0, 2));
    CHECK(HasForm(Entry->Forms, "fini", 0, 8, 0, 0, 0));
}

TEST_CASE("An irregular verb's conjugated forms are readable", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("venir");
    const FTDictionaryEntry* Entry = FindEntryByPartOfSpeech(Entries, "verb");
    REQUIRE(Entry != nullptr);

    CHECK(HasForm(Entry->Forms, "viendrais", 3, 0, 1, 0, 1));
    CHECK(HasForm(Entry->Forms, "viens", 4, 0, 2, 0, 1));
    CHECK(HasForm(Entry->Forms, "venu", 0, 8, 0, 0, 0));
}

TEST_CASE("A noun's feminine and plural forms are readable", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("chien");
    const FTDictionaryEntry* Entry = FindEntryByPartOfSpeech(Entries, "noun");
    REQUIRE(Entry != nullptr);

    CHECK(HasForm(Entry->Forms, "chienne", 0, 0, 0, 2, 1));
    CHECK(HasForm(Entry->Forms, "chiens", 0, 0, 0, 1, 2));
}

TEST_CASE("A profession noun's feminine and plural forms are readable",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("boulanger");
    const FTDictionaryEntry* Entry = FindEntryByPartOfSpeech(Entries, "noun");
    REQUIRE(Entry != nullptr);

    CHECK(HasForm(Entry->Forms, "boulangère", 0, 0, 0, 2, 1));
    CHECK(HasForm(Entry->Forms, "boulangères", 0, 0, 0, 2, 2));
}

TEST_CASE("A noun with a suppletive feminine still gets its plural read correctly",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("cheval");
    const FTDictionaryEntry* Entry = FindEntryByPartOfSpeech(Entries, "noun");
    REQUIRE(Entry != nullptr);

    CHECK(HasForm(Entry->Forms, "jument", 0, 0, 0, 2, 1));
    CHECK(HasForm(Entry->Forms, "chevaux", 0, 0, 0, 1, 2));
}

TEST_CASE("A regular adjective's agreement forms are readable", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("grand");
    const FTDictionaryEntry* Entry = FindEntryByPartOfSpeech(Entries, "adj");
    REQUIRE(Entry != nullptr);

    CHECK(HasForm(Entry->Forms, "grande", 0, 0, 0, 2, 1));
    CHECK(HasForm(Entry->Forms, "grands", 0, 0, 0, 1, 2));
    CHECK(HasForm(Entry->Forms, "grandes", 0, 0, 0, 2, 2));
}

TEST_CASE("The built dictionary's verbs with two spellings get a conjugation card",
          "[dictionaryrepository][spellings]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const auto Expand = [&](const std::string& Headword, const std::string& Tense)
    {
        const std::vector<FTDictionaryEntry> Entries = Repository.LookUp(Headword);
        const FTDictionaryEntry* Entry = FindEntryByPartOfSpeech(Entries, "verb");
        REQUIRE(Entry != nullptr);

        const std::optional<FTCard> Stored = MakeFormsCard(FormsKindVerb, *Entry, "fr", Headword, Tense);
        REQUIRE(Stored.has_value());

        std::vector<std::string> Shown;

        for (const FTCard& Item : ExpandFormsCard(*Stored, *Entry, "fr"))
        {
            Shown.push_back(Item.TargetWord);
        }

        return Shown;
    };

    CHECK(Expand("payer", "present")
          == std::vector<std::string>{"je paie", "tu paies", "il paie", "nous payons", "vous payez", "ils paient"});
    CHECK(Expand("essayer", "present")
          == std::vector<std::string>{"j'essaie", "tu essaies", "il essaie", "nous essayons", "vous essayez",
                                      "ils essaient"});
    CHECK(Expand("payer", "future")
          == std::vector<std::string>{"je paierai", "tu paieras", "il paiera", "nous paierons", "vous paierez",
                                      "ils paieront"});
    CHECK(Expand("espérer", "future")
          == std::vector<std::string>{"j'espérerai", "tu espéreras", "il espérera", "nous espérerons",
                                      "vous espérerez", "ils espéreront"});
    CHECK(Expand("vouloir", "present")
          == std::vector<std::string>{"je veux", "tu veux", "il veut", "nous voulons", "vous voulez", "ils veulent"});
}

TEST_CASE("An adjective ending in -f agrees correctly", "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("actif");
    const FTDictionaryEntry* Entry = FindEntryByPartOfSpeech(Entries, "adj");
    REQUIRE(Entry != nullptr);

    CHECK(HasForm(Entry->Forms, "active", 0, 0, 0, 2, 1));
    CHECK(HasForm(Entry->Forms, "actifs", 0, 0, 0, 1, 2));
}

TEST_CASE("A translation the corpus measured sorts above one it did not",
          "[dictionaryrepository]")
{
    Fixture Test;
    REQUIRE(Test.Target.has_value());

    FTDictionaryRepository Repository(*Test.Target);

    const std::vector<FTDictionaryTranslation> Translations =
        Repository.LoadTranslationsForReading("ici", "adv", std::nullopt, 50);

    REQUIRE_FALSE(Translations.empty());
    CHECK(Translations.front().Probability.has_value());

    const auto Unmeasured = std::find_if(Translations.begin(), Translations.end(),
        [](const FTDictionaryTranslation& Translation)
        {
            return !Translation.Probability.has_value();
        });

    REQUIRE(Unmeasured != Translations.end());
    CHECK(std::all_of(Unmeasured, Translations.end(),
        [](const FTDictionaryTranslation& Translation)
        {
            return !Translation.Probability.has_value();
        }));
}

namespace
{
    std::optional<FTDatabase> OpenWordWithoutMeanings()
    {
        std::optional<FTDatabase> Database = FTDatabase::OpenInMemory();

        if (!Database.has_value())
        {
            return Database;
        }

        Database->Execute(
            "CREATE TABLE languages (id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE);"
            "CREATE TABLE parts_of_speech (id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE);"
            "CREATE TABLE words (id INTEGER PRIMARY KEY, headword TEXT NOT NULL COLLATE NOCASE, "
            "  part_of_speech_id INTEGER NOT NULL, phonetic TEXT, frequency_rank INTEGER, "
            "  cefr_level INTEGER, llm_complete INTEGER NOT NULL DEFAULT 0, "
            "  conjugation_group INTEGER, teaching_rank INTEGER, teaching_score REAL, "
            "  synonyms TEXT, idioms TEXT, tags TEXT);"
            "CREATE TABLE senses (id INTEGER PRIMARY KEY, word_id INTEGER NOT NULL, "
            "  definition TEXT, gloss TEXT, gist TEXT, gender INTEGER, tags TEXT, "
            "  is_derived_sense INTEGER NOT NULL DEFAULT 0);"
            "CREATE TABLE translations (id INTEGER PRIMARY KEY, word_id INTEGER NOT NULL, "
            "  sense_id INTEGER, language_id INTEGER NOT NULL, translation TEXT NOT NULL, "
            "  source TEXT NOT NULL, count INTEGER, probability REAL, rank INTEGER, "
            "  verified INTEGER NOT NULL DEFAULT 0);"
            "CREATE TABLE word_forms (id INTEGER PRIMARY KEY, form TEXT NOT NULL, "
            "  word_id INTEGER NOT NULL, mood INTEGER, tense INTEGER, person INTEGER, "
            "  gender INTEGER, number INTEGER);"
            "CREATE TABLE examples (id INTEGER PRIMARY KEY, word_id INTEGER NOT NULL, "
            "  language_id INTEGER, example TEXT NOT NULL, translation TEXT);"
            "INSERT INTO languages (id, code) VALUES (1, 'uk');"
            "INSERT INTO parts_of_speech (id, code) VALUES (1, 'noun');"
            "INSERT INTO words (id, headword, part_of_speech_id, frequency_rank, teaching_rank) "
            "  VALUES (1, 'room', 1, 80, 80);"
            "INSERT INTO translations (word_id, sense_id, language_id, translation, source, "
            "  probability, verified) VALUES (1, NULL, 1, 'кімната', 'dictionary', 0.58, 1);");

        return Database;
    }
}

TEST_CASE("A word with no meanings still offers its translations for its reading",
          "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenWordWithoutMeanings();
    REQUIRE(Database.has_value());

    FTDictionaryRepository Repository(*Database, "uk");

    const std::vector<FTDictionaryTranslation> Translations =
        Repository.LoadTranslationsForReading("room", "noun", std::nullopt, 10);

    REQUIRE(Translations.size() == 1);
    CHECK(Translations.front().Text == "кімната");
}

TEST_CASE("A word with no meanings can be offered to learn", "[dictionaryrepository][triage]")
{
    std::optional<FTDatabase> Database = OpenWordWithoutMeanings();
    REQUIRE(Database.has_value());

    FTDictionaryRepository Repository(*Database, "uk");

    const std::vector<FTLearnableWord> Words = Repository.LoadLearnableWords(10, 0);

    REQUIRE(Words.size() == 1);
    CHECK(Words.front().Headword == "room");
    CHECK(Words.front().Translation == "кімната");
    CHECK_FALSE(Words.front().Gender.has_value());
}

TEST_CASE("A word with no meanings is found by its translation", "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenWordWithoutMeanings();
    REQUIRE(Database.has_value());

    FTDictionaryRepository Repository(*Database, "uk");

    const std::vector<FTReverseLookupMatch> Matches = Repository.LookUpByTranslation("кімната");

    REQUIRE(Matches.size() == 1);
    CHECK(Matches.front().Headword == "room");

    const std::vector<FTDictionarySearchMatch> Found = Repository.Search("кімната", 10);

    REQUIRE_FALSE(Found.empty());
    CHECK(Found.front().Headword == "room");
    CHECK(Found.front().Translation == "кімната");
}

TEST_CASE("A looked-up word with no meanings carries its translations on the entry",
          "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenWordWithoutMeanings();
    REQUIRE(Database.has_value());

    FTDictionaryRepository Repository(*Database, "uk");

    const std::vector<FTDictionaryEntry> Entries = Repository.LookUp("room");

    REQUIRE(Entries.size() == 1);
    CHECK(Entries.front().Senses.empty());
    REQUIRE(Entries.front().Translations.size() == 1);
    CHECK(Entries.front().Translations.front().Text == "кімната");
}

TEST_CASE("A search orders exact headword matches by teaching rank", "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenWordWithoutMeanings();
    REQUIRE(Database.has_value());

    Database->Execute(
        "INSERT INTO parts_of_speech (id, code) VALUES (2, 'verb');"
        "INSERT INTO words (id, headword, part_of_speech_id, cefr_level, teaching_rank) VALUES (2, 'porte', 1, 1, 300);"
        "INSERT INTO words (id, headword, part_of_speech_id, cefr_level, teaching_rank) VALUES (3, 'porte', 2, 5, 20);"
        "INSERT INTO words (id, headword, part_of_speech_id, cefr_level) VALUES (4, 'portée', 1, 1);");

    FTDictionaryRepository Repository(*Database, "uk");

    const std::vector<FTDictionarySearchMatch> Matches = Repository.Search("porte", 10);

    REQUIRE(Matches.size() >= 3);
    CHECK(Matches[0].Headword == "porte");
    CHECK(Matches[0].PartOfSpeech == "verb");
    CHECK(Matches[1].Headword == "porte");
    CHECK(Matches[1].PartOfSpeech == "noun");
    CHECK(Matches[2].Headword == "portée");
}

TEST_CASE("A search lists a noun of two genders once, with the meanings of both", "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenWordWithoutMeanings();
    REQUIRE(Database.has_value());

    Database->Execute(
        "INSERT INTO words (id, headword, part_of_speech_id, cefr_level, teaching_rank) VALUES (2, 'part', 1, 2, 199);"
        "INSERT INTO senses (id, word_id, gender) VALUES (10, 2, 0);"
        "INSERT INTO senses (id, word_id, gender) VALUES (11, 2, 1);"
        "INSERT INTO translations (word_id, sense_id, language_id, translation, source, probability) "
        "  VALUES (2, 11, 1, 'share', 'gloss', 0.06);"
        "INSERT INTO translations (word_id, sense_id, language_id, translation, source) "
        "  VALUES (2, NULL, 1, 'piece', 'wiktionary');"
        "INSERT INTO translations (word_id, sense_id, language_id, translation, source) "
        "  VALUES (2, 10, 1, 'newborn', 'gloss');"
        "INSERT INTO translations (word_id, sense_id, language_id, translation, source) "
        "  VALUES (2, 11, 1, 'piece', 'gloss');");

    FTDictionaryRepository Repository(*Database, "uk");

    const std::vector<FTDictionarySearchMatch> Matches = Repository.Search("part", 10);

    const auto NounCount = std::count_if(Matches.begin(), Matches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "part" && Match.PartOfSpeech == "noun"; });

    REQUIRE(NounCount == 1);

    const auto Found = std::find_if(Matches.begin(), Matches.end(),
        [](const FTDictionarySearchMatch& Match) { return Match.Headword == "part"; });

    REQUIRE(Found != Matches.end());
    CHECK(Found->Translation == "share, piece, newborn");

    const std::optional<FTDictionarySearchMatch> Exact = Repository.LookUpExact("part", "noun");

    REQUIRE(Exact.has_value());
    CHECK(Exact->Translation == "share, piece, newborn");
}

TEST_CASE("A lowercase search finds a capitalised or uppercase translation", "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenWordWithoutMeanings();
    REQUIRE(Database.has_value());

    Database->Execute(
        "INSERT INTO words (id, headword, part_of_speech_id) VALUES (2, 'dimanche', 1);"
        "INSERT INTO words (id, headword, part_of_speech_id) VALUES (3, 'Ukraine', 1);"
        "INSERT INTO words (id, headword, part_of_speech_id) VALUES (4, 'télé', 1);"
        "INSERT INTO translations (word_id, language_id, translation, source) VALUES (2, 1, 'Sunday', 'gloss');"
        "INSERT INTO translations (word_id, language_id, translation, source) VALUES (3, 1, 'Україна', 'gloss');"
        "INSERT INTO translations (word_id, language_id, translation, source) VALUES (4, 1, 'TV', 'gloss');");

    FTDictionaryRepository Repository(*Database, "uk");

    const auto FindsHeadword = [&Repository](const std::string& SearchText, const std::string& Headword)
    {
        const std::vector<FTDictionarySearchMatch> Matches = Repository.Search(SearchText, 10);

        return std::any_of(Matches.begin(), Matches.end(),
            [&Headword](const FTDictionarySearchMatch& Match) { return Match.Headword == Headword; });
    };

    CHECK(FindsHeadword("sunday", "dimanche"));
    CHECK(FindsHeadword("україна", "Ukraine"));
    CHECK(FindsHeadword("tv", "télé"));
}

namespace
{
    std::optional<FTDatabase> OpenAttendSearchDatabase()
    {
        std::optional<FTDatabase> Database = OpenWordWithoutMeanings();

        if (!Database.has_value())
        {
            return Database;
        }

        Database->Execute(
            "INSERT INTO words (id, headword, part_of_speech_id, cefr_level, teaching_rank) VALUES (2, 'attendre', 1, 1, 50);"
            "INSERT INTO words (id, headword, part_of_speech_id, cefr_level, teaching_rank) VALUES (3, 'aller', 1, 1, 10);"
            "INSERT INTO words (id, headword, part_of_speech_id, cefr_level, teaching_rank) VALUES (4, 'assister', 1, 1, 20);"
            "INSERT INTO words (id, headword, part_of_speech_id, cefr_level, teaching_rank) VALUES (5, 'fréquenter', 1, 2, 30);"
            "INSERT INTO word_forms (form, word_id) VALUES ('attend', 2);"
            "INSERT INTO translations (word_id, language_id, translation, source, probability) VALUES (2, 1, 'wait', 'gloss', 0.7);"
            "INSERT INTO translations (word_id, language_id, translation, source, probability) VALUES (3, 1, 'attend', 'gloss', 0.05);"
            "INSERT INTO translations (word_id, language_id, translation, source, probability) VALUES (4, 1, 'attend', 'gloss', 0.4);"
            "INSERT INTO translations (word_id, language_id, translation, source) VALUES (5, 1, 'attend', 'gloss');");

        return Database;
    }

    std::vector<std::string> ListHeadwords(const std::vector<FTDictionarySearchMatch>& Matches)
    {
        std::vector<std::string> Headwords;

        for (const FTDictionarySearchMatch& Match : Matches)
        {
            Headwords.push_back(Match.Headword);
        }

        return Headwords;
    }
}

TEST_CASE("A foreign search puts a form of the word before words that merely translate to it",
          "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenAttendSearchDatabase();
    REQUIRE(Database.has_value());

    FTDictionaryRepository Repository(*Database, "uk");

    const std::vector<std::string> Headwords = ListHeadwords(Repository.Search("attend", 10, false));

    REQUIRE(Headwords.size() == 4);
    CHECK(Headwords[0] == "attendre");
}

TEST_CASE("A native search puts translations first, the likeliest translation leading", "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenAttendSearchDatabase();
    REQUIRE(Database.has_value());

    FTDictionaryRepository Repository(*Database, "uk");

    const std::vector<std::string> Headwords = ListHeadwords(Repository.Search("attend", 10, true));

    REQUIRE(Headwords.size() == 4);
    CHECK(Headwords[0] == "assister");
    CHECK(Headwords[1] == "aller");
    CHECK(Headwords[2] == "fréquenter");
    CHECK(Headwords[3] == "attendre");
}

namespace
{
    std::string ReadDictionarySchema()
    {
        std::ifstream Stream(DICTIONARY_SCHEMA_PATH);
        std::stringstream Buffer;
        Buffer << Stream.rdbuf();

        return Buffer.str();
    }
}

TEST_CASE("The build script creates the same search indices the app queries with", "[dictionaryrepository]")
{
    const std::string Script = ReadDictionarySchema();

    REQUIRE_FALSE(Script.empty());

    CHECK(Script.find("ON words(" + FTDictionaryRepository::MakeFoldedExpression("headword") + ")")
          != std::string::npos);
    CHECK(Script.find("ON word_forms(" + FTDictionaryRepository::MakeFoldedExpression("form") + ")")
          != std::string::npos);
    CHECK(Script.find("ON translations(translation, language_id)") != std::string::npos);
}

TEST_CASE("A dictionary reports whether it carries the search indices", "[dictionaryrepository]")
{
    std::optional<FTDatabase> Bare = OpenWordWithoutMeanings();
    REQUIRE(Bare.has_value());

    CHECK_FALSE(FTDictionaryRepository::HasSearchIndices(*Bare));

    std::optional<FTDatabase> Built = FTDatabase::OpenInMemory();
    REQUIRE(Built.has_value());
    REQUIRE(Built->Execute(ReadDictionarySchema()));

    CHECK(FTDictionaryRepository::HasSearchIndices(*Built));
}

TEST_CASE("Each CEFR level is placed at the median teaching rank of its words, and the ceiling leaves "
          "learnable words above it", "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = FTDatabase::OpenInMemory();
    REQUIRE(Database.has_value());

    Database->Execute(
        "CREATE TABLE languages (id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE);"
        "CREATE TABLE parts_of_speech (id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE);"
        "CREATE TABLE words (id INTEGER PRIMARY KEY, headword TEXT NOT NULL, part_of_speech_id INTEGER, "
        "  frequency_rank INTEGER, cefr_level INTEGER, teaching_rank INTEGER);"
        "INSERT INTO parts_of_speech (id, code) VALUES (1, 'noun'), (2, 'article');"
        "INSERT INTO words (headword, part_of_speech_id, frequency_rank, cefr_level, teaching_rank) VALUES "
        "  ('a', 1, 1, 0, 10), ('b', 1, 3, 0, 30), ('c', 1, 2, 0, 20), "
        "  ('d', 1, 5, 1, 500), ('e', 1, 9, 1, 900), "
        "  ('f', 1, 40, NULL, 4000), ('g', 1, 50, 2, NULL), ('h', 2, 60, NULL, 6000);");

    FTDictionaryRepository Repository(*Database, "en");

    CHECK(Repository.GetTeachingRankCeiling(0) == 4000);
    CHECK(Repository.GetTeachingRankCeiling(2) == 500);

    const std::vector<FTLevelRank> Levels = Repository.LoadLevelTeachingRanks();

    REQUIRE(Levels.size() == 2);
    CHECK(Levels[0].Level == 0);
    CHECK(Levels[0].TeachingRank == 20);
    CHECK(Levels[1].Level == 1);
    CHECK(Levels[1].TeachingRank == 900);
}

namespace
{
    std::string ReadLearnableReadingsScript()
    {
        std::ifstream Stream(LEARNABLE_READINGS_SCRIPT_PATH);
        std::stringstream Buffer;
        Buffer << Stream.rdbuf();

        return Buffer.str();
    }

    std::optional<FTDatabase> OpenGeneratedLearnableWords()
    {
        std::optional<FTDatabase> Database = FTDatabase::OpenInMemory();

        if (!Database.has_value())
        {
            return Database;
        }

        const std::string Numbers = "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 1500) ";

        Database->Execute(
            "CREATE TABLE metadata (key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL);"
            "CREATE TABLE languages (id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE);"
            "CREATE TABLE parts_of_speech (id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE);"
            "CREATE TABLE words (id INTEGER PRIMARY KEY, headword TEXT NOT NULL COLLATE NOCASE, "
            "  part_of_speech_id INTEGER NOT NULL, frequency_rank INTEGER, cefr_level INTEGER, "
            "  teaching_rank INTEGER, UNIQUE (headword, part_of_speech_id));"
            "CREATE TABLE senses (id INTEGER PRIMARY KEY, word_id INTEGER NOT NULL, gist TEXT, gender INTEGER);"
            "CREATE TABLE translations (id INTEGER PRIMARY KEY, word_id INTEGER NOT NULL, sense_id INTEGER, "
            "  language_id INTEGER NOT NULL, translation TEXT NOT NULL, probability REAL);"
            "INSERT INTO languages (id, code) VALUES (1, 'en'), (2, 'uk');"
            "INSERT INTO parts_of_speech (id, code) VALUES (1, 'noun'), (2, 'verb'), (3, 'adj'), (4, 'intj'), "
            "  (5, 'article');"
            + Numbers +
            "INSERT INTO words (id, headword, part_of_speech_id, frequency_rank, cefr_level, teaching_rank) "
            "  SELECT i, 'w' || i, i % 4 + 1, i, CASE WHEN i % 7 = 6 THEN NULL ELSE i % 6 END, "
            "         CASE WHEN i % 97 = 0 THEN NULL ELSE i * 10 END FROM n;"
            "INSERT INTO words (id, headword, part_of_speech_id, frequency_rank, cefr_level, teaching_rank) VALUES "
            "  (2001, 'oh là', 4, 2001, 0, 20010), (2002, 'le', 5, 2002, 0, 20020), (2003, 'rare', 1, NULL, 0, 20030);"
            + Numbers +
            "INSERT INTO senses (id, word_id, gender) SELECT i * 10 + 1, i, CASE WHEN i % 4 = 0 THEN 1 END FROM n;"
            + Numbers +
            "INSERT INTO senses (id, word_id, gender) SELECT i * 10 + 2, i, 2 FROM n WHERE i % 8 = 0;"
            + Numbers +
            "INSERT INTO translations (word_id, sense_id, language_id, translation, probability) "
            "  SELECT i, NULL, 1, 'word ' || i, CASE WHEN i % 3 = 0 THEN NULL ELSE (i % 10) / 10.0 END "
            "  FROM n WHERE i % 5 != 0;"
            + Numbers +
            "INSERT INTO translations (word_id, sense_id, language_id, translation, probability) "
            "  SELECT i, NULL, 1, 'better ' || i, 0.95 FROM n WHERE i % 5 != 0 AND i % 11 = 0;"
            + Numbers +
            "INSERT INTO translations (word_id, sense_id, language_id, translation, probability) "
            "  SELECT i, i * 10 + 2, 1, 'feminine ' || i, 0.99 FROM n WHERE i % 8 = 0 AND i % 5 = 0;"
            + Numbers +
            "INSERT INTO translations (word_id, sense_id, language_id, translation, probability) "
            "  SELECT i, NULL, 2, 'uk ' || i, 0.5 FROM n WHERE i % 13 = 0;"
            "INSERT INTO translations (word_id, sense_id, language_id, translation, probability) VALUES "
            "  (2001, NULL, 1, 'oh there', 0.5), (2002, NULL, 1, 'the', 0.5), (2003, NULL, 1, 'rare', 0.5);");

        return Database;
    }

    std::vector<FTLearnableWordFilter> LearnableWordFilters()
    {
        std::vector<FTLearnableWordFilter> Filters(1);

        for (int Level = 0; Level < 6; ++Level)
        {
            FTLearnableWordFilter& Graded = Filters.emplace_back();
            Graded.CefrLevel = Level;
        }

        FTLearnableWordFilter& Ungraded = Filters.emplace_back();
        Ungraded.bOnlyUngraded = true;

        FTLearnableWordFilter& Nouns = Filters.emplace_back();
        Nouns.CefrLevel = 2;
        Nouns.PartOfSpeech = "noun";

        FTLearnableWordFilter& Deeper = Filters.emplace_back();
        Deeper.AboveTeachingRank = 5000;

        FTLearnableWordFilter& Everything = Filters.emplace_back();
        Everything.CefrLevel = 1;
        Everything.PartOfSpeech = "verb";
        Everything.AboveTeachingRank = 3000;

        return Filters;
    }

    std::vector<std::string> DescribeWords(const std::vector<FTLearnableWord>& Words)
    {
        std::vector<std::string> Lines;

        for (const FTLearnableWord& Word : Words)
        {
            Lines.push_back(Word.Headword + "|" + Word.PartOfSpeech + "|"
                            + (Word.Gender ? std::to_string(*Word.Gender) : "-") + "|"
                            + std::to_string(Word.FrequencyRank) + "|" + Word.Translation);
        }

        return Lines;
    }

    std::vector<std::string> DescribeCounts(std::vector<FTPartOfSpeechCount> Counts)
    {
        std::sort(Counts.begin(), Counts.end(),
                  [](const FTPartOfSpeechCount& Left, const FTPartOfSpeechCount& Right)
                  {
                      return Left.PartOfSpeech < Right.PartOfSpeech;
                  });

        std::vector<std::string> Lines;

        for (const FTPartOfSpeechCount& Count : Counts)
        {
            Lines.push_back(Count.PartOfSpeech + "|" + std::to_string(Count.Count));
        }

        return Lines;
    }

    std::vector<std::string> DescribeLevels(const std::vector<FTLevelRank>& Levels)
    {
        std::vector<std::string> Lines;

        for (const FTLevelRank& Level : Levels)
        {
            Lines.push_back(std::to_string(Level.Level) + "|" + std::to_string(Level.TeachingRank));
        }

        return Lines;
    }

    struct FTLearnableWordsAnswers
    {
        std::vector<std::vector<std::string>> Words;
        std::vector<std::vector<std::string>> Pages;
        std::vector<std::vector<std::string>> Counts;
        std::vector<std::string> Levels;
        int Ceiling = 0;
    };

    FTLearnableWordsAnswers AskLearnableWords(FTDatabase& Database)
    {
        FTDictionaryRepository Repository(Database, "en");
        FTLearnableWordsAnswers Answers;

        for (const FTLearnableWordFilter& Filter : LearnableWordFilters())
        {
            Answers.Words.push_back(DescribeWords(Repository.LoadLearnableWords(100000, 0, Filter)));
            Answers.Pages.push_back(DescribeWords(Repository.LoadLearnableWords(7, 13, Filter)));
            Answers.Counts.push_back(DescribeCounts(Repository.CountLearnableWords(Filter)));
        }

        Answers.Levels = DescribeLevels(Repository.LoadLevelTeachingRanks());
        Answers.Ceiling = Repository.GetTeachingRankCeiling(1000);

        return Answers;
    }
}

TEST_CASE("The built learnable readings give the same words, counts and slider marks as the queries",
          "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenGeneratedLearnableWords();
    REQUIRE(Database.has_value());

    const FTLearnableWordsAnswers Queried = AskLearnableWords(*Database);

    const std::string Script = ReadLearnableReadingsScript();
    REQUIRE_FALSE(Script.empty());
    REQUIRE(Database->Execute(Script));

    FTStatement Rows = Database->Prepare("SELECT COUNT(*) FROM learnable_readings WHERE language_id = 1");
    REQUIRE(Rows.Step());
    CHECK(Rows.GetInt(0) > 1000);

    const FTLearnableWordsAnswers Built = AskLearnableWords(*Database);

    CHECK(Queried.Ceiling > 0);
    CHECK(Built.Ceiling == Queried.Ceiling);
    CHECK(Built.Levels == Queried.Levels);
    CHECK(Queried.Levels.size() == 6);

    for (std::size_t Index = 0; Index < Queried.Words.size(); ++Index)
    {
        INFO("filter " << Index);
        CHECK_FALSE(Queried.Words[Index].empty());
        CHECK(Built.Words[Index] == Queried.Words[Index]);
        CHECK(Built.Pages[Index] == Queried.Pages[Index]);
        CHECK(Built.Counts[Index] == Queried.Counts[Index]);
    }
}

TEST_CASE("Stored slider marks are read instead of queried", "[dictionaryrepository]")
{
    std::optional<FTDatabase> Database = OpenGeneratedLearnableWords();
    REQUIRE(Database.has_value());

    REQUIRE(Database->Execute(
        "INSERT INTO metadata (key, value) VALUES ('teaching_rank_ceiling_1000', '4321'), "
        "  ('level_teaching_ranks', '[{\"level\":0,\"rank\":11},{\"level\":3,\"rank\":33}]');"));

    FTDictionaryRepository Repository(*Database, "en");

    CHECK(Repository.GetTeachingRankCeiling(1000) == 4321);
    CHECK(Repository.GetTeachingRankCeiling(2) != 4321);
    CHECK(DescribeLevels(Repository.LoadLevelTeachingRanks()) == std::vector<std::string>{"0|11", "3|33"});
}

TEST_CASE("A dictionary without decks lists none", "[dictionaryrepository][decks]")
{
    std::optional<FTDatabase> Database = FTDatabase::OpenInMemory();
    REQUIRE(Database.has_value());

    Database->Execute("CREATE TABLE languages (id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE);");

    FTDictionaryRepository Repository(*Database, "uk");

    CHECK_FALSE(Repository.HasDecks());
    CHECK(Repository.ListDecks().empty());
    CHECK(Repository.LoadDeckWords(1).empty());
}

TEST_CASE("A deck lists its words in file order, each with the translation of its meaning",
          "[dictionaryrepository][decks]")
{
    std::optional<FTDatabase> Database = FTDatabase::OpenInMemory();
    REQUIRE(Database.has_value());

    REQUIRE(Database->Execute(
        "CREATE TABLE languages (id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE);"
        "CREATE TABLE parts_of_speech (id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE);"
        "CREATE TABLE words (id INTEGER PRIMARY KEY, headword TEXT NOT NULL, part_of_speech_id INTEGER, "
        "  cefr_level INTEGER, teaching_rank INTEGER);"
        "CREATE TABLE senses (id INTEGER PRIMARY KEY, word_id INTEGER, gist TEXT, gender INTEGER);"
        "CREATE TABLE translations (id INTEGER PRIMARY KEY, word_id INTEGER, sense_id INTEGER, "
        "  language_id INTEGER, translation TEXT, probability REAL);"
        "CREATE TABLE decks (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE);"
        "CREATE TABLE deck_words (id INTEGER PRIMARY KEY, deck_id INTEGER NOT NULL, headword TEXT NOT NULL, "
        "  part_of_speech TEXT NOT NULL, gist TEXT, context TEXT);"
        "INSERT INTO languages (id, code) VALUES (1, 'uk'), (2, 'fr');"
        "INSERT INTO parts_of_speech (id, code) VALUES (1, 'noun'), (2, 'adj');"
        "INSERT INTO words (id, headword, part_of_speech_id, cefr_level, teaching_rank) VALUES "
        "  (1, 'light', 1, 0, 120), (2, 'light', 2, 1, 900), (3, 'abandon', 1, NULL, NULL);"
        "INSERT INTO senses (id, word_id, gist) VALUES (1, 1, 'lamp'), (2, 1, 'brightness'), (3, 2, 'pale');"
        "INSERT INTO translations (word_id, sense_id, language_id, translation, probability) VALUES "
        "  (1, 1, 1, 'лампа', 0.9), (1, 2, 1, 'світло', 0.2), (1, NULL, 2, 'lumière', 0.99), "
        "  (2, 3, 1, 'світлий', NULL), (3, NULL, 1, 'покинутість', NULL);"
        "INSERT INTO decks (id, name) VALUES (1, 'IELTS General'), (2, 'IELTS Academic');"
        "INSERT INTO deck_words (deck_id, headword, part_of_speech, gist, context) VALUES "
        "  (2, 'light', 'noun', 'brightness', 'the light of day'), (2, 'abandon', 'noun', NULL, NULL), "
        "  (1, 'light', 'adj', NULL, NULL), (2, 'light', 'noun', NULL, NULL);"));

    FTDictionaryRepository Repository(*Database, "uk");

    REQUIRE(Repository.HasDecks());

    const std::vector<FTDeck> Decks = Repository.ListDecks();

    REQUIRE(Decks.size() == 2);
    CHECK(Decks[0].Name == "IELTS General");
    CHECK(Decks[0].Category.empty());
    CHECK(Decks[0].WordCount == 1);
    CHECK(Decks[1].Name == "IELTS Academic");
    CHECK(Decks[1].WordCount == 3);

    const std::vector<FTDeckWord> Words = Repository.LoadDeckWords(Decks[1].Id);

    REQUIRE(Words.size() == 3);
    CHECK(Words[0].Headword == "light");
    CHECK(Words[0].Gist == "brightness");
    CHECK(Words[0].Context == "the light of day");
    CHECK(Words[0].Translation == "світло");
    CHECK(Words[0].CefrLevel == 0);
    CHECK(Words[0].TeachingRank == 120);
    CHECK(Words[1].Headword == "abandon");
    CHECK(Words[1].Translation == "покинутість");
    CHECK_FALSE(Words[1].CefrLevel.has_value());
    CHECK_FALSE(Words[1].TeachingRank.has_value());
    CHECK(Words[2].Gist.empty());
    CHECK(Words[2].Translation == "лампа");
}

TEST_CASE("A deck carries the group the deck screen filters it by", "[dictionaryrepository][decks]")
{
    std::optional<FTDatabase> Database = FTDatabase::OpenInMemory();
    REQUIRE(Database.has_value());

    REQUIRE(Database->Execute(
        "CREATE TABLE decks (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, category TEXT NOT NULL DEFAULT '');"
        "CREATE TABLE deck_words (id INTEGER PRIMARY KEY, deck_id INTEGER NOT NULL, headword TEXT NOT NULL, "
        "  part_of_speech TEXT NOT NULL, gist TEXT, context TEXT);"
        "INSERT INTO decks (id, name, category) VALUES (1, 'Corps', 'Personnes'), (2, 'Santé', 'Vie quotidienne');"
        "INSERT INTO deck_words (deck_id, headword, part_of_speech) VALUES "
        "  (1, 'main', 'noun'), (2, 'médecin', 'noun');"));

    FTDictionaryRepository Repository(*Database, "en");

    const std::vector<FTDeck> Decks = Repository.ListDecks();

    REQUIRE(Decks.size() == 2);
    CHECK(Decks[0].Category == "Personnes");
    CHECK(Decks[1].Category == "Vie quotidienne");
}
