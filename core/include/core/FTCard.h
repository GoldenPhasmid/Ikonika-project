#pragma once

#include "core/FTTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class EFTCardField : uint32_t
{
    None        = 0,
    Translation = 1u << 0,
    Definition  = 1u << 1,

    /** Set when the user chose the gender themselves. The dictionary is wrong about a handful of
        nouns, and a card carries whatever it was given when it was made, so a later dictionary
        that knows better cannot reach a card already on the ladder. This marks the ones a person
        has corrected, so nothing later overwrites them. */
    Gender       = 1u << 2,
    PartOfSpeech = 1u << 3
};

constexpr int PoolStage = 0;
constexpr int FirstLadderStage = 1;

constexpr int GenderMasculine = 0;
constexpr int GenderFeminine = 1;
constexpr int GenderNeuter = 3;

/**
 * One spelling that takes either gender.
 *
 * Two kinds of word arrive here and both are real: one whose gender follows the person it names,
 * so "el soldado" and "la soldado" are the same word about two people; and one that simply takes
 * either article with no change of meaning, like "el mar" and "la mar". Distinct from no gender at
 * all, which is a word nothing has said anything about.
 *
 * Shown as an article, this is the masculine: it is the citation form, it is never wrong, and a
 * learner typing the feminine is not what the exercises are asking about.
 */
constexpr int GenderCommon = 2;

/** How long a word or a translation is allowed to be, in characters rather than bytes. Long
    enough for a noun with its article and a translation carrying two or three alternatives,
    short enough that a pasted sentence cannot become a card. Deliberately not a setting: it
    bounds what the exercises can lay out, which is not the user's decision to make. */
constexpr int MaximumCardFieldLength = 70;

struct FTCard
{
    std::string Id;

    std::string TargetWord;
    std::string Translation;
    std::string Definition;
    std::string Hint;

    /** 0 = masculine, 1 = feminine, unset for a card with no gender (not a noun, or gender was
        never supplied). TargetWord itself never carries the article; GetDisplayWord adds it. */
    std::optional<int> Gender;

    /** "noun", "verb", "adj", "adv", or empty for a card typed in by hand. What decides whether
        the translation is shown as "to need" or "need"; Translation itself never carries the
        prefix, the same way TargetWord never carries its article. */
    std::string PartOfSpeech;

    std::string MeaningGist;

    /**
     * Whether this spelling is also a verb in the dictionary. Runtime only: never stored, never in
     * a column list, filled in as the card is loaded.
     *
     * It is a fact about the headword rather than about this reading, so the card cannot answer it
     * alone and the dictionary is asked. Not stored because a dictionary rebuild would leave the
     * answer stale on every card; false is the honest reading when no dictionary is open, and
     * costs only the prefix.
     */
    bool bSpellingIsAlsoAVerb = false;

    /**
     * Whether the word's first syllable carries the primary stress. Runtime only: never stored,
     * never in a column list, filled in as the card is loaded.
     *
     * Spanish needs it to choose between "la" and "el" for a feminine noun, and spelling cannot
     * answer it: Spanish writes an accent only where its default stress rule fails, so "agua" and
     * "abeja" look alike and are stressed differently. The dictionary's own phonetic decides,
     * which is why this is filled from there rather than computed here.
     */
    bool bStressedFirstSyllable = false;

    /**
     * Whether the article elides before this word on the dictionary's say-so. Runtime only: never
     * stored, never in a column list, filled in as the card is loaded.
     *
     * French needs it because its h is silent whether or not it blocks elision, so neither the
     * spelling nor the phonetic separates "l'heure" from "le hasard". Only the dictionary's own
     * classification does, which is why this is filled from there. False is the honest reading
     * when nothing is known: the spelling rule still elides before a vowel, and a word starting
     * with h keeps its full article, which is what the app did before this existed.
     */
    bool bElidesArticle = false;

    std::vector<std::string> AlternativeSpellings;

    uint32_t UserEditedFields = 0;

    /** 0 is the pool, 1 upward is the ladder. */
    int LeitnerStage = PoolStage;

    /** Empty while the card is in the pool, since a pool card is not waiting for a date. */
    std::optional<FTDayNumber> DueDate;

    std::optional<FTDayNumber> BoxChangedDate;

    /** Set once a card leaves the last stage, and the card is then out of the schedule. */
    std::optional<FTDayNumber> CompletedDate;

    FTTimestamp CreatedDate = 0;
    FTTimestamp LastUpdatedDate = 0;
};

/** Whether the user wrote this field rather than taking the dictionary's value. */
bool IsUserEdited(const FTCard& Subject, EFTCardField Field);

void SetUserEdited(FTCard& Subject, EFTCardField Field, bool bEdited);

/** In the pool, so not scheduled. Distinct from completed, which is also not scheduled. */
bool IsInPool(const FTCard& Subject);

bool IsCompleted(const FTCard& Subject);

/**
 * TargetWord with its definite article prepended when Gender is set; TargetWord unchanged when
 * Gender is unset. What every exercise shows, speaks and compares a typed answer against — the
 * article is part of the word being learned, not decoration added for display.
 *
 * The article rule depends on TargetLanguage (ISO 639-1, lowercase):
 * - French ("fr"): "le"/"la", eliding to "l'" before a vowel sound ("eau" -> "l'eau").
 * - Spanish ("es"): "el"/"la", except a feminine word takes "el" instead of "la" when it opens
 *   with a stressed "a"/"ha" sound ("agua" -> "el agua") for pronunciation only — it stays
 *   feminine, so its plural is still "las aguas".
 * - Portuguese ("pt"): "o"/"a".
 * - Italian ("it"): "il"/"la", "lo" for a masculine word opening with s and a consonant, z, gn,
 *   ps, pn, x, y or a semivowel i ("lo studente", "lo iato"), and "l'" before a vowel or h for
 *   either gender ("l'amico", "l'isola").
 * - Any other language: no article, same as an unset Gender.
 */
std::string GetDisplayWord(const FTCard& Subject, const std::string& TargetLanguage);
std::string GetDisplayHeadword(const std::string& Headword, const std::string& PartOfSpeech,
                               const std::string& TargetLanguage);

/**
 * Translation with the word class written into it: "to need" for a verb, "the need" for a noun
 * whose spelling is also a verb, and the stored text unchanged for everything else. A
 * comma-separated list gets the prefix on each item: "to take, to make".
 *
 * The same idea as the article on the target side. "besoin" and "besoiner" are two different words
 * a learner has to keep apart, and a bare "need" on both cards does not say which one is being
 * asked for. A noun with no verb to be confused with needs no such help: "eye" is only ever a noun
 * here, and "the eye" would be clutter.
 *
 * Never stored. Both this and the bare translation are accepted when an answer is typed, since the
 * prefix is a reading aid rather than part of the word.
 */
std::string GetDisplayTranslation(const FTCard& Subject, const std::string& NativeLanguage);

/** The grammatical gender as a learner writes it: "m", "f", or "m/f" for a spelling that takes
    either. Empty when nothing has said anything about the word's gender. Takes the optional
    rather than a card, since a dictionary entry answers the same question without being one. */
std::string GetGenderCode(const std::optional<int>& Gender);
