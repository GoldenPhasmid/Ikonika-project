#include "core/FTAnswerComparison.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace
{
    // C3 leads the Latin-1 supplement, which is where every accented letter French uses
    // lives. C5 leads the block containing the Œ ligature.
    constexpr unsigned char LatinLeadByte = 0xC3;
    constexpr unsigned char LigatureLeadByte = 0xC5;

    constexpr unsigned char UppercaseOeSecondByte = 0x92;
    constexpr unsigned char LowercaseOeSecondByte = 0x93;

    /** Byte length of the UTF-8 sequence starting at this lead byte. */
    size_t SequenceLength(unsigned char LeadByte)
    {
        if (LeadByte < 0x80)
        {
            return 1;
        }

        if ((LeadByte & 0xE0) == 0xC0)
        {
            return 2;
        }

        if ((LeadByte & 0xF0) == 0xE0)
        {
            return 3;
        }

        if ((LeadByte & 0xF8) == 0xF0)
        {
            return 4;
        }

        // Not a valid lead byte. Pass it through rather than guessing at a length.
        return 1;
    }

    bool IsIgnorableSpace(std::string_view Sequence)
    {
        if (Sequence.size() == 1)
        {
            const char Byte = Sequence[0];

            return Byte == ' ' || Byte == '\t' || Byte == '\n'
                || Byte == '\r' || Byte == '\v' || Byte == '\f';
        }

        // U+00A0, the non-breaking space.
        return Sequence == "\xC2\xA0";
    }

    /** Apostrophe forms a keyboard or word processor might produce instead of ASCII '. */
    bool IsApostrophe(std::string_view Sequence)
    {
        return Sequence == "\xE2\x80\x99"  // U+2019 right single quotation mark
            || Sequence == "\xE2\x80\x98"  // U+2018 left single quotation mark
            || Sequence == "\xCA\xBC";     // U+02BC modifier letter apostrophe
    }

    /**
     * The combining acute and grave, U+0301 and U+0300.
     *
     * Ukrainian writes stress as an acute over the vowel: "диви́тися" and "дивитися" are one word,
     * not two. The mark tells a reader which syllable to lean on, which is a note about the word
     * rather than part of it, and no keyboard offers it.
     */
    bool IsCombiningStressMark(std::string_view Sequence)
    {
        return Sequence == "\xCC\x81" || Sequence == "\xCC\x80";
    }

    /** U+0400 to U+04FF, the Cyrillic block. */
    bool IsCyrillic(std::string_view Sequence)
    {
        if (Sequence.size() != 2)
        {
            return false;
        }

        const unsigned char LeadByte = static_cast<unsigned char>(Sequence[0]);

        return LeadByte >= 0xD0 && LeadByte <= 0xD3;
    }

    /** The unaccented form of a letter, or empty if there is nothing to fold. */
    std::string_view FoldedAccent(std::string_view Sequence)
    {
        if (Sequence.size() != 2)
        {
            return {};
        }

        const unsigned char LeadByte = static_cast<unsigned char>(Sequence[0]);
        const unsigned char SecondByte = static_cast<unsigned char>(Sequence[1]);

        if (LeadByte == LigatureLeadByte && SecondByte == LowercaseOeSecondByte)
        {
            return "oe";
        }

        if (LeadByte != LatinLeadByte)
        {
            return {};
        }

        switch (SecondByte)
        {
            case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5:
                return "a";
            case 0xA6:
                return "ae";
            case 0xA7:
                return "c";
            case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                return "e";
            case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                return "i";
            case 0xB1:
                return "n";
            case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
                return "o";
            case 0xB9: case 0xBA: case 0xBB: case 0xBC:
                return "u";
            case 0xBD: case 0xBF:
                return "y";
            default:
                return {};
        }
    }

    bool IsPunctuation(std::string_view Sequence)
    {
        if (Sequence.size() == 1)
        {
            const char Byte = Sequence[0];

            return Byte == '.' || Byte == ',' || Byte == '!' || Byte == '?'
                || Byte == ';' || Byte == ':' || Byte == '"';
        }

        return Sequence == "\xC2\xA1" || Sequence == "\xC2\xBF"
            || Sequence == "\xC2\xAB" || Sequence == "\xC2\xBB"
            || Sequence == "\xE2\x80\x9C" || Sequence == "\xE2\x80\x9D" || Sequence == "\xE2\x80\x9E"
            || Sequence == "\xE2\x80\xA6";
    }

    std::string StripHints(std::string_view Text)
    {
        if (Text.find('(') == std::string_view::npos)
        {
            return std::string(Text);
        }

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

            if (Character == ')' && Depth > 0)
            {
                --Depth;
                continue;
            }

            if (Depth > 0)
            {
                continue;
            }

            if (Character == ' ' && (Result.empty() || Result.back() == ' '))
            {
                continue;
            }

            Result.push_back(Character);
        }

        while (!Result.empty() && Result.back() == ' ')
        {
            Result.pop_back();
        }

        return Result.empty() ? std::string(Text) : Result;
    }

    size_t FindTranslationSeparator(std::string_view Text, size_t Start)
    {
        int Depth = 0;

        for (size_t Index = Start; Index < Text.size(); ++Index)
        {
            if (Text[Index] == '(')
            {
                ++Depth;
            }
            else if (Text[Index] == ')' && Depth > 0)
            {
                --Depth;
            }
            else if (Text[Index] == ',' && Depth == 0)
            {
                return Index;
            }
        }

        return std::string_view::npos;
    }

    std::string Prepare(std::string_view Original, bool bIgnoreCase)
    {
        const std::string Hintless = StripHints(Original);
        const std::string_view Text = Hintless;

        std::string Result;
        Result.reserve(Text.size());

        bool bPreviousWasCyrillic = false;

        for (size_t Index = 0; Index < Text.size();)
        {
            const unsigned char LeadByte = static_cast<unsigned char>(Text[Index]);
            const size_t Length = (std::min)(SequenceLength(LeadByte), Text.size() - Index);
            const std::string_view Sequence = Text.substr(Index, Length);

            Index += Length;

            // Dropped whatever the accent setting says, because this is not that kind of accent:
            // it marks stress rather than spelling. Only after a Cyrillic letter, since the same
            // combining mark is how a decomposed French é is written, and there it is the word.
            if (bPreviousWasCyrillic && IsCombiningStressMark(Sequence))
            {
                continue;
            }

            bPreviousWasCyrillic = IsCyrillic(Sequence);

            if (IsPunctuation(Sequence))
            {
                continue;
            }

            if (IsApostrophe(Sequence))
            {
                Result.push_back('\'');
                continue;
            }

            if (!bIgnoreCase)
            {
                Result.append(Sequence);
                continue;
            }

            if (Length == 1 && LeadByte < 0x80)
            {
                Result.push_back(static_cast<char>(std::tolower(LeadByte)));
                continue;
            }

            if (Length == 2)
            {
                const unsigned char SecondByte = static_cast<unsigned char>(Sequence[1]);

                const bool bIsAccentedCapital = LeadByte == LatinLeadByte
                    && SecondByte >= 0x80 && SecondByte <= 0x9E && SecondByte != 0x97;

                if (bIsAccentedCapital)
                {
                    Result.push_back(static_cast<char>(LeadByte));
                    Result.push_back(static_cast<char>(SecondByte + 0x20));
                    continue;
                }

                if (LeadByte == LigatureLeadByte && SecondByte == UppercaseOeSecondByte)
                {
                    Result.push_back(static_cast<char>(LigatureLeadByte));
                    Result.push_back(static_cast<char>(LowercaseOeSecondByte));
                    continue;
                }
            }

            Result.append(Sequence);
        }

        return Result;
    }

    /**
     * The two-letter spelling of a ligature, or empty when there is nothing to expand.
     *
     * These two only. A keyboard with no key for them is not the learner's spelling mistake, and
     * "oe" for "œ" is unambiguous in a way that "e" for "é" is not: dropping an accent changes the
     * word, so accents stay required.
     */
    std::string_view ExpandedLigature(std::string_view Sequence)
    {
        if (Sequence.size() != 2)
        {
            return {};
        }

        const unsigned char LeadByte = static_cast<unsigned char>(Sequence[0]);
        const unsigned char SecondByte = static_cast<unsigned char>(Sequence[1]);

        if (LeadByte == LigatureLeadByte
            && (SecondByte == LowercaseOeSecondByte || SecondByte == UppercaseOeSecondByte))
        {
            return "oe";
        }

        // U+00E6 and U+00C6, the two halves of the ae ligature's case pair.
        if (LeadByte == LatinLeadByte && (SecondByte == 0xA6 || SecondByte == 0x86))
        {
            return "ae";
        }

        return {};
    }

    /**
     * Every ligature written out in full.
     *
     * Applied to both sides of a comparison and to neither side of anything shown to the learner:
     * a card that spells the word "cœur" goes on saying so, and the correction the interface
     * displays is the real spelling rather than the one that was easy to type.
     */
    std::string ExpandLigatures(std::string_view Text)
    {
        std::string Result;
        Result.reserve(Text.size());

        for (size_t Index = 0; Index < Text.size();)
        {
            const unsigned char LeadByte = static_cast<unsigned char>(Text[Index]);
            const size_t Length = (std::min)(SequenceLength(LeadByte), Text.size() - Index);
            const std::string_view Sequence = Text.substr(Index, Length);

            Index += Length;

            const std::string_view Expanded = ExpandedLigature(Sequence);

            Result.append(Expanded.empty() ? Sequence : Expanded);
        }

        return Result;
    }

    /** Drops whitespace and hyphens per the rules. Accents are handled separately. */
    std::string StripSpacing(std::string_view Text, const FTAnswerComparisonRules& Rules)
    {
        std::string Result;
        Result.reserve(Text.size());

        for (size_t Index = 0; Index < Text.size();)
        {
            const unsigned char LeadByte = static_cast<unsigned char>(Text[Index]);
            const size_t Length = (std::min)(SequenceLength(LeadByte), Text.size() - Index);
            const std::string_view Sequence = Text.substr(Index, Length);

            Index += Length;

            if (Rules.bIgnoreWhitespace && IsIgnorableSpace(Sequence))
            {
                continue;
            }

            if (Rules.bIgnoreHyphens && Sequence == "-")
            {
                continue;
            }

            Result.append(Sequence);
        }

        return Result;
    }

    std::string FoldAccents(std::string_view Text)
    {
        std::string Result;
        Result.reserve(Text.size());

        for (size_t Index = 0; Index < Text.size();)
        {
            const unsigned char LeadByte = static_cast<unsigned char>(Text[Index]);
            const size_t Length = (std::min)(SequenceLength(LeadByte), Text.size() - Index);
            const std::string_view Sequence = Text.substr(Index, Length);

            Index += Length;

            const std::string_view Folded = FoldedAccent(Sequence);
            Result.append(Folded.empty() ? Sequence : Folded);
        }

        return Result;
    }
}

