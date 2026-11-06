#include <catch2/catch_test_macros.hpp>

#include "core/FTArticleRules.h"
#include "core/FTCard.h"
#include "core/FTTypes.h"

TEST_CASE("English verb headwords show the infinitive prefix once", "[types][englishverbs]")
{
    FTCard Card;
    Card.TargetWord = "learn";
    Card.PartOfSpeech = "verb";

    CHECK(GetDisplayWord(Card, "en") == "to learn");
    CHECK(Card.TargetWord == "learn");
    CHECK(GetDisplayWord(Card, "fr") == "learn");

    Card.TargetWord = "to teach";
    CHECK(GetDisplayWord(Card, "en") == "to teach");

    Card.TargetWord = "To run";
    CHECK(GetDisplayWord(Card, "en") == "To run");

    Card.TargetWord = "take off";
    CHECK(GetDisplayWord(Card, "en") == "to take off");

    Card.TargetWord.clear();
    CHECK(GetDisplayWord(Card, "en").empty());

    Card.TargetWord = "run";
    Card.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Card, "en") == "run");

    Card.PartOfSpeech.clear();
    CHECK(GetDisplayWord(Card, "en") == "run");
}

TEST_CASE("English modal verbs never receive an infinitive prefix", "[types][englishverbs]")
{
    for (const std::string Word : {"can", "could", "may", "might", "must", "shall", "should", "will", "would", "CAN", "Must"})
    {
        CAPTURE(Word);

        FTCard Card;
        Card.TargetWord = Word;
        Card.Translation = Word;
        Card.PartOfSpeech = "verb";

        CHECK(GetDisplayHeadword(Word, "verb", "en") == Word);
        CHECK(GetDisplayWord(Card, "en") == Word);
        CHECK(GetDisplayTranslation(Card, "en") == Word);
    }
}

TEST_CASE("English translations prefix ordinary verbs alongside modal verbs", "[types][englishverbs]")
{
    FTCard Card;
    Card.PartOfSpeech = "verb";
    Card.Translation = " can, learn, Must, to teach, would ";

    CHECK(GetDisplayTranslation(Card, "en") == "can, to learn, Must, to teach, would");

    Card.Translation = "can do, willpower, muster";
    CHECK(GetDisplayTranslation(Card, "en") == "to can do, to willpower, to muster");

    Card.PartOfSpeech = "noun";
    Card.bSpellingIsAlsoAVerb = true;
    Card.Translation = "can, will";
    CHECK(GetDisplayTranslation(Card, "en") == "the can, the will");
}

TEST_CASE("A new card carries no assumptions", "[types]")
{
    const FTCard Fresh;

    CHECK(Fresh.TargetWord.empty());
    CHECK(Fresh.Translation.empty());
    CHECK(Fresh.Definition.empty());

    // A new card is in the pool, which is not a box and has no due date.
    CHECK(Fresh.LeitnerStage == PoolStage);
    CHECK(IsInPool(Fresh));
    CHECK_FALSE(Fresh.DueDate.has_value());
    CHECK_FALSE(Fresh.BoxChangedDate.has_value());
    CHECK_FALSE(IsCompleted(Fresh));

    // Nothing overridden, so a dictionary import may fill every field.
    CHECK_FALSE(IsUserEdited(Fresh, EFTCardField::Translation));
    CHECK_FALSE(IsUserEdited(Fresh, EFTCardField::Definition));
}

// The test that checked bPassed and FailureCount is gone with the per-exercise progress type it
// covered. Progress is now one Leitner stage on the card rather than a pass flag per exercise.

TEST_CASE("A verb's translation is shown as an infinitive", "[types]")
{
    FTCard Card;
    Card.TargetWord = "aimer";
    Card.Translation = "like";
    Card.PartOfSpeech = "verb";

    CHECK(GetDisplayTranslation(Card, "en") == "to like");
}

