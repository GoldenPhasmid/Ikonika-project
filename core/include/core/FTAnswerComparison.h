#pragma once

#include <string>
#include <string_view>
#include <vector>

enum class EFTAnswerGrade
{
    Exact,

    /** Right word, article left out. "chien" for "le chien". */
    ArticleOmitted,

    AccentsWrong,

    Wrong,

    /** Declined rather than answered. Nothing was typed, so nothing was compared. */
    Skipped
};

struct FTAnswerComparisonRules
{
    bool bIgnoreCase = true;
    bool bIgnoreWhitespace = true;

    /** An extension of ignoring whitespace: "peut être" is accepted for "peut-être". */
    bool bIgnoreHyphens = true;

    bool bAllowMissingArticle = false;

    bool bRequireExactAccents = false;
};

/** Everything except Wrong and Skipped scores as a correct answer. */
bool IsCorrectGrade(EFTAnswerGrade Grade);

/** Never returns Skipped. That grade comes from the user declining, not from a comparison. */
EFTAnswerGrade CompareAnswer(std::string_view Given,
                             std::string_view Expected,
                             const FTAnswerComparisonRules& Rules = {},
                             std::string_view ExpectedWithoutArticle = {});

std::string NormalizeAnswer(std::string_view Text, const FTAnswerComparisonRules& Rules = {});

/**
 * The translation without the reading aid the interface puts in front of it: "to need" and "the
 * need" both become "need".
 *
 * The English counterpart of the target article, and used the same way: the prefix marks which
 * word class is being asked for, so typing it is welcome and leaving it out is not a mistake.
 * Unlike the French article it is never part of the stored word, so both spellings grade Exact.
 */
std::string StripLeadingParticle(std::string_view Text);

/**
 * A card's translations, split on commas and trimmed. One entry for a card holding a single word.
 *
 * A card can teach more than one translation - "temps" is time and weather - and the learner picks
 * which ones when the card is made. Typing any one of them is the whole answer: reciting all of a
 * word's meanings is a different exercise from knowing what it means.
 */
std::vector<std::string> SplitTranslations(std::string_view Text);

/**
 * The best grade any one of the expected translations earns.
 *
 * Best rather than first, so a card whose second translation is typed exactly is not marked down
 * because the first one was nearly missed.
 */
EFTAnswerGrade CompareAgainstAny(std::string_view Given,
                                 std::string_view ExpectedTranslations,
                                 const FTAnswerComparisonRules& Rules = {},
                                 std::string_view ExpectedWithoutArticle = {});

bool IsApproximateTranslation(std::string_view Given,
                              std::string_view ExpectedTranslations,
                              const FTAnswerComparisonRules& Rules = {});