bool IsCorrectGrade(EFTAnswerGrade Grade)
{
    return Grade != EFTAnswerGrade::Wrong && Grade != EFTAnswerGrade::Skipped;
}

std::string StripLeadingParticle(std::string_view Text)
{
    static constexpr std::array<std::string_view, 2> Particles{"to ", "the "};

    for (const std::string_view Particle : Particles)
    {
        if (Text.size() > Particle.size() && Text.compare(0, Particle.size(), Particle) == 0)
        {
            return std::string(Text.substr(Particle.size()));
        }
    }

    return std::string(Text);
}

std::vector<std::string> SplitTranslations(std::string_view Text)
{
    std::vector<std::string> Parts;

    size_t Start = 0;

    while (Start <= Text.size())
    {
        const size_t Comma = FindTranslationSeparator(Text, Start);
        const size_t End = Comma == std::string_view::npos ? Text.size() : Comma;

        std::string_view Part = Text.substr(Start, End - Start);

        while (!Part.empty() && (Part.front() == ' ' || Part.front() == '\t'))
        {
            Part.remove_prefix(1);
        }

        while (!Part.empty() && (Part.back() == ' ' || Part.back() == '\t'))
        {
            Part.remove_suffix(1);
        }

        if (!Part.empty())
        {
            Parts.emplace_back(Part);
        }

        if (Comma == std::string_view::npos)
        {
            break;
        }

        Start = Comma + 1;
    }

    // A card holding nothing but separators still has to be compared against something, and the
    // text as given is the least surprising answer.
    if (Parts.empty() && !Text.empty())
    {
        Parts.emplace_back(Text);
    }

    return Parts;
}

