#include <catch2/catch_test_macros.hpp>

#include "core/FTAnswerComparison.h"
#include "core/FTSettings.h"

TEST_CASE("A card's translations are split on commas", "[answer]")
{
    const std::vector<std::string> Two = SplitTranslations("time, weather");
    REQUIRE(Two.size() == 2);
    CHECK(Two[0] == "time");
    CHECK(Two[1] == "weather");

    // Spacing around the separator is the learner's typing, not part of the word.
    const std::vector<std::string> Spaced = SplitTranslations("  time ,weather  ");
    REQUIRE(Spaced.size() == 2);
    CHECK(Spaced[0] == "time");
    CHECK(Spaced[1] == "weather");

    const std::vector<std::string> One = SplitTranslations("eye");
    REQUIRE(One.size() == 1);
    CHECK(One[0] == "eye");

    CHECK(SplitTranslations("").empty());
}

TEST_CASE("Any one of a card's translations is the whole answer", "[answer]")
{
    // Reciting every meaning is a different exercise from knowing what the word means.
    CHECK(CompareAgainstAny("time", "time, weather") == EFTAnswerGrade::Exact);
    CHECK(CompareAgainstAny("weather", "time, weather") == EFTAnswerGrade::Exact);
    CHECK(CompareAgainstAny("clock", "time, weather") == EFTAnswerGrade::Wrong);

    // The best grade any of them earns, so an exact hit on the second is not marked down by a
    // near miss on the first.
    CHECK(CompareAgainstAny("cafe", "café, coffee shop") == EFTAnswerGrade::AccentsWrong);
    CHECK(CompareAgainstAny("coffee shop", "café, coffee shop") == EFTAnswerGrade::Exact);

    // The prefix rule still applies to each of them separately.
    CHECK(CompareAgainstAny("need", "the need, the requirement") == EFTAnswerGrade::Exact);

    // The whole comma-joined string is what the card actually shows as its translation, so typing
    // it back verbatim is the answer too, not only one piece split out of it.
    CHECK(CompareAgainstAny("place, location", "place, location") == EFTAnswerGrade::Exact);
}

TEST_CASE("A full list of translations matches in any order", "[answer]")
{
    // Same words, different order: the answer is the set of words, not their arrangement.
    CHECK(CompareAgainstAny("sight, vision", "vision, sight") == EFTAnswerGrade::Exact);
    CHECK(CompareAgainstAny("vision, sight", "sight, vision") == EFTAnswerGrade::Exact);
    CHECK(CompareAgainstAny("take, make", "make, take") == EFTAnswerGrade::Exact);

    // A single word from the list is still the whole answer, as before.
    CHECK(CompareAgainstAny("sight", "vision, sight") == EFTAnswerGrade::Exact);

    // Missing, wrong or extra words still fail.
    CHECK(CompareAgainstAny("sight, clock", "vision, sight") == EFTAnswerGrade::Wrong);
    CHECK(CompareAgainstAny("sight, vision, view", "vision, sight") == EFTAnswerGrade::Wrong);
    CHECK(CompareAgainstAny("make, take, make", "take, make") == EFTAnswerGrade::Wrong);

    // Accents grade the same way when the order is swapped.
    CHECK(CompareAgainstAny("cafe, vision", "vision, café") == EFTAnswerGrade::AccentsWrong);
}

TEST_CASE("The reading prefix is optional on each word of a list", "[answer]")
{
    // "to" is written once on the card ("to take, make"); typing it on every word is the same
    // answer, and so is leaving it out everywhere, in either order.
    CHECK(CompareAgainstAny("to take, to make", "to take, make") == EFTAnswerGrade::Exact);
    CHECK(CompareAgainstAny("take, make", "to take, make") == EFTAnswerGrade::Exact);
    CHECK(CompareAgainstAny("to take, make", "to take, make") == EFTAnswerGrade::Exact);
    CHECK(CompareAgainstAny("to take, to make", "make, to take") == EFTAnswerGrade::Exact);
}