TEST_CASE("A noun takes the only where its spelling is also a verb", "[types]")
{
    // "besoin" is a noun and a verb, so a bare "need" does not say which is being asked for.
    FTCard Ambiguous;
    Ambiguous.TargetWord = "besoin";
    Ambiguous.Translation = "need";
    Ambiguous.PartOfSpeech = "noun";
    Ambiguous.bSpellingIsAlsoAVerb = true;

    CHECK(GetDisplayTranslation(Ambiguous, "en") == "the need");

    // "œil" is only ever a noun, and "the eye" would be clutter.
    FTCard Plain;
    Plain.TargetWord = "\xC5\x93il";
    Plain.Translation = "eye";
    Plain.PartOfSpeech = "noun";

    CHECK(GetDisplayTranslation(Plain, "en") == "eye");
}

TEST_CASE("Every item of a translation list takes the prefix", "[types]")
{
    FTCard Verb;
    Verb.TargetWord = "prendre";
    Verb.Translation = "take, make,to grab";
    Verb.PartOfSpeech = "verb";

    CHECK(GetDisplayTranslation(Verb, "en") == "to take, to make, to grab");

    FTCard Ambiguous;
    Ambiguous.TargetWord = "besoin";
    Ambiguous.Translation = "need, want";
    Ambiguous.PartOfSpeech = "noun";
    Ambiguous.bSpellingIsAlsoAVerb = true;

    CHECK(GetDisplayTranslation(Ambiguous, "en") == "the need, the want");
}

TEST_CASE("The English prefix never lands on a non-English translation", "[types]")
{
    FTCard Verb;
    Verb.TargetWord = "aimer";
    Verb.Translation = "\xD0\xBB\xD1\x8E\xD0\xB1\xD0\xB8\xD1\x82\xD0\xB8";
    Verb.PartOfSpeech = "verb";

    CHECK(GetDisplayTranslation(Verb, "uk") == Verb.Translation);

    FTCard Ambiguous;
    Ambiguous.TargetWord = "besoin";
    Ambiguous.Translation = "\xD0\xBF\xD0\xBE\xD1\x82\xD1\x80\xD0\xB5\xD0\xB1\xD0\xB0";
    Ambiguous.PartOfSpeech = "noun";
    Ambiguous.bSpellingIsAlsoAVerb = true;

    CHECK(GetDisplayTranslation(Ambiguous, "uk") == Ambiguous.Translation);
}

TEST_CASE("A card with no part of speech gets no prefix", "[types]")
{
    // What a hand-typed card looks like, and what every card looks like with no dictionary open.
    FTCard Card;
    Card.TargetWord = "besoin";
    Card.Translation = "need";

    CHECK(GetDisplayTranslation(Card, "en") == "need");
}

TEST_CASE("A card with no gender displays its bare target word", "[types]")
{
    FTCard Card;
    Card.TargetWord = "chien";

    CHECK(GetDisplayWord(Card, "fr") == "chien");
}

TEST_CASE("A masculine French card is prefixed with le", "[types]")
{
    FTCard Card;
    Card.TargetWord = "chien";
    Card.Gender = GenderMasculine;
    Card.PartOfSpeech = "noun";

    CHECK(GetDisplayWord(Card, "fr") == "le chien");
}

TEST_CASE("A feminine French card is prefixed with la", "[types]")
{
    FTCard Card;
    Card.TargetWord = "maison";
    Card.Gender = GenderFeminine;
    Card.PartOfSpeech = "noun";

    CHECK(GetDisplayWord(Card, "fr") == "la maison");
}

TEST_CASE("A gendered French card starting with a vowel sound elides to l'", "[types]")
{
    FTCard Masculine;
    Masculine.TargetWord = "eau";
    Masculine.Gender = GenderFeminine;
    Masculine.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Masculine, "fr") == "l'eau");

    FTCard Accented;
    Accented.TargetWord = "école";
    Accented.Gender = GenderFeminine;
    Accented.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Accented, "fr") == "l'école");
}