/**
 * A comma-separated list as a sorted, normalised multiset, so two lists can be compared without
 * caring about order.
 *
 * Each word is normalised the way CompareAnswer normalises a single answer, and its reading
 * prefix ("to ", "the ") comes off individually: "to take, to make" and "to take, make" are the
 * same answer whichever of the two the card was showing.
 */
std::vector<std::string> NormaliseTokenList(std::string_view Text,
                                            const FTAnswerComparisonRules& Rules,
                                            bool bFoldAccents)
{
    std::vector<std::string> Result;

    for (const std::string& Token : SplitTranslations(Text))
    {
        std::string Normalised = ExpandLigatures(StripSpacing(
            StripLeadingParticle(Prepare(Token, Rules.bIgnoreCase)), Rules));

        if (bFoldAccents)
        {
            Normalised = FoldAccents(Normalised);
        }

        Result.push_back(std::move(Normalised));
    }

    std::sort(Result.begin(), Result.end());

    return Result;
}

EFTAnswerGrade CompareAgainstAny(std::string_view Given,
                                 std::string_view ExpectedTranslations,
                                 const FTAnswerComparisonRules& Rules,
                                 std::string_view ExpectedWithoutArticle)
{
    const std::vector<std::string> Expected = SplitTranslations(ExpectedTranslations);

    if (Expected.empty())
    {
        return CompareAnswer(Given, ExpectedTranslations, Rules, ExpectedWithoutArticle);
    }

    // Ordered worst to best, so "better than" is a comparison rather than a table of special
    // cases. Skipped never comes out of a comparison, and sits with Wrong.
    const auto Rank = [](EFTAnswerGrade Grade)
    {
        switch (Grade)
        {
        case EFTAnswerGrade::Exact:          return 4;
        case EFTAnswerGrade::ArticleOmitted: return 3;
        case EFTAnswerGrade::AccentsWrong:   return 2;
        default:                             return 1;
        }
    };

    // The whole comma-joined string is what the interface actually shows as the card's
    // translation, so typing it back verbatim is accepted too - not only one alternative from
    // it. Tried first: it is the more literal answer, and the split alternatives below would
    // otherwise never let it win once one of them scores Exact and the loop stops early.
    EFTAnswerGrade Best = CompareAnswer(Given, ExpectedTranslations, Rules, ExpectedWithoutArticle);

    // The same words in a different order are the same answer, so a list answered back in any
    // order is a full match. Only when both sides hold more than one word: a single word is
    // already the whole answer, and the per-entry loop below keeps that behaviour.
    if (Expected.size() > 1 && Given.find(',') != std::string_view::npos)
    {
        const std::vector<std::string> GivenTokens =
            NormaliseTokenList(Given, Rules, false);

        if (GivenTokens == NormaliseTokenList(ExpectedTranslations, Rules, false))
        {
            return EFTAnswerGrade::Exact;
        }

        if (!Rules.bRequireExactAccents
            && NormaliseTokenList(Given, Rules, true)
                == NormaliseTokenList(ExpectedTranslations, Rules, true))
        {
            return EFTAnswerGrade::AccentsWrong;
        }
    }

    for (const std::string& One : Expected)
    {
        if (Best == EFTAnswerGrade::Exact)
        {
            break;
        }

        const EFTAnswerGrade Grade = CompareAnswer(Given, One, Rules);

        if (Rank(Grade) > Rank(Best))
        {
            Best = Grade;
        }
    }

    return Best;
}