TEST_CASE("The reading prefix is optional in both directions", "[answer]")
{
    // The prefix says which word class is being asked for. Typing it is welcome and leaving it
    // out is not a mistake, so both spellings are the same answer.
    CHECK(CompareAnswer("need", "the need") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("the need", "need") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("like", "to like") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("to like", "like") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("to like", "to like") == EFTAnswerGrade::Exact);

    // A wrong word is still wrong with the prefix attached.
    CHECK(CompareAnswer("to want", "to like") == EFTAnswerGrade::Wrong);

    // The French article is a different rule and still governed by bAllowMissingArticle.
    FTAnswerComparisonRules Strict;
    Strict.bAllowMissingArticle = false;
    CHECK(CompareAnswer("chien", "le chien", Strict) == EFTAnswerGrade::Wrong);
}

TEST_CASE("A ligature typed as two letters is exact", "[answer]")
{
    FTAnswerComparisonRules Rules;
    Rules.bRequireExactAccents = true;

    // A keyboard without the key is not a spelling mistake.
    CHECK(CompareAnswer("oeil", "\xC5\x93il", Rules) == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("soeur", "s\xC5\x93ur", Rules) == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("nevus", "n\xC3\xA6vus", Rules) != EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("naevus", "n\xC3\xA6vus", Rules) == EFTAnswerGrade::Exact);

    // The other direction too, so a card already written out long-hand accepts the ligature.
    CHECK(CompareAnswer("\xC5\x93il", "oeil", Rules) == EFTAnswerGrade::Exact);

    // Accents are a different matter and stay required, which is the whole reason this is a
    // separate rule from folding.
    CHECK(CompareAnswer("etre", "\xC3\xAAtre", Rules) == EFTAnswerGrade::Wrong);
}

TEST_CASE("An identical answer matches exactly", "[answer]")
{
    CHECK(CompareAnswer("le chien", "le chien") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("café", "café") == EFTAnswerGrade::Exact);
}

TEST_CASE("Case is ignored", "[answer]")
{
    CHECK(CompareAnswer("LE CHIEN", "le chien") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("Le Chien", "le chien") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("le chien", "LE CHIEN") == EFTAnswerGrade::Exact);
}

TEST_CASE("Case folding covers accented capitals", "[answer]")
{
    CHECK(CompareAnswer("CAFÉ", "café") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("ÊTRE", "être") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("GARÇON", "garçon") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("ÀÂÄÈÉÊËÎÏÔÖÙÛÜ", "àâäèéêëîïôöùûü") == EFTAnswerGrade::Exact);
}

TEST_CASE("Case folding covers the oe ligature", "[answer]")
{
    // French uses this in cœur, œuf, œuvre.
    CHECK(CompareAnswer("CŒUR", "cœur") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("Œuf", "œuf") == EFTAnswerGrade::Exact);
}

TEST_CASE("The oe ligature matches its two-letter spelling in either direction", "[answer]")
{
    CHECK(CompareAnswer("manoeuvre", "manœuvre") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("manœuvre", "manoeuvre") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("MANOEUVRE", "manœuvre") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("l'oeuf", "l'œuf") == EFTAnswerGrade::Exact);
}

TEST_CASE("German umlauts and the eszett are accepted spelled out", "[answer]")
{
    FTAnswerComparisonRules German;
    German.bSpellOutUmlauts = true;

    CHECK(CompareAnswer("Aepfel", "Äpfel", German) == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("schoen", "schön", German) == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("Tuer", "Tür", German) == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("Strasse", "Straße", German) == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("Äpfel", "Aepfel", German) == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("Apfel", "Äpfel", German) == EFTAnswerGrade::AccentsWrong);
    CHECK(CompareAnswer("Apfel", "Aepfel", German) == EFTAnswerGrade::Wrong);

    FTAnswerComparisonRules Strict = German;
    Strict.bRequireExactAccents = true;

    CHECK(CompareAnswer("Aepfel", "Äpfel", Strict) == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("Apfel", "Äpfel", Strict) == EFTAnswerGrade::Wrong);

    CHECK(CompareAgainstAny("Tuer, Tor", "Tür, Tor", German) == EFTAnswerGrade::Exact);
}

TEST_CASE("Umlaut spellings stay wrong outside German", "[answer]")
{
    CHECK(CompareAnswer("pingueino", "pingüino") == EFTAnswerGrade::Wrong);
}

TEST_CASE("Whitespace does not count", "[answer]")
{
    CHECK(CompareAnswer("  le chien  ", "le chien") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("le  chien", "le chien") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("lechien", "le chien") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("\tle\nchien ", "le chien") == EFTAnswerGrade::Exact);
}

