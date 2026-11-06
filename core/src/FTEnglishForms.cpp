#include "core/FTEnglishForms.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <initializer_list>
#include <unordered_map>

namespace
{
    constexpr size_t MinimumInflectedBaseLength = 2;
    constexpr size_t MaximumPlaceholderWords = 3;

    const std::unordered_map<std::string_view, std::vector<std::string_view>>& GetIrregularForms()
    {
        static const std::unordered_map<std::string_view, std::vector<std::string_view>> Forms{
            {"arise", {"arose", "arisen"}}, {"awake", {"awoke", "awoken"}},
            {"be", {"am", "is", "are", "was", "were", "been", "being"}}, {"bear", {"bore", "borne", "born"}},
            {"beat", {"beaten"}}, {"become", {"became"}}, {"begin", {"began", "begun"}}, {"bend", {"bent"}},
            {"bind", {"bound"}}, {"bite", {"bit", "bitten"}}, {"bleed", {"bled"}}, {"blow", {"blew", "blown"}},
            {"break", {"broke", "broken"}}, {"breed", {"bred"}}, {"bring", {"brought"}}, {"build", {"built"}},
            {"burn", {"burnt"}}, {"buy", {"bought"}}, {"catch", {"caught"}}, {"choose", {"chose", "chosen"}},
            {"come", {"came"}}, {"creep", {"crept"}}, {"deal", {"dealt"}}, {"dig", {"dug"}},
            {"do", {"did", "done", "does"}}, {"draw", {"drew", "drawn"}}, {"dream", {"dreamt"}},
            {"drink", {"drank", "drunk"}}, {"drive", {"drove", "driven"}}, {"eat", {"ate", "eaten"}},
            {"fall", {"fell", "fallen"}}, {"feed", {"fed"}}, {"feel", {"felt"}}, {"fight", {"fought"}},
            {"find", {"found"}}, {"flee", {"fled"}}, {"fly", {"flew", "flown"}},
            {"forbid", {"forbade", "forbidden"}}, {"forget", {"forgot", "forgotten"}},
            {"forgive", {"forgave", "forgiven"}}, {"freeze", {"froze", "frozen"}}, {"get", {"got", "gotten"}},
            {"give", {"gave", "given"}}, {"go", {"went", "gone", "goes"}}, {"grind", {"ground"}},
            {"grow", {"grew", "grown"}}, {"hang", {"hung"}}, {"have", {"had", "has"}}, {"hear", {"heard"}},
            {"hide", {"hid", "hidden"}}, {"hold", {"held"}}, {"keep", {"kept"}}, {"kneel", {"knelt"}},
            {"know", {"knew", "known"}}, {"lay", {"laid"}}, {"lead", {"led"}}, {"lean", {"leant"}},
            {"leap", {"leapt"}}, {"learn", {"learnt"}}, {"leave", {"left"}}, {"lend", {"lent"}},
            {"lie", {"lay", "lain"}}, {"light", {"lit"}}, {"lose", {"lost"}}, {"make", {"made"}},
            {"mean", {"meant"}}, {"meet", {"met"}}, {"mistake", {"mistook", "mistaken"}},
            {"overcome", {"overcame"}}, {"pay", {"paid"}}, {"prove", {"proven"}}, {"ride", {"rode", "ridden"}},
            {"ring", {"rang", "rung"}}, {"rise", {"rose", "risen"}}, {"run", {"ran"}}, {"say", {"said"}},
            {"see", {"saw", "seen"}}, {"seek", {"sought"}}, {"sell", {"sold"}}, {"send", {"sent"}},
            {"sew", {"sewn"}}, {"shake", {"shook", "shaken"}}, {"shine", {"shone"}}, {"shoot", {"shot"}},
            {"show", {"shown"}}, {"shrink", {"shrank", "shrunk"}}, {"sing", {"sang", "sung"}},
            {"sink", {"sank", "sunk"}}, {"sit", {"sat"}}, {"sleep", {"slept"}}, {"slide", {"slid"}},
            {"speak", {"spoke", "spoken"}}, {"speed", {"sped"}}, {"spend", {"spent"}}, {"spill", {"spilt"}},
            {"spin", {"spun"}}, {"spit", {"spat"}}, {"spring", {"sprang", "sprung"}}, {"stand", {"stood"}},
            {"steal", {"stole", "stolen"}}, {"stick", {"stuck"}}, {"sting", {"stung"}},
            {"stink", {"stank", "stunk"}}, {"strike", {"struck", "stricken"}}, {"strive", {"strove", "striven"}},
            {"swear", {"swore", "sworn"}}, {"sweep", {"swept"}}, {"swim", {"swam", "swum"}},
            {"swing", {"swung"}}, {"take", {"took", "taken"}}, {"teach", {"taught"}}, {"tear", {"tore", "torn"}},
            {"tell", {"told"}}, {"think", {"thought"}}, {"throw", {"threw", "thrown"}},
            {"undertake", {"undertook", "undertaken"}}, {"understand", {"understood"}},
            {"wake", {"woke", "woken"}}, {"wear", {"wore", "worn"}}, {"weep", {"wept"}}, {"win", {"won"}},
            {"wind", {"wound"}}, {"withdraw", {"withdrew", "withdrawn"}}, {"write", {"wrote", "written"}},
            {"man", {"men"}}, {"woman", {"women"}}, {"child", {"children"}}, {"person", {"people"}},
            {"foot", {"feet"}}, {"tooth", {"teeth"}}, {"mouse", {"mice"}}, {"goose", {"geese"}},
            {"good", {"better", "best"}}, {"bad", {"worse", "worst"}}, {"far", {"farther", "further", "furthest"}},
            {"many", {"more", "most"}}, {"much", {"more", "most"}}, {"little", {"less", "least"}}};

        return Forms;
    }