TEST_CASE("A French card the dictionary marks as eliding takes l'", "[types]")
{
    // French h is silent whether or not it blocks elision, so the spelling cannot separate these
    // two and only the dictionary's own marking does.
    FTCard Heure;
    Heure.TargetWord = "heure";
    Heure.Gender = GenderFeminine;
    Heure.PartOfSpeech = "noun";
    Heure.bElidesArticle = true;
    CHECK(GetDisplayWord(Heure, "fr") == "l'heure");

    FTCard Homme;
    Homme.TargetWord = "homme";
    Homme.Gender = GenderMasculine;
    Homme.PartOfSpeech = "noun";
    Homme.bElidesArticle = true;
    CHECK(GetDisplayWord(Homme, "fr") == "l'homme");
}

TEST_CASE("A French card the dictionary marks as keeping its article does not elide", "[types]")
{
    // The flag is off for these because the dictionary marked them, which is the whole point of
    // marking the exception rather than the rule.
    FTCard Hasard;
    Hasard.TargetWord = "hasard";
    Hasard.Gender = GenderMasculine;
    Hasard.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Hasard, "fr") == "le hasard");

    FTCard Haine;
    Haine.TargetWord = "haine";
    Haine.Gender = GenderFeminine;
    Haine.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Haine, "fr") == "la haine");
}

TEST_CASE("An unmarked French h word elides, since most of them do", "[types]")
{
    // Nothing in either Wiktionary edition classifies "hiver", and four in five unclassified h
    // words contract, so contracting is the reading that is right more often.
    FTCard Hiver;
    Hiver.TargetWord = "hiver";
    Hiver.Gender = GenderMasculine;
    Hiver.PartOfSpeech = "noun";
    Hiver.bElidesArticle = true;
    CHECK(GetDisplayWord(Hiver, "fr") == "l'hiver");
}

TEST_CASE("A French consonant never elides, whatever the dictionary says", "[types]")
{
    // The flag is only ever set for an h. A consonant reaching this with it set would read
    // "l'chien", which is why the lookup refuses to answer for one.
    FTCard Chien;
    Chien.TargetWord = "chien";
    Chien.Gender = GenderMasculine;
    Chien.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Chien, "fr") == "le chien");
}

TEST_CASE("A French vowel still elides with nothing from the dictionary", "[types]")
{
    FTCard Ecole;
    Ecole.TargetWord = "école";
    Ecole.Gender = GenderFeminine;
    Ecole.PartOfSpeech = "noun";
    CHECK_FALSE(Ecole.bElidesArticle);
    CHECK(GetDisplayWord(Ecole, "fr") == "l'école");
}

TEST_CASE("A gendered Spanish card takes el or la, never an elided form", "[types]")
{
    FTCard Masculine;
    Masculine.TargetWord = "perro";
    Masculine.Gender = GenderMasculine;
    Masculine.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Masculine, "es") == "el perro");

    FTCard Feminine;
    Feminine.TargetWord = "casa";
    Feminine.Gender = GenderFeminine;
    Feminine.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Feminine, "es") == "la casa");
}

TEST_CASE("A feminine Spanish word starting with a stressed a takes el, not la", "[types]")
{
    FTCard Agua;
    Agua.TargetWord = "agua";
    Agua.Gender = GenderFeminine;
    Agua.PartOfSpeech = "noun";
    Agua.bStressedFirstSyllable = true;
    CHECK(GetDisplayWord(Agua, "es") == "el agua");

    FTCard Hacha;
    Hacha.TargetWord = "hacha";
    Hacha.Gender = GenderFeminine;
    Hacha.PartOfSpeech = "noun";
    Hacha.bStressedFirstSyllable = true;
    CHECK(GetDisplayWord(Hacha, "es") == "el hacha");

    // Masculine words starting the same way are unaffected: the exception only ever swaps la for
    // el, never touches a word already taking el.
    FTCard Masculine;
    Masculine.TargetWord = "actor";
    Masculine.Gender = GenderMasculine;
    Masculine.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Masculine, "es") == "el actor");
}