TEST_CASE("Punctuation does not count", "[answer]")
{
    CHECK(CompareAnswer("bonjour !", "bonjour") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("bonjour", "Bonjour !") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("Ça va ?", "ça va") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("hola", "¡Hola!") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("qué tal", "¿Qué tal?") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("« merci. »", "merci") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("eh bien…", "eh bien") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("to need.", "to need") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("le cafe.", "le café") == EFTAnswerGrade::AccentsWrong);
    CHECK(CompareAgainstAny("weather!", "time, weather") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("leau", "l'eau") == EFTAnswerGrade::Wrong);
}

TEST_CASE("A space is accepted where a hyphen belongs", "[answer]")
{
    CHECK(CompareAnswer("peut être", "peut-être") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("peut-être", "peut-être") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("peutêtre", "peut-être") == EFTAnswerGrade::Exact);
}

TEST_CASE("A missing accent still counts as correct", "[answer]")
{
    // Reported rather than accepted silently, so the interface can show the real spelling.
    CHECK(CompareAnswer("le cafe", "le café") == EFTAnswerGrade::AccentsWrong);
    CHECK(CompareAnswer("etre", "être") == EFTAnswerGrade::AccentsWrong);
    CHECK(CompareAnswer("garcon", "garçon") == EFTAnswerGrade::AccentsWrong);

    // A ligature is not an accent: writing it out in full is the correct spelling of the word,
    // not a near miss at it. See the ligature test above.
    CHECK(CompareAnswer("coeur", "cœur") == EFTAnswerGrade::Exact);

    CHECK(IsCorrectGrade(CompareAnswer("le cafe", "le café")));
}

TEST_CASE("A wrong accent is treated the same as a missing one", "[answer]")
{
    CHECK(CompareAnswer("le cafè", "le café") == EFTAnswerGrade::AccentsWrong);
    CHECK(CompareAnswer("pécher", "pêcher") == EFTAnswerGrade::AccentsWrong);
}

TEST_CASE("Accent leniency cannot tell apart words that differ only by an accent", "[answer]")
{
    // The known cost of the rule. "ou" means or, "où" means where, and this accepts one for the
    // other. Recorded as a test so the behaviour is deliberate rather than a surprise.
    CHECK(CompareAnswer("ou", "où") == EFTAnswerGrade::AccentsWrong);
}

TEST_CASE("An exact answer is still exact, not downgraded", "[answer]")
{
    CHECK(CompareAnswer("le café", "le café") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("cœur", "cœur") == EFTAnswerGrade::Exact);
}

TEST_CASE("A missing article is a wrong answer", "[answer]")
{
    // The article is part of the word being learned. Accepting the bare noun would make gender
    // optional, which is the one thing the article rule exists to prevent.
    CHECK(CompareAnswer("chien", "le chien") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("maison", "la maison") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("gens", "les gens") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("livre", "un livre") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("voiture", "une voiture") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("eau", "l'eau") == EFTAnswerGrade::Wrong);
}

TEST_CASE("A typographic apostrophe is treated as a plain one", "[answer]")
{
    // U+2019 is what word processors and some keyboards produce.
    CHECK(CompareAnswer("l’eau", "l'eau") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("l'eau", "l’eau") == EFTAnswerGrade::Exact);
}

TEST_CASE("Every apostrophe variant is treated as a plain one", "[answer]")
{
    // U+2018 (left single quote) and U+02BC (modifier letter apostrophe) are the other two forms
    // IsApostrophe recognises, alongside U+2019 above.
    CHECK(CompareAnswer("l‘eau", "l'eau") == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("l\xCA\xBC" "eau", "l'eau") == EFTAnswerGrade::Exact);
}

TEST_CASE("A non-breaking space counts as whitespace", "[answer]")
{
    // U+00A0, the other space form IsIgnorableSpace recognises alongside the ASCII ones.
    CHECK(CompareAnswer("le\xC2\xA0" "chien", "le chien") == EFTAnswerGrade::Exact);
}

TEST_CASE("The wrong article is a wrong answer", "[answer]")
{
    CHECK(CompareAnswer("la chien", "le chien") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("le maison", "la maison") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("un voiture", "une voiture") == EFTAnswerGrade::Wrong);
}

TEST_CASE("An article that should not be there is a wrong answer", "[answer]")
{
    CHECK(CompareAnswer("le manger", "manger") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("la parler", "parler") == EFTAnswerGrade::Wrong);
}