    bool IsPlaceholder(std::string_view Word)
    {
        static constexpr std::array<std::string_view, 8> Placeholders{
            "one's", "oneself", "someone", "someone's", "somebody", "something", "sb", "sth"};

        return std::find(Placeholders.begin(), Placeholders.end(), Word) != Placeholders.end();
    }

    bool IsVowel(char Letter)
    {
        return Letter == 'a' || Letter == 'e' || Letter == 'i' || Letter == 'o' || Letter == 'u';
    }

    bool IsSuffixed(std::string_view Word, std::string_view Stem, std::initializer_list<std::string_view> Suffixes)
    {
        if (Word.size() <= Stem.size() || Word.compare(0, Stem.size(), Stem) != 0)
        {
            return false;
        }

        const std::string_view Ending = Word.substr(Stem.size());

        return std::find(Suffixes.begin(), Suffixes.end(), Ending) != Suffixes.end();
    }

    bool IsRegularForm(std::string_view Word, std::string_view Base)
    {
        if (Base.size() < MinimumInflectedBaseLength)
        {
            return false;
        }

        if (IsSuffixed(Word, Base, {"s", "es", "d", "ed", "ing", "er", "est"}))
        {
            return true;
        }

        const char Last = Base.back();
        const std::string_view WithoutLast = Base.substr(0, Base.size() - 1);

        if (Last == 'e' && IsSuffixed(Word, WithoutLast, {"ing", "ed", "er", "est"}))
        {
            return true;
        }

        if (Base.size() > 2 && Base.substr(Base.size() - 2) == "ie"
            && IsSuffixed(Word, Base.substr(0, Base.size() - 2), {"ying"}))
        {
            return true;
        }

        if (Last == 'y' && !IsVowel(Base[Base.size() - 2]) && IsSuffixed(Word, WithoutLast, {"ies", "ied", "ier", "iest"}))
        {
            return true;
        }

        if (Last == 'f' && IsSuffixed(Word, WithoutLast, {"ves"}))
        {
            return true;
        }

        if (Base.size() > 2 && Base.substr(Base.size() - 2) == "fe"
            && IsSuffixed(Word, Base.substr(0, Base.size() - 2), {"ves"}))
        {
            return true;
        }

        const bool bDoubles = !IsVowel(Last) && Last != 'w' && Last != 'x' && Last != 'y' && IsVowel(Base[Base.size() - 2]);

        if (bDoubles && Word.size() > Base.size() + 1 && Word[Base.size()] == Last)
        {
            return IsSuffixed(Word, Word.substr(0, Base.size() + 1), {"ed", "ing", "er", "est"});
        }

        return false;
    }