namespace
{
    constexpr size_t MinimumPartialLetters = 2;
    constexpr size_t MinimumTypoLetters = 4;

    std::vector<std::string> SplitWords(std::string_view Text)
    {
        std::vector<std::string> Words;
        std::string Current;

        for (size_t Index = 0; Index < Text.size();)
        {
            const unsigned char LeadByte = static_cast<unsigned char>(Text[Index]);
            const size_t Length = (std::min)(SequenceLength(LeadByte), Text.size() - Index);
            const std::string_view Sequence = Text.substr(Index, Length);

            Index += Length;

            if (IsIgnorableSpace(Sequence) || Sequence == "-")
            {
                if (!Current.empty())
                {
                    Words.push_back(std::move(Current));
                    Current.clear();
                }

                continue;
            }

            Current.append(Sequence);
        }

        if (!Current.empty())
        {
            Words.push_back(std::move(Current));
        }

        return Words;
    }

    std::vector<std::string> ApproximationWords(std::string_view Text, const FTAnswerComparisonRules& Rules)
    {
        std::string Spaced;

        for (const std::string& Word : SplitWords(Prepare(Text, Rules.bIgnoreCase)))
        {
            if (!Spaced.empty())
            {
                Spaced.push_back(' ');
            }

            Spaced += Word;
        }

        std::string Comparable = ExpandLigatures(StripLeadingParticle(Spaced));

        if (!Rules.bRequireExactAccents)
        {
            Comparable = FoldAccents(Comparable);
        }

        return SplitWords(Comparable);
    }

    std::u32string DecodeLetters(std::string_view Text)
    {
        std::u32string Letters;

        for (size_t Index = 0; Index < Text.size();)
        {
            const unsigned char LeadByte = static_cast<unsigned char>(Text[Index]);
            const size_t Length = (std::min)(SequenceLength(LeadByte), Text.size() - Index);

            char32_t CodePoint = Length == 1 ? LeadByte : static_cast<char32_t>(LeadByte & (0xFF >> (Length + 1)));

            for (size_t Offset = 1; Offset < Length; ++Offset)
            {
                CodePoint = (CodePoint << 6) | (static_cast<unsigned char>(Text[Index + Offset]) & 0x3F);
            }

            Letters.push_back(CodePoint);
            Index += Length;
        }

        return Letters;
    }