TEST_CASE("A wrong article plus a missing accent is still wrong", "[answer]")
{
    // Accent leniency must not smuggle a gender mistake through.
    CHECK(CompareAnswer("la cafe", "le café") == EFTAnswerGrade::Wrong);
}

TEST_CASE("A word that merely starts like an article is left alone", "[answer]")
{
    FTAnswerComparisonRules Lenient;
    Lenient.bAllowMissingArticle = true;

    CHECK(CompareAnswer("cture", "lecture", Lenient) == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("ver", "laver", Lenient) == EFTAnswerGrade::Wrong);
}

TEST_CASE("A phrase that opens with an article word keeps it", "[answer]")
{
    FTAnswerComparisonRules Lenient;
    Lenient.bAllowMissingArticle = true;

    CHECK(CompareAnswer("siento", "lo siento", Lenient) == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("que", "o que", Lenient) == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("gente", "a gente", Lenient) == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("plupart", "la plupart", Lenient) == EFTAnswerGrade::Wrong);
}

TEST_CASE("An unrelated answer is wrong", "[answer]")
{
    CHECK(CompareAnswer("le chat", "le chien") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("", "le chien") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("   ", "le chien") == EFTAnswerGrade::Wrong);
}

TEST_CASE("Accent strictness can be turned on", "[answer]")
{
    FTAnswerComparisonRules Strict;
    Strict.bRequireExactAccents = true;

    CHECK(CompareAnswer("le cafe", "le café", Strict) == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("garcon", "garçon", Strict) == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("ou", "où", Strict) == EFTAnswerGrade::Wrong);

    CHECK(CompareAnswer("le café", "le café", Strict) == EFTAnswerGrade::Exact);
}

TEST_CASE("Article leniency can be turned on", "[answer]")
{
    FTAnswerComparisonRules Lenient;
    Lenient.bAllowMissingArticle = true;

    CHECK(CompareAnswer("chien", "le chien", Lenient, "chien") == EFTAnswerGrade::ArticleOmitted);
    CHECK(CompareAnswer("eau", "l'eau", Lenient, "eau") == EFTAnswerGrade::ArticleOmitted);
    CHECK(CompareAnswer("le chien", "le chien", Lenient, "chien") == EFTAnswerGrade::Exact);

    // A missing article combined with wrong accents reports the accents.
    CHECK(CompareAnswer("elephant", "l'éléphant", Lenient, "éléphant") == EFTAnswerGrade::AccentsWrong);
}

TEST_CASE("Article leniency needs the word without its article", "[answer]")
{
    FTAnswerComparisonRules Lenient;
    Lenient.bAllowMissingArticle = true;

    CHECK(CompareAnswer("chien", "le chien", Lenient) == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("chien", "le chien", {}, "chien") == EFTAnswerGrade::Wrong);
}

TEST_CASE("Article leniency covers every language's article", "[answer]")
{
    FTAnswerComparisonRules Lenient;
    Lenient.bAllowMissingArticle = true;

    CHECK(CompareAnswer("perro", "el perro", Lenient, "perro") == EFTAnswerGrade::ArticleOmitted);
    CHECK(CompareAnswer("agua", "el agua", Lenient, "agua") == EFTAnswerGrade::ArticleOmitted);
    CHECK(CompareAnswer("gato", "o gato", Lenient, "gato") == EFTAnswerGrade::ArticleOmitted);
    CHECK(CompareAnswer("casa", "a casa", Lenient, "casa") == EFTAnswerGrade::ArticleOmitted);
    CHECK(CompareAnswer("libro", "il libro", Lenient, "libro") == EFTAnswerGrade::ArticleOmitted);
    CHECK(CompareAnswer("studente", "lo studente", Lenient, "studente") == EFTAnswerGrade::ArticleOmitted);
    CHECK(CompareAnswer("amico", "l'amico", Lenient, "amico") == EFTAnswerGrade::ArticleOmitted);

    CHECK(CompareAnswer("la perro", "el perro", Lenient, "perro") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("il studente", "lo studente", Lenient, "studente") == EFTAnswerGrade::Wrong);
}