    std::vector<std::string> SplitWords(std::string_view Text)
    {
        std::vector<std::string> Words;
        std::string Current;
        int Depth = 0;

        const auto Flush = [&Words, &Current]()
        {
            while (!Current.empty() && Current.back() == '\'')
            {
                Current.pop_back();
            }

            if (!Current.empty())
            {
                Words.push_back(std::move(Current));
            }

            Current.clear();
        };

        for (size_t Index = 0; Index < Text.size(); ++Index)
        {
            const char Character = Text[Index];

            if (Character == '(')
            {
                Flush();
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

            if (Text.compare(Index, 3, "\xE2\x80\x99") == 0)
            {
                if (!Current.empty())
                {
                    Current.push_back('\'');
                }

                Index += 2;

                continue;
            }

            const unsigned char Byte = static_cast<unsigned char>(Character);

            if (std::isalnum(Byte) || Byte >= 0x80 || (Character == '\'' && !Current.empty()))
            {
                Current.push_back(static_cast<char>(std::tolower(Byte)));

                continue;
            }

            Flush();
        }

        Flush();

        return Words;
    }

    std::string_view WithoutPossessive(std::string_view Word)
    {
        if (Word.size() > 2 && Word.substr(Word.size() - 2) == "'s")
        {
            return Word.substr(0, Word.size() - 2);
        }

        return Word;
    }

    bool MatchesAt(const std::vector<std::string>& Words, size_t Start,
                   const std::vector<std::string>& Phrase, size_t PhraseIndex)
    {
        if (PhraseIndex == Phrase.size())
        {
            return true;
        }

        if (Start >= Words.size())
        {
            return false;
        }

        if (IsPlaceholder(Phrase[PhraseIndex]))
        {
            for (size_t Count = 1; Count <= MaximumPlaceholderWords && Start + Count <= Words.size(); ++Count)
            {
                if (MatchesAt(Words, Start + Count, Phrase, PhraseIndex + 1))
                {
                    return true;
                }
            }

            return false;
        }

        if (Words[Start] != Phrase[PhraseIndex] && !IsEnglishForm(WithoutPossessive(Words[Start]), Phrase[PhraseIndex]))
        {
            return false;
        }

        return MatchesAt(Words, Start + 1, Phrase, PhraseIndex + 1);
    }
}

bool IsEnglishForm(std::string_view Word, std::string_view Base)
{
    if (Word == Base)
    {
        return true;
    }

    const auto& Irregular = GetIrregularForms();
    const auto Found = Irregular.find(Base);

    if (Found != Irregular.end() && std::find(Found->second.begin(), Found->second.end(), Word) != Found->second.end())
    {
        return true;
    }

    return IsRegularForm(Word, Base);
}

bool MentionsTranslation(std::string_view Text, const std::vector<std::string>& Translations)
{
    static constexpr std::array<std::string_view, 4> Particles{"to", "the", "a", "an"};

    const std::vector<std::string> Words = SplitWords(Text);

    for (const std::string& Translation : Translations)
    {
        std::vector<std::string> Phrase = SplitWords(Translation);

        if (Phrase.size() > 1 && std::find(Particles.begin(), Particles.end(), Phrase.front()) != Particles.end())
        {
            Phrase.erase(Phrase.begin());
        }

        if (Phrase.empty() || std::all_of(Phrase.begin(), Phrase.end(), IsPlaceholder))
        {
            continue;
        }

        for (size_t Start = 0; Start < Words.size(); ++Start)
        {
            if (MatchesAt(Words, Start, Phrase, 0))
            {
                return true;
            }
        }
    }

    return false;
}