TEST_CASE("A feminine Spanish word whose a is unstressed keeps la", "[types]")
{
    // The exception is about the sound, not the spelling: "abeja" is a-BE-ja, so the article's
    // own a never meets a stressed one and there is nothing to avoid.
    FTCard Abeja;
    Abeja.TargetWord = "abeja";
    Abeja.Gender = GenderFeminine;
    Abeja.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Abeja, "es") == "la abeja");

    FTCard Arena;
    Arena.TargetWord = "arena";
    Arena.Gender = GenderFeminine;
    Arena.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Arena, "es") == "la arena");
}

TEST_CASE("Primary stress on the first syllable is read from the phonetic", "[types]")
{
    CHECK(IsFirstSyllableStressed("/ˈaɡwa/"));
    CHECK(IsFirstSyllableStressed("/ˈat͡ʃa/"));
    CHECK(IsFirstSyllableStressed("/ˈambɾe/"));

    CHECK_FALSE(IsFirstSyllableStressed("/aˈbexa/"));
    CHECK_FALSE(IsFirstSyllableStressed("/aˈɾena/"));
    CHECK_FALSE(IsFirstSyllableStressed("/isˈtoɾja/"));

    // Nothing to read is not the same as unstressed, and both answer no rather than guessing.
    CHECK_FALSE(IsFirstSyllableStressed(""));
    CHECK_FALSE(IsFirstSyllableStressed("/aɡwa/"));
}

TEST_CASE("A card in a language with no article rule shows the bare word", "[types]")
{
    FTCard Card;
    Card.TargetWord = "dog";
    Card.Gender = GenderMasculine;

    CHECK(GetDisplayWord(Card, "en") == "dog");
}

TEST_CASE("A gendered Portuguese card takes o or a", "[types]")
{
    FTCard Masculine;
    Masculine.TargetWord = "livro";
    Masculine.Gender = GenderMasculine;
    Masculine.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Masculine, "pt") == "o livro");

    FTCard Feminine;
    Feminine.TargetWord = "água";
    Feminine.Gender = GenderFeminine;
    Feminine.PartOfSpeech = "noun";
    CHECK(GetDisplayWord(Feminine, "pt") == "a água");
}

TEST_CASE("A gendered Italian card takes il, lo, la or l' by its first letters", "[types]")
{
    const auto Display = [](const char* Word, int Gender)
    {
        FTCard Card;
        Card.TargetWord = Word;
        Card.Gender = Gender;
        Card.PartOfSpeech = "noun";
        return GetDisplayWord(Card, "it");
    };

    CHECK(Display("libro", GenderMasculine) == "il libro");
    CHECK(Display("studente", GenderMasculine) == "lo studente");
    CHECK(Display("zaino", GenderMasculine) == "lo zaino");
    CHECK(Display("gnomo", GenderMasculine) == "lo gnomo");
    CHECK(Display("psicologo", GenderMasculine) == "lo psicologo");
    CHECK(Display("yogurt", GenderMasculine) == "lo yogurt");
    CHECK(Display("iato", GenderMasculine) == "lo iato");
    CHECK(Display("sole", GenderMasculine) == "il sole");
    CHECK(Display("amico", GenderMasculine) == "l'amico");
    CHECK(Display("hotel", GenderMasculine) == "l'hotel");
    CHECK(Display("casa", GenderFeminine) == "la casa");
    CHECK(Display("isola", GenderFeminine) == "l'isola");
    CHECK(Display("iena", GenderFeminine) == "la iena");
    CHECK(Display("cantante", GenderCommon) == "il cantante");
}