TEST_CASE("Whitespace leniency can be turned off", "[answer]")
{
    FTAnswerComparisonRules Strict;
    Strict.bIgnoreWhitespace = false;
    Strict.bIgnoreHyphens = false;

    CHECK(CompareAnswer("lechien", "le chien", Strict) == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("le chien", "le chien", Strict) == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer("peut être", "peut-être", Strict) == EFTAnswerGrade::Wrong);
}

TEST_CASE("Case sensitivity can be turned on", "[answer]")
{
    FTAnswerComparisonRules Strict;
    Strict.bIgnoreCase = false;

    CHECK(CompareAnswer("LE CHIEN", "le chien", Strict) == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("le chien", "le chien", Strict) == EFTAnswerGrade::Exact);
}

TEST_CASE("Everything but Wrong and Skipped scores as correct", "[answer]")
{
    CHECK(IsCorrectGrade(EFTAnswerGrade::Exact));
    CHECK(IsCorrectGrade(EFTAnswerGrade::ArticleOmitted));
    CHECK(IsCorrectGrade(EFTAnswerGrade::AccentsWrong));
    CHECK_FALSE(IsCorrectGrade(EFTAnswerGrade::Wrong));

    // Declining is not an answer, so it cannot be a correct one.
    CHECK_FALSE(IsCorrectGrade(EFTAnswerGrade::Skipped));
}

TEST_CASE("Comparing never produces Skipped", "[answer]")
{
    // That grade only ever comes from the user declining. An empty answer is wrong rather than
    // skipped, and the session is what tells those apart.
    CHECK(CompareAnswer("", "le chien") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("le chien", "") == EFTAnswerGrade::Wrong);
    CHECK(CompareAnswer("", "") != EFTAnswerGrade::Skipped);
}

TEST_CASE("Normalisation is exposed for the interface to show corrections", "[answer]")
{
    // Accents fold under the default rules, which is what makes a missing accent match.
    CHECK(NormalizeAnswer("  Le  Chien ") == "lechien");
    CHECK(NormalizeAnswer("CAFÉ") == "cafe");
    CHECK(NormalizeAnswer("cœur") == "coeur");
    CHECK(NormalizeAnswer("peut-être") == "peutetre");
    CHECK(NormalizeAnswer("l’eau") == "l'eau");

    FTAnswerComparisonRules Strict;
    Strict.bRequireExactAccents = true;
    CHECK(NormalizeAnswer("café", Strict) == "café");
    CHECK(NormalizeAnswer("cœur", Strict) == "cœur");
}

/*
 * The two leniency rules are user settings, so the settings singleton has to actually hand them to
 * the comparison. They were defaults nobody could reach until they were promoted, which is the kind
 * of gap that looks fine in isolation: the rules worked, nothing supplied them.
 */

namespace
{
    struct LeniencyGuard
    {
        LeniencyGuard()
            : Previous(FTSettings::Get().GetSessionExercises(TrainingSessionGroup))
            , bAllowMissingArticle(FTSettings::Get().GetLeniencyRules().bAllowMissingArticle)
        {
        }

        ~LeniencyGuard()
        {
            FTSettings::Get().SetSessionExercises(TrainingSessionGroup, Previous);
            FTSettings::Get().SetAllowMissingArticle(bAllowMissingArticle);
        }

        FTSessionExerciseRules Previous;
        bool bAllowMissingArticle = false;
    };

    void SetLeniency(bool bAllowMissingArticle, bool bRequireExactAccents)
    {
        FTSessionExerciseRules Rules = FTSettings::Get().GetSessionExercises(TrainingSessionGroup);
        Rules.bRequireExactAccents = bRequireExactAccents;

        FTSettings::Get().SetSessionExercises(TrainingSessionGroup, Rules);
        FTSettings::Get().SetAllowMissingArticle(bAllowMissingArticle);
    }

    FTAnswerComparisonRules TrainingComparisonRules()
    {
        return FTSettings::Get().GetComparisonRules(TrainingSessionGroup);
    }
}

TEST_CASE("The settings' comparison rules carry both leniency choices", "[answer]")
{
    LeniencyGuard Guard;

    SetLeniency(true, true);

    const FTAnswerComparisonRules Rules = TrainingComparisonRules();

    CHECK(Rules.bAllowMissingArticle);
    CHECK(Rules.bRequireExactAccents);

    SetLeniency(false, false);

    const FTAnswerComparisonRules Relaxed = TrainingComparisonRules();

    CHECK_FALSE(Relaxed.bAllowMissingArticle);
    CHECK_FALSE(Relaxed.bRequireExactAccents);
}