    bool IsWithinOneEdit(const std::u32string& Left, const std::u32string& Right)
    {
        const size_t Shorter = (std::min)(Left.size(), Right.size());
        const size_t SizeGap = Left.size() > Right.size() ? Left.size() - Right.size() : Right.size() - Left.size();

        if (SizeGap > 1)
        {
            return false;
        }

        size_t SharedStart = 0;

        while (SharedStart < Shorter && Left[SharedStart] == Right[SharedStart])
        {
            ++SharedStart;
        }

        size_t SharedEnd = 0;

        while (SharedEnd < Shorter - SharedStart
               && Left[Left.size() - 1 - SharedEnd] == Right[Right.size() - 1 - SharedEnd])
        {
            ++SharedEnd;
        }

        return Left.size() - SharedStart - SharedEnd <= 1 && Right.size() - SharedStart - SharedEnd <= 1;
    }
}

bool IsApproximateTranslation(std::string_view Given,
                              std::string_view ExpectedTranslations,
                              const FTAnswerComparisonRules& Rules)
{
    std::string GivenJoined;

    for (const std::string& Word : ApproximationWords(Given, Rules))
    {
        GivenJoined += Word;
    }

    const std::u32string GivenLetters = DecodeLetters(GivenJoined);

    if (GivenLetters.size() < MinimumPartialLetters)
    {
        return false;
    }

    for (const std::string& Translation : SplitTranslations(ExpectedTranslations))
    {
        std::string Opening;

        for (const std::string& Word : ApproximationWords(Translation, Rules))
        {
            Opening += Word;

            const std::u32string OpeningLetters = DecodeLetters(Opening);

            if (OpeningLetters == GivenLetters)
            {
                return true;
            }

            if (OpeningLetters.size() >= MinimumTypoLetters && IsWithinOneEdit(GivenLetters, OpeningLetters))
            {
                return true;
            }
        }
    }

    return false;
}

std::string NormalizeAnswer(std::string_view Text, const FTAnswerComparisonRules& Rules)
{
    const std::string Spaced = StripSpacing(Prepare(Text, Rules.bIgnoreCase), Rules);

    if (Rules.bRequireExactAccents)
    {
        return Spaced;
    }

    return FoldAccents(Spaced);
}

EFTAnswerGrade CompareAnswer(std::string_view Given,
                           std::string_view Expected,
                           const FTAnswerComparisonRules& Rules,
                           std::string_view ExpectedWithoutArticle)
{
    const std::string PreparedExpected = Prepare(Expected, Rules.bIgnoreCase);

    // Accents are deliberately not folded yet. Folding here would report an answer with the
    // wrong accents as exact, and the interface needs to tell those apart.
    //
    // The English particle comes off both sides rather than the expected answer being compared
    // against two spellings, which reaches the same place with one comparison: "need", "to need"
    // and "the need" are the same answer whichever of them the card was showing. Only "to " and
    // "the " go, so a French expected answer's own article is untouched and still governed by
    // bAllowMissingArticle below.
    const std::string ComparableGiven = ExpandLigatures(StripSpacing(
        StripLeadingParticle(Prepare(Given, Rules.bIgnoreCase)), Rules));
    const std::string ComparableExpected = ExpandLigatures(StripSpacing(
        StripLeadingParticle(PreparedExpected), Rules));

    if (ComparableGiven == ComparableExpected)
    {
        return EFTAnswerGrade::Exact;
    }

    const bool bArticleMayBeOmitted = Rules.bAllowMissingArticle && !ExpectedWithoutArticle.empty();

    std::string ComparableWithoutArticle;
    if (bArticleMayBeOmitted)
    {
        ComparableWithoutArticle = ExpandLigatures(StripSpacing(
            Prepare(ExpectedWithoutArticle, Rules.bIgnoreCase), Rules));

        if (ComparableGiven == ComparableWithoutArticle)
        {
            return EFTAnswerGrade::ArticleOmitted;
        }
    }

    if (Rules.bRequireExactAccents)
    {
        return EFTAnswerGrade::Wrong;
    }

    const std::string FoldedGiven = FoldAccents(ComparableGiven);

    if (FoldedGiven == FoldAccents(ComparableExpected))
    {
        return EFTAnswerGrade::AccentsWrong;
    }

    if (bArticleMayBeOmitted && FoldedGiven == FoldAccents(ComparableWithoutArticle))
    {
        return EFTAnswerGrade::AccentsWrong;
    }

    return EFTAnswerGrade::Wrong;
}