TEST_CASE("Case, whitespace and hyphens stay forgiven whatever the settings say", "[answer]")
{
    LeniencyGuard Guard;

    SetLeniency(false, true);

    const FTAnswerComparisonRules Rules = TrainingComparisonRules();

    // None of these is part of knowing a word, and a keyboard makes all three easy to get wrong, so
    // they are not offered as choices at all.
    CHECK(Rules.bIgnoreCase);
    CHECK(Rules.bIgnoreWhitespace);
    CHECK(Rules.bIgnoreHyphens);
}

TEST_CASE("A word judged with the settings' rules honours the article choice", "[answer]")
{
    LeniencyGuard Guard;

    SetLeniency(false, false);
    CHECK(CompareAnswer("chien", "le chien", TrainingComparisonRules(), "chien") == EFTAnswerGrade::Wrong);

    SetLeniency(true, false);
    CHECK(IsCorrectGrade(CompareAnswer("chien", "le chien", TrainingComparisonRules(), "chien")));
}

/*
 * Ukrainian marks stress with an acute over the vowel. Dictionaries carry it, keyboards do not
 * produce it, and it says nothing about which word this is - so it cannot be what separates a
 * right answer from a wrong one, in either direction.
 */

TEST_CASE("A Ukrainian stress mark is ignored on either side", "[answer]")
{
    FTAnswerComparisonRules Rules;

    // "дивитися", written with and without the acute on the second и.
    const std::string Stressed = "\xD0\xB4\xD0\xB8\xD0\xB2\xD0\xB8\xCC\x81"
                                 "\xD1\x82\xD0\xB8\xD1\x81\xD1\x8F";
    const std::string Plain = "\xD0\xB4\xD0\xB8\xD0\xB2\xD0\xB8\xD1\x82\xD0\xB8\xD1\x81\xD1\x8F";

    CHECK(CompareAnswer(Plain, Stressed, Rules) == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer(Stressed, Plain, Rules) == EFTAnswerGrade::Exact);
    CHECK(CompareAnswer(Stressed, Stressed, Rules) == EFTAnswerGrade::Exact);
}

TEST_CASE("A Ukrainian stress mark is ignored even with accents required", "[answer]")
{
    // Not the kind of accent that setting is about. Requiring exact accents is about French, where
    // dropping one changes the word; a stress mark never does.
    FTAnswerComparisonRules Rules;
    Rules.bRequireExactAccents = true;

    const std::string Stressed = "\xD0\xB4\xD0\xB8\xD0\xB2\xD0\xB8\xCC\x81"
                                 "\xD1\x82\xD0\xB8\xD1\x81\xD1\x8F";
    const std::string Plain = "\xD0\xB4\xD0\xB8\xD0\xB2\xD0\xB8\xD1\x82\xD0\xB8\xD1\x81\xD1\x8F";

    CHECK(CompareAnswer(Plain, Stressed, Rules) == EFTAnswerGrade::Exact);
}

TEST_CASE("A stress mark does not make two different words equal", "[answer]")
{
    // Dropping the mark is all that happens. The letters underneath still have to match.
    FTAnswerComparisonRules Rules;

    const std::string Stressed = "\xD0\xB4\xD0\xB8\xD0\xB2\xD0\xB8\xCC\x81"
                                 "\xD1\x82\xD0\xB8\xD1\x81\xD1\x8F";

    // "робити", an entirely different word.
    const std::string Other = "\xD1\x80\xD0\xBE\xD0\xB1\xD0\xB8\xD1\x82\xD0\xB8";

    CHECK(CompareAnswer(Other, Stressed, Rules) == EFTAnswerGrade::Wrong);
}

TEST_CASE("A decomposed French accent is still an accent", "[answer]")
{
    // The same combining mark, on a Latin letter. Here it is part of the word, so requiring exact
    // accents has to keep telling "élève" from "eleve" whichever way the é was encoded.
    FTAnswerComparisonRules Rules;
    Rules.bRequireExactAccents = true;

    // "élève" with e + U+0301 / e + U+0300 rather than precomposed.
    const std::string Decomposed = "e\xCC\x81l\xC3\xA8ve";

    CHECK(CompareAnswer("eleve", Decomposed, Rules) != EFTAnswerGrade::Exact);
}

TEST_CASE("A translation is approximate when it is the opening words of the expected one", "[answercomparison][approximate]")
{
    CHECK(IsApproximateTranslation("look", "to look at"));
    CHECK(IsApproximateTranslation("look at", "to look at"));
    CHECK(IsApproximateTranslation("to look", "to look at"));
    CHECK(IsApproximateTranslation("take", "to take off, to remove"));
    CHECK(IsApproximateTranslation("remove", "to take off, to remove"));

    CHECK_FALSE(IsApproximateTranslation("at", "to look at"));
    CHECK_FALSE(IsApproximateTranslation("off", "to take off"));
    CHECK_FALSE(IsApproximateTranslation("a", "a lot"));
    CHECK_FALSE(IsApproximateTranslation("", "to look at"));
}

TEST_CASE("A translation is approximate with one letter wrong, missing or extra", "[answercomparison][approximate]")
{
    CHECK(IsApproximateTranslation("louk", "look"));
    CHECK(IsApproximateTranslation("looke", "look"));
    CHECK(IsApproximateTranslation("lok", "look"));
    CHECK(IsApproximateTranslation("LOUK", "look"));
    CHECK(IsApproximateTranslation("to louk", "to look"));
    CHECK(IsApproximateTranslation("louk", "to look at"));

    CHECK_FALSE(IsApproximateTranslation("loukk", "look"));
    CHECK_FALSE(IsApproximateTranslation("luuk", "look"));
    CHECK_FALSE(IsApproximateTranslation("lo", "look"));
}

TEST_CASE("One letter apart is not forgiven for a short word", "[answercomparison][approximate]")
{
    CHECK_FALSE(IsApproximateTranslation("car", "cat"));
    CHECK_FALSE(IsApproximateTranslation("go", "to"));
    CHECK_FALSE(IsApproximateTranslation("cap", "cat"));
}

TEST_CASE("A letter of a non-Latin translation counts as one letter", "[answercomparison][approximate]")
{
    CHECK(IsApproximateTranslation("\xD0\xBA\xD1\x96\xD0\xBC\xD0\xBD\xD0\xB0\xD1\x82\xD0\xB0",
                                   "\xD0\xBA\xD1\x96\xD0\xBC\xD0\xBD\xD0\xB0\xD1\x82\xD0\xB0"));
    CHECK(IsApproximateTranslation("\xD0\xBA\xD1\x96\xD0\xBC\xD0\xBD\xD0\xB0\xD1\x82\xD1\x83",
                                   "\xD0\xBA\xD1\x96\xD0\xBC\xD0\xBD\xD0\xB0\xD1\x82\xD0\xB0"));
    CHECK_FALSE(IsApproximateTranslation("\xD0\xBA\xD1\x96\xD0\xBC\xD0\xBD\xD1\x83\xD1\x82\xD1\x83",
                                         "\xD0\xBA\xD1\x96\xD0\xBC\xD0\xBD\xD0\xB0\xD1\x82\xD0\xB0"));
}

TEST_CASE("Only an alternative of the expected translations needs to be approximated", "[answercomparison][approximate]")
{
    CHECK(IsApproximateTranslation("weathr", "time, weather"));
    CHECK_FALSE(IsApproximateTranslation("whether", "time, weather"));
}

TEST_CASE("A hint in parentheses is not part of the answer", "[answer]")
{
    CHECK(CompareAgainstAny("real", "real (not imitation), genuine, legitimate") == EFTAnswerGrade::Exact);
    CHECK(CompareAgainstAny("real (not imitation)", "real (not imitation), genuine") == EFTAnswerGrade::Exact);
    CHECK(CompareAgainstAny("genuine", "real (not imitation), genuine") == EFTAnswerGrade::Exact);
    CHECK(CompareAgainstAny("go", "(informal) to go") == EFTAnswerGrade::Exact);
    CHECK(CompareAgainstAny("not imitation", "real (not imitation)") == EFTAnswerGrade::Wrong);
    CHECK(IsApproximateTranslation("rea", "real (not imitation)"));
}

TEST_CASE("A comma inside parentheses does not split a translation", "[answer]")
{
    const std::vector<std::string> Parts = SplitTranslations("real (not fake, not imitation), genuine");
    REQUIRE(Parts.size() == 2);
    CHECK(Parts[0] == "real (not fake, not imitation)");
    CHECK(Parts[1] == "genuine");
}
