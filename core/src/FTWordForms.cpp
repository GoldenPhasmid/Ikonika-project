#include "core/FTWordForms.h"

#include "core/FTTextNormalization.h"

#include <QString>
#include <QStringList>
#include <QStringView>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <unordered_map>
#include <utility>

static constexpr char ItemIdSeparator = '#';
static constexpr char RoundIdSeparator = '@';
static constexpr int MaximumWordsBetweenSubjectAndVerb = 3;
static constexpr std::string_view PromptBlank = "___";

static constexpr int MoodIndicative = 1;
static constexpr int TensePresent = 1;
static constexpr int TenseImperfect = 2;
static constexpr int TensePreterite = 3;
static constexpr int TenseFuture = 4;
static constexpr int TensePastParticiple = 8;
static constexpr int PersonsPerNumber = 3;
static constexpr int GendersPerNumber = 2;
static constexpr int FormGenderUnmarked = 0;
static constexpr int FormGenderMasculine = 1;
static constexpr int FormGenderFeminine = 2;
static constexpr int FormSingular = 1;
static constexpr int FormPlural = 2;
static constexpr int VerbCellCount = 6;
static constexpr int AgreementCellCount = 4;
static constexpr int MaximumEndingsPerPerson = 4;
static constexpr int MaximumTenses = 5;
static constexpr int MaximumSecondAuxiliaryVerbs = 24;

struct FTFormsKind
{
    std::string_view Name;
    std::string_view IdPrefix;
    bool bHasTenses = false;
    std::string_view Training;
    std::string_view SharedPartOfSpeech;
};

static constexpr std::array<FTFormsKind, 3> FormsKinds{{
    {FormsKindVerb, "conjugation", true, FormsTrainingConjugation, {}},
    {FormsKindAdjective, "forms-adj", false, FormsTrainingAgreement, "det"},
    {FormsKindNoun, "forms-noun", false, FormsTrainingAgreement, {}}
}};

struct FTFormsTraining
{
    std::string_view Name;
    int WordsPerSession = 0;
};

static constexpr std::array<FTFormsTraining, 2> FormsTrainings{{
    {FormsTrainingConjugation, 4},
    {FormsTrainingAgreement, 8}
}};

static constexpr std::string_view TrainingIdPrefix = "training-";

struct FTFormsCell
{
    int First = 0;
    int Number = 0;
};

struct FTFormsItem
{
    FTFormsCell Cell;
    std::string Form;
    std::string Base;
    std::vector<std::string> Alternatives;
};

struct FTFormsTable
{
    std::vector<FTFormsItem> Items;
    bool bRegular = false;
};

using FTFormsEndings = std::array<std::array<std::string_view, MaximumEndingsPerPerson>, VerbCellCount>;

struct FTFormsTense
{
    std::string_view Name;
    std::string_view Label;
    std::string_view Requires;
    int Tense = 0;
    bool bCompound = false;
    FTFormsEndings Endings;
};

struct FTFormsLanguage
{
    std::string_view Language;
    std::array<std::string_view, VerbCellCount> Pronouns;
    std::array<std::string_view, VerbCellCount> PronounLabels;
    std::string_view ElidedFirstPerson;
    std::array<FTFormsTense, MaximumTenses> Tenses;
    std::array<std::string_view, VerbCellCount> Auxiliary;
    std::array<std::string_view, VerbCellCount> SecondAuxiliary;
    std::array<std::string_view, MaximumSecondAuxiliaryVerbs> SecondAuxiliaryVerbs;
    std::array<std::string_view, AgreementCellCount> AdjectiveCues;
    std::array<std::string_view, AgreementCellCount> NounCues;
    std::string_view FeminineSuffix;
    std::string_view FeminineReplaces;
    std::string_view PluralSuffix;
    std::string_view PluralSuffixAfterConsonant;
    std::string_view UnchangedPluralEndings;
    bool bHasOneGenderAdjectives = false;
    std::string_view FirstGroupEnding;
    std::string_view FirstGroupException;
    std::string_view SecondGroupEnding;
    std::string_view SecondGroupMarker;
    std::array<std::string_view, VerbCellCount> SubjectWords;
    std::string_view WordsBetweenSubjectAndVerb;
    bool bOmitsSubject = false;
    std::string_view OptionalPronoun;
    std::array<std::string_view, 3> ModelInfinitiveEndings{"ar", "er", "ir"};
    bool bFutureFromStem = false;
    std::string_view PluralReplaces;
};

static constexpr std::array<FTFormsLanguage, 4> FormsLanguages{{
    {
        "fr",
        {"je", "tu", "il", "nous", "vous", "ils"},
        {"je", "tu", "il/elle", "nous", "vous", "ils/elles"},
        "j'",
        {{
            {FormsTensePresent, "présent", "", TensePresent, false,
             {{{"e", "s", "x"}, {"es", "s", "x"}, {"e", "t", "d"}, {"ons"}, {"ez"}, {"ent"}}}},
            {"imperfect", "imparfait", FormsTensePresent, TenseImperfect, false,
             {{{"ais"}, {"ais"}, {"ait"}, {"ions"}, {"iez"}, {"aient"}}}},
            {"future", "futur", FormsTensePresent, TenseFuture, false,
             {{{"ai"}, {"as"}, {"a"}, {"ons"}, {"ez"}, {"ont"}}}},
            {"perfect", "passé composé", FormsTensePresent, TensePastParticiple, true, {}}
        }},
        {"ai", "as", "a", "avons", "avez", "ont"},
        {"suis", "es", "est", "sommes", "êtes", "sont"},
        {"aller", "venir", "arriver", "partir", "entrer", "sortir", "monter", "descendre", "naître", "mourir",
         "rester", "tomber", "retourner", "rentrer", "devenir", "revenir", "repartir", "intervenir", "parvenir",
         "survenir", "redescendre", "remonter", "ressortir", "décéder"},
        {"il", "elle", "ils", "elles"},
        {"un", "une", "des", "des"},
        "e",
        "",
        "s",
        "",
        "sxz",
        false,
        "er",
        "aller",
        "ir",
        "issons",
        {"je j", "tu", "il elle on", "nous", "vous", "ils elles"},
        "ne n me m te t se s le la l les lui leur y en nous vous",
        false,
        ""
    },
    {
        "es",
        {"yo", "tú", "él", "nosotros", "vosotros", "ellos"},
        {"yo", "tú", "él/usted", "nosotros", "vosotros", "ellos/ustedes"},
        "",
        {{
            {FormsTensePresent, "presente", "", TensePresent, false,
             {{{"o"}, {"as", "es"}, {"a", "e"}, {"amos", "emos", "imos"}, {"áis", "éis", "ís"}, {"an", "en"}}}},
            {"imperfect", "imperfecto", FormsTensePresent, TenseImperfect, false,
             {{{"aba", "ía"}, {"abas", "ías"}, {"aba", "ía"}, {"ábamos", "íamos"}, {"abais", "íais"},
               {"aban", "ían"}}}},
            {"preterite", "indefinido", FormsTensePresent, TensePreterite, false,
             {{{"é", "í"}, {"aste", "iste"}, {"ó", "ió"}, {"amos", "imos"}, {"asteis", "isteis"},
               {"aron", "ieron"}}}},
            {"future", "futuro", FormsTensePresent, TenseFuture, false,
             {{{"é"}, {"ás"}, {"á"}, {"emos"}, {"éis"}, {"án"}}}},
            {"perfect", "pretérito perfecto", FormsTensePresent, TensePastParticiple, true, {}}
        }},
        {"he", "has", "ha", "hemos", "habéis", "han"},
        {},
        {},
        {"él", "ella", "ellos", "ellas"},
        {"un", "una", "unos", "unas"},
        "a",
        "o",
        "s",
        "es",
        "",
        true,
        "",
        "",
        "",
        "",
        {"yo", "tú", "él ella usted", "nosotros nosotras", "vosotros vosotras", "ellos ellas ustedes"},
        "no me te se lo la le nos os los las les",
        true,
        "vosotros"
    },
    {
        "pt",
        {"eu", "tu", "ele", "nós", "vós", "eles"},
        {"eu", "tu", "ele/ela", "nós", "vós", "eles/elas"},
        "",
        {{
            {FormsTensePresent, "presente", "", TensePresent, false,
             {{{"o"}, {"as", "es"}, {"a", "e"}, {"amos", "emos", "imos"}, {"ais", "eis", "is"}, {"am", "em"}}}},
            {"preterite", "pretérito perfeito", FormsTensePresent, TensePreterite, false,
             {{{"ei", "i"}, {"aste", "este", "iste"}, {"ou", "eu", "iu"}, {"ámos", "amos", "emos", "imos"},
               {"astes", "estes", "istes"}, {"aram", "eram", "iram"}}}},
            {"imperfect", "imperfeito", FormsTensePresent, TenseImperfect, false,
             {{{"ava", "ia"}, {"avas", "ias"}, {"ava", "ia"}, {"ávamos", "íamos"}, {"áveis", "íeis"},
               {"avam", "iam"}}}},
            {"future", "futuro", FormsTensePresent, TenseFuture, false,
             {{{"ei"}, {"ás"}, {"á"}, {"emos"}, {"eis"}, {"ão"}}}},
            {"perfect", "perfeito composto", FormsTensePresent, TensePastParticiple, true, {}}
        }},
        {"tenho", "tens", "tem", "temos", "tendes", "têm"},
        {},
        {},
        {"ele", "ela", "eles", "elas"},
        {"um", "uma", "uns", "umas"},
        "a",
        "o",
        "s",
        "es",
        "",
        true,
        "",
        "",
        "",
        "",
        {"eu", "tu", "ele ela você", "nós", "vós", "eles elas vocês"},
        "não me te se o a lhe nos vos os as lhes",
        true,
        ""
    },
    {
        "it",
        {"io", "tu", "lui", "noi", "voi", "loro"},
        {"io", "tu", "lui/lei", "noi", "voi", "loro"},
        "",
        {{
            {FormsTensePresent, "presente", "", TensePresent, false,
             {{{"o", "isco"}, {"i", "isci"}, {"a", "e", "isce"}, {"iamo"}, {"ate", "ete", "ite"},
               {"ano", "ono", "iscono"}}}},
            {"perfect", "passato prossimo", FormsTensePresent, TensePastParticiple, true, {}},
            {"imperfect", "imperfetto", FormsTensePresent, TenseImperfect, false,
             {{{"avo", "evo", "ivo"}, {"avi", "evi", "ivi"}, {"ava", "eva", "iva"}, {"avamo", "evamo", "ivamo"},
               {"avate", "evate", "ivate"}, {"avano", "evano", "ivano"}}}},
            {"future", "futuro", FormsTensePresent, TenseFuture, false,
             {{{"erò", "irò"}, {"erai", "irai"}, {"erà", "irà"}, {"eremo", "iremo"}, {"erete", "irete"},
               {"eranno", "iranno"}}}},
            {"preterite", "passato remoto", FormsTensePresent, TensePreterite, false,
             {{{"ai", "ei", "etti", "ii"}, {"asti", "esti", "isti"}, {"ò", "é", "ette", "ì"},
               {"ammo", "emmo", "immo"}, {"aste", "este", "iste"}, {"arono", "erono", "ettero", "irono"}}}}
        }},
        {"ho", "hai", "ha", "abbiamo", "avete", "hanno"},
        {"sono", "sei", "è", "siamo", "siete", "sono"},
        {"essere", "stare", "andare", "venire", "arrivare", "partire", "tornare", "ritornare", "uscire",
         "entrare", "restare", "rimanere", "diventare", "nascere", "morire", "cadere", "succedere", "accadere",
         "piacere", "sembrare", "riuscire", "scendere", "crescere", "apparire"},
        {},
        {},
        "",
        "",
        "i",
        "",
        "",
        false,
        "",
        "",
        "",
        "",
        {"io", "tu", "lui lei", "noi", "voi", "loro"},
        "non mi m ti t si s ci c vi v lo la l li le gli ne me te se ce ve",
        true,
        "",
        {"are", "ere", "ire"},
        true,
        "o"
    }
}};

static const FTFormsKind* FindKind(std::string_view Name)
{
    for (const FTFormsKind& Kind : FormsKinds)
    {
        if (Kind.Name == Name)
        {
            return &Kind;
        }
    }

    return nullptr;
}

static const FTFormsTraining* FindTraining(std::string_view Name)
{
    for (const FTFormsTraining& Training : FormsTrainings)
    {
        if (Training.Name == Name)
        {
            return &Training;
        }
    }

    return nullptr;
}

static const FTFormsKind* FindKindOfId(std::string_view Id)
{
    for (const FTFormsKind& Kind : FormsKinds)
    {
        const size_t Length = Kind.IdPrefix.size();

        if (Id.size() > Length && Id.substr(0, Length) == Kind.IdPrefix && Id[Length] == '-')
        {
            return &Kind;
        }
    }

    return nullptr;
}

static const FTFormsLanguage* FindLanguage(std::string_view TargetLanguage)
{
    for (const FTFormsLanguage& Language : FormsLanguages)
    {
        if (Language.Language == TargetLanguage)
        {
            return &Language;
        }
    }

    return nullptr;
}

static const FTFormsTense* FindTense(const FTFormsLanguage& Language, std::string_view Name)
{
    for (const FTFormsTense& Tense : Language.Tenses)
    {
        if (!Tense.Name.empty() && Tense.Name == Name)
        {
            return &Tense;
        }
    }

    return nullptr;
}

static const std::array<std::string_view, AgreementCellCount>& GetAgreementCues(const FTFormsKind& Kind,
                                                                               const FTFormsLanguage& Language)
{
    return Kind.Name == FormsKindAdjective ? Language.AdjectiveCues : Language.NounCues;
}

static bool IsVerbKind(const FTFormsKind& Kind)
{
    return Kind.Name == FormsKindVerb;
}

static bool IsSupported(const FTFormsKind& Kind, const FTFormsLanguage& Language)
{
    return IsVerbKind(Kind) || !GetAgreementCues(Kind, Language).front().empty();
}

static std::string_view ReadIdLanguage(const FTFormsKind& Kind, std::string_view Id)
{
    const std::string_view AfterPrefix = Id.substr(Kind.IdPrefix.size() + 1);

    return AfterPrefix.substr(0, AfterPrefix.find('-'));
}

static std::string_view ReadIdTense(const FTFormsKind& Kind, std::string_view Id)
{
    if (!Kind.bHasTenses)
    {
        return {};
    }

    const std::string_view Stored = Id.substr(0, Id.find(ItemIdSeparator));
    const size_t DashAt = Stored.rfind('-');

    return DashAt == std::string_view::npos ? std::string_view() : Stored.substr(DashAt + 1);
}

static std::string ReadIdHeadword(const FTFormsKind& Kind, std::string_view Id)
{
    std::string_view Rest = Id.substr(0, Id.find(ItemIdSeparator));

    Rest.remove_prefix(std::min(Rest.size(), Kind.IdPrefix.size() + 1));
    Rest.remove_prefix(std::min(Rest.size(), ReadIdLanguage(Kind, Id).size() + 1));

    const size_t TenseLength = ReadIdTense(Kind, Id).size();

    if (Kind.bHasTenses && Rest.size() > TenseLength)
    {
        Rest.remove_suffix(TenseLength + 1);
    }

    return std::string(Rest);
}

static bool StartsWithVowelSound(const std::string& Form)
{
    if (Form.empty())
    {
        return false;
    }

    const unsigned char First = static_cast<unsigned char>(Form.front());

    if (std::string_view("aeiouyh").find(static_cast<char>(First)) != std::string_view::npos)
    {
        return true;
    }

    constexpr unsigned char LatinSupplementLead = 0xC3;
    constexpr unsigned char LowercaseCedilla = 0xA7;
    constexpr unsigned char LowercaseEnye = 0xB1;
    constexpr unsigned char LowercaseLettersBegin = 0xA0;

    if (First != LatinSupplementLead || Form.size() < 2)
    {
        return false;
    }

    const unsigned char Second = static_cast<unsigned char>(Form[1]);

    return Second >= LowercaseLettersBegin && Second != LowercaseCedilla && Second != LowercaseEnye;
}

static bool IsSingleWord(const std::string& Form)
{
    return !Form.empty() && Form.find_first_of(" '") == std::string::npos
        && Form.find("\xE2\x80\x99") == std::string::npos && Form.find("\xC2\xB7") == std::string::npos;
}

static bool EndsWith(std::string_view Word, std::string_view Suffix)
{
    return !Suffix.empty() && Word.size() >= Suffix.size() && Word.substr(Word.size() - Suffix.size()) == Suffix;
}

static bool EndsWithVowel(std::string_view Word)
{
    static constexpr std::array<std::string_view, 15> Vowels{"a", "e", "i", "o", "u", "á", "é", "í", "ó", "ú",
                                                             "ã", "õ", "â", "ê", "ô"};

    return std::any_of(Vowels.begin(), Vowels.end(),
                       [Word](std::string_view Vowel) { return EndsWith(Word, Vowel); });
}

static bool IsSameSpelling(const std::string& Left, const std::string& Right)
{
    return FoldCase(Left) == FoldCase(Right);
}

static std::string MakeRegularPlural(const FTFormsLanguage& Language, const std::string& Word)
{
    if (EndsWith(Word, Language.PluralReplaces))
    {
        return Word.substr(0, Word.size() - Language.PluralReplaces.size()) + std::string(Language.PluralSuffix);
    }

    if (!Word.empty() && Language.UnchangedPluralEndings.find(Word.back()) != std::string_view::npos)
    {
        return Word;
    }

    if (!Language.PluralSuffixAfterConsonant.empty() && !EndsWithVowel(Word))
    {
        return Word + std::string(Language.PluralSuffixAfterConsonant);
    }

    return Word + std::string(Language.PluralSuffix);
}

static std::string MakeRegularFeminine(const FTFormsLanguage& Language, const std::string& Word)
{
    if (EndsWith(Word, Language.FeminineReplaces))
    {
        return Word.substr(0, Word.size() - Language.FeminineReplaces.size()) + std::string(Language.FeminineSuffix);
    }

    return EndsWith(Word, Language.FeminineSuffix) ? Word : Word + std::string(Language.FeminineSuffix);
}

static bool CollectSpelling(const std::vector<FTDictionaryForm>& Forms, const FTDictionaryForm& Wanted,
                            std::string& Found, const std::string& Ignored = {})
{
    for (const FTDictionaryForm& Form : Forms)
    {
        const bool bMatches = Form.Mood == Wanted.Mood && Form.Tense == Wanted.Tense && Form.Person == Wanted.Person
            && Form.Gender == Wanted.Gender && Form.Number == Wanted.Number && IsSingleWord(Form.Text)
            && Form.Text != Ignored;

        if (!bMatches)
        {
            continue;
        }

        if (!Found.empty() && Found != Form.Text)
        {
            return false;
        }

        Found = Form.Text;
    }

    return true;
}

static std::vector<std::string> CollectSpellings(const std::vector<FTDictionaryForm>& Forms,
                                                 const FTDictionaryForm& Wanted)
{
    std::vector<std::string> Found;

    for (const FTDictionaryForm& Form : Forms)
    {
        const bool bMatches = Form.Mood == Wanted.Mood && Form.Tense == Wanted.Tense && Form.Person == Wanted.Person
            && Form.Gender == Wanted.Gender && Form.Number == Wanted.Number && IsSingleWord(Form.Text);

        if (bMatches && std::find(Found.begin(), Found.end(), Form.Text) == Found.end())
        {
            Found.push_back(Form.Text);
        }
    }

    std::sort(Found.begin(), Found.end());

    return Found;
}

static size_t CountSharedStart(std::string_view Left, std::string_view Right)
{
    const size_t Limit = std::min(Left.size(), Right.size());
    size_t Count = 0;

    while (Count < Limit && Left[Count] == Right[Count])
    {
        ++Count;
    }

    return Count;
}

static std::string PickFurthestSpelling(const std::vector<std::string>& Spellings, std::string_view From)
{
    return *std::min_element(Spellings.begin(), Spellings.end(),
        [From](const std::string& Left, const std::string& Right)
        {
            return CountSharedStart(Left, From) < CountSharedStart(Right, From);
        });
}

static std::string PickClosestSpelling(const std::vector<std::string>& Spellings, std::string_view To)
{
    return *std::max_element(Spellings.begin(), Spellings.end(),
        [To](const std::string& Left, const std::string& Right)
        {
            return CountSharedStart(Left, To) < CountSharedStart(Right, To);
        });
}

static std::vector<std::string> ListOtherSpellings(const std::vector<std::string>& Spellings,
                                                   const std::string& Shown)
{
    std::vector<std::string> Others;

    for (const std::string& Spelling : Spellings)
    {
        if (Spelling != Shown)
        {
            Others.push_back(Spelling);
        }
    }

    return Others;
}

static bool CollectAgreement(const std::vector<FTDictionaryForm>& Forms, int Gender, int Number, std::string& Found,
                             const std::string& Ignored = {})
{
    FTDictionaryForm Wanted;
    Wanted.Gender = Gender;
    Wanted.Number = Number;

    return CollectSpelling(Forms, Wanted, Found, Ignored);
}

static bool CollectPlural(const FTFormsLanguage& Language, const std::vector<FTDictionaryForm>& Forms, int Gender,
                          const std::string& Singular, std::string& Found)
{
    if (!CollectAgreement(Forms, FormGenderUnmarked, FormPlural, Found)
        || !CollectAgreement(Forms, Gender, FormPlural, Found))
    {
        return false;
    }

    if (Found.empty() && MakeRegularPlural(Language, Singular) == Singular)
    {
        Found = Singular;
    }

    return !Found.empty();
}

static bool CollectFemininePlural(const FTFormsLanguage& Language, const std::vector<FTDictionaryForm>& Forms,
                                  const std::string& Feminine, const std::string& MasculinePlural,
                                  bool bSharedPlural, std::string& Found)
{
    if (!CollectAgreement(Forms, FormGenderFeminine, FormPlural, Found, MasculinePlural))
    {
        return false;
    }

    if (Found.empty() && bSharedPlural && !CollectAgreement(Forms, FormGenderUnmarked, FormPlural, Found))
    {
        return false;
    }

    if (Found.empty())
    {
        Found = MakeRegularPlural(Language, Feminine);
    }

    return true;
}

static std::optional<FTFormsTable> FindCompoundForms(const FTFormsLanguage& Language, const FTFormsTense& Tense,
                                                     const FTDictionaryEntry& Entry)
{
    FTDictionaryForm Wanted;
    Wanted.Tense = Tense.Tense;

    const std::vector<std::string> Participles = CollectSpellings(Entry.Forms, Wanted);

    if (Participles.empty() || Language.Auxiliary.front().empty())
    {
        return std::nullopt;
    }

    const std::string Participle = PickFurthestSpelling(Participles, Entry.Headword);
    const std::vector<std::string> OtherParticiples = ListOtherSpellings(Participles, Participle);

    const bool bSecondAuxiliary = std::find(Language.SecondAuxiliaryVerbs.begin(), Language.SecondAuxiliaryVerbs.end(),
                                            std::string_view(Entry.Headword)) != Language.SecondAuxiliaryVerbs.end();

    const std::array<std::string_view, VerbCellCount>& Auxiliary =
        bSecondAuxiliary ? Language.SecondAuxiliary : Language.Auxiliary;

    FTFormsTable Table;

    for (int Number = FormSingular; Number <= FormPlural; ++Number)
    {
        const bool bAgrees = bSecondAuxiliary && Number == FormPlural;

        const auto Agree = [&Language, bAgrees](const std::string& Spelling)
        {
            return bAgrees ? MakeRegularPlural(Language, Spelling) : Spelling;
        };

        for (int Person = 1; Person <= PersonsPerNumber; ++Person)
        {
            const size_t CellIndex = static_cast<size_t>((Number - 1) * PersonsPerNumber + (Person - 1));
            const std::string Lead = std::string(Auxiliary[CellIndex]) + " ";

            FTFormsItem Item{{Person, Number}, Lead + Agree(Participle), Entry.Headword, {}};

            for (const std::string& Other : OtherParticiples)
            {
                Item.Alternatives.push_back(Lead + Agree(Other));
            }

            Table.Items.push_back(std::move(Item));
        }
    }

    return Table;
}

static std::optional<FTFormsTable> FindVerbForms(const FTFormsLanguage& Language, const FTFormsTense& Tense,
                                                 const FTDictionaryEntry& Entry)
{
    if (Tense.bCompound)
    {
        return FindCompoundForms(Language, Tense, Entry);
    }

    FTFormsTable Table;
    std::string FirstShown;

    for (int Number = FormSingular; Number <= FormPlural; ++Number)
    {
        for (int Person = 1; Person <= PersonsPerNumber; ++Person)
        {
            FTDictionaryForm Wanted;
            Wanted.Mood = MoodIndicative;
            Wanted.Tense = Tense.Tense;
            Wanted.Person = Person;
            Wanted.Number = Number;

            const std::vector<std::string> Spellings = CollectSpellings(Entry.Forms, Wanted);

            if (Spellings.empty())
            {
                return std::nullopt;
            }

            std::string Shown = FirstShown.empty() ? PickFurthestSpelling(Spellings, Entry.Headword)
                                                   : PickClosestSpelling(Spellings, FirstShown);

            if (FirstShown.empty())
            {
                FirstShown = Shown;
            }

            std::vector<std::string> Others = ListOtherSpellings(Spellings, Shown);

            Table.Items.push_back({{Person, Number}, std::move(Shown), Entry.Headword, std::move(Others)});
        }
    }

    return Table;
}

static std::string DropLastLetters(const std::string& Word, int Count)
{
    constexpr unsigned char ContinuationMask = 0xC0;
    constexpr unsigned char ContinuationBits = 0x80;

    size_t Length = Word.size();

    for (int Dropped = 0; Dropped < Count && Length > 0; ++Dropped)
    {
        --Length;

        while (Length > 0 && (static_cast<unsigned char>(Word[Length]) & ContinuationMask) == ContinuationBits)
        {
            --Length;
        }
    }

    return Word.substr(0, Length);
}

static void ReplaceAll(std::string& Text, std::string_view From, std::string_view To)
{
    size_t At = 0;

    while ((At = Text.find(From, At)) != std::string::npos)
    {
        Text.replace(At, From.size(), To);
        At += To.size();
    }
}

static std::string MakeSpellingSkeleton(const std::string& Word)
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 7> Equivalents{{
        {"qu", "c"}, {"gu", "g"}, {"ch", "c"}, {"gh", "g"}, {"z", "c"}, {"j", "g"}, {"y", "i"}
    }};

    std::string Skeleton = FoldAccents(Word);

    for (const auto& [From, To] : Equivalents)
    {
        ReplaceAll(Skeleton, From, To);
    }

    return Skeleton;
}

static bool IsRegularSpelling(const std::string& Form, const std::string& Stem, std::string_view Ending)
{
    const std::string Skeleton = MakeSpellingSkeleton(Form);

    if (Skeleton == MakeSpellingSkeleton(Stem + std::string(Ending)))
    {
        return true;
    }

    const bool bDropsLeadingI = Ending.size() > 1 && Ending.front() == 'i'
        && (EndsWith(Stem, "ñ") || EndsWith(Stem, "ll"));

    return bDropsLeadingI && Skeleton == MakeSpellingSkeleton(Stem + std::string(Ending.substr(1)));
}

static bool HasRegularSpelling(const std::vector<std::string>& Spellings, const std::string& Base,
                               const std::array<std::string_view, MaximumEndingsPerPerson>& Endings)
{
    return std::any_of(Spellings.begin(), Spellings.end(), [&Base, &Endings](const std::string& Spelling)
    {
        return std::any_of(Endings.begin(), Endings.end(), [&Base, &Spelling](std::string_view Ending)
        {
            return !Ending.empty() && IsRegularSpelling(Spelling, Base, Ending);
        });
    });
}

static bool DeviatesFromRegularModel(const FTFormsLanguage& Language, const FTDictionaryEntry& Entry)
{
    const std::string Folded = FoldAccents(Entry.Headword);

    const auto Model = std::find_if(Language.ModelInfinitiveEndings.begin(), Language.ModelInfinitiveEndings.end(),
        [&Folded](std::string_view Ending) { return EndsWith(Folded, Ending); });

    if (Model == Language.ModelInfinitiveEndings.end())
    {
        return true;
    }

    const std::string Stem = DropLastLetters(Entry.Headword, static_cast<int>(Model->size()));

    for (const FTFormsTense& Tense : Language.Tenses)
    {
        if (Tense.Name.empty() || Tense.bCompound)
        {
            continue;
        }

        const std::string& Base = Tense.Tense == TenseFuture && !Language.bFutureFromStem ? Entry.Headword : Stem;

        for (int Number = FormSingular; Number <= FormPlural; ++Number)
        {
            for (int Person = 1; Person <= PersonsPerNumber; ++Person)
            {
                FTDictionaryForm Wanted;
                Wanted.Mood = MoodIndicative;
                Wanted.Tense = Tense.Tense;
                Wanted.Person = Person;
                Wanted.Number = Number;

                const std::vector<std::string> Spellings = CollectSpellings(Entry.Forms, Wanted);
                const size_t CellIndex = static_cast<size_t>((Number - 1) * PersonsPerNumber + (Person - 1));

                if (!Spellings.empty() && !HasRegularSpelling(Spellings, Base, Tense.Endings[CellIndex]))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool BelongsToThirdGroup(const FTFormsLanguage& Language, const FTDictionaryEntry& Entry)
{
    const std::string Headword = FoldAccents(Entry.Headword);

    if (EndsWith(Headword, Language.FirstGroupEnding))
    {
        return Headword == Language.FirstGroupException;
    }

    if (!EndsWith(Headword, Language.SecondGroupEnding))
    {
        return true;
    }

    FTDictionaryForm Wanted;
    Wanted.Mood = MoodIndicative;
    Wanted.Tense = TensePresent;
    Wanted.Person = 1;
    Wanted.Number = FormPlural;

    const std::vector<std::string> Spellings = CollectSpellings(Entry.Forms, Wanted);

    return std::none_of(Spellings.begin(), Spellings.end(), [&Language](const std::string& Spelling)
    {
        return EndsWith(FoldAccents(Spelling), Language.SecondGroupMarker);
    });
}

static std::optional<FTFormsTable> FindAdjectiveForms(const FTFormsLanguage& Language, const FTDictionaryEntry& Entry)
{
    const std::string& Headword = Entry.Headword;
    const bool bSharedPlural = Entry.PartOfSpeech != FormsKindAdjective;

    std::string Feminine;
    std::string Plural;
    std::string FemininePlural;

    const bool bComplete = CollectAgreement(Entry.Forms, FormGenderFeminine, FormSingular, Feminine)
        && (!Feminine.empty() || Language.bHasOneGenderAdjectives)
        && CollectPlural(Language, Entry.Forms, FormGenderMasculine, Headword, Plural)
        && (Feminine.empty()
            || CollectFemininePlural(Language, Entry.Forms, Feminine, Plural, bSharedPlural, FemininePlural));

    if (!bComplete)
    {
        return std::nullopt;
    }

    const bool bRegularFeminine = Feminine.empty()
        || (IsSameSpelling(Feminine, MakeRegularFeminine(Language, Headword))
            && IsSameSpelling(FemininePlural, MakeRegularPlural(Language, Feminine)));

    FTFormsTable Table;
    Table.bRegular = bRegularFeminine && IsSameSpelling(Plural, MakeRegularPlural(Language, Headword));

    Table.Items.push_back({{FormGenderMasculine, FormSingular}, Headword, Headword});

    if (!Feminine.empty())
    {
        Table.Items.push_back({{FormGenderFeminine, FormSingular}, Feminine, Headword});
    }

    Table.Items.push_back({{FormGenderMasculine, FormPlural}, Plural, Headword});

    if (!Feminine.empty())
    {
        Table.Items.push_back({{FormGenderFeminine, FormPlural}, FemininePlural, Headword});
    }

    return Table;
}

static std::optional<int> FindNounGender(const FTDictionaryEntry& Entry)
{
    std::optional<int> Leading;
    int LeadingCount = 0;

    for (const FTDictionarySense& Sense : Entry.Senses)
    {
        if (!Sense.Gender.has_value())
        {
            continue;
        }

        const int Count = static_cast<int>(std::count_if(Entry.Senses.begin(), Entry.Senses.end(),
            [&Sense](const FTDictionarySense& Other) { return Other.Gender == Sense.Gender; }));

        if (Count > LeadingCount)
        {
            Leading = Sense.Gender;
            LeadingCount = Count;
        }
    }

    return Leading;
}

static std::optional<FTFormsTable> FindNounForms(const FTFormsLanguage& Language, const FTDictionaryEntry& Entry)
{
    const std::string& Headword = Entry.Headword;
    const std::optional<int> Gender = FindNounGender(Entry);

    if (!Gender.has_value())
    {
        return std::nullopt;
    }

    FTFormsTable Table;
    std::string Plural;

    if (*Gender == GenderFeminine)
    {
        if (!CollectPlural(Language, Entry.Forms, FormGenderFeminine, Headword, Plural))
        {
            return std::nullopt;
        }

        Table.bRegular = IsSameSpelling(Plural, MakeRegularPlural(Language, Headword));
        Table.Items.push_back({{FormGenderFeminine, FormSingular}, Headword, Headword});
        Table.Items.push_back({{FormGenderFeminine, FormPlural}, Plural, Headword});

        return Table;
    }

    std::string Feminine;
    std::string FemininePlural;

    const bool bComplete = CollectPlural(Language, Entry.Forms, FormGenderMasculine, Headword, Plural)
        && CollectAgreement(Entry.Forms, FormGenderFeminine, FormSingular, Feminine)
        && (Feminine.empty() || CollectFemininePlural(Language, Entry.Forms, Feminine, Plural, false, FemininePlural));

    if (!bComplete)
    {
        return std::nullopt;
    }

    const bool bRegularPartner = Feminine.empty()
        || (IsSameSpelling(Feminine, MakeRegularFeminine(Language, Headword))
            && IsSameSpelling(FemininePlural, MakeRegularPlural(Language, Feminine)));

    Table.bRegular = bRegularPartner && IsSameSpelling(Plural, MakeRegularPlural(Language, Headword));

    Table.Items.push_back({{FormGenderMasculine, FormSingular}, Headword, Headword});

    if (!Feminine.empty())
    {
        Table.Items.push_back({{FormGenderFeminine, FormSingular}, Feminine, Headword});
    }

    Table.Items.push_back({{FormGenderMasculine, FormPlural}, Plural, Headword});

    if (!Feminine.empty())
    {
        Table.Items.push_back({{FormGenderFeminine, FormPlural}, FemininePlural, Feminine});
    }

    return Table;
}

static std::optional<FTFormsTable> FindForms(const FTFormsKind& Kind, const FTFormsLanguage& Language,
                                             std::string_view TenseName, const FTDictionaryEntry& Entry)
{
    const bool bReadsEntry = Entry.PartOfSpeech == Kind.Name
        || (!Kind.SharedPartOfSpeech.empty() && Entry.PartOfSpeech == Kind.SharedPartOfSpeech);

    if (!bReadsEntry || !IsSingleWord(Entry.Headword) || !IsSupported(Kind, Language))
    {
        return std::nullopt;
    }

    if (IsVerbKind(Kind))
    {
        const FTFormsTense* Tense = FindTense(Language, TenseName);

        return Tense != nullptr ? FindVerbForms(Language, *Tense, Entry) : std::nullopt;
    }

    return Kind.Name == FormsKindAdjective ? FindAdjectiveForms(Language, Entry) : FindNounForms(Language, Entry);
}

static std::optional<size_t> FindCellIndex(const FTFormsKind& Kind, const FTFormsCell& Cell)
{
    const int PerNumber = IsVerbKind(Kind) ? PersonsPerNumber : GendersPerNumber;

    if (Cell.First < 1 || Cell.First > PerNumber || Cell.Number < FormSingular || Cell.Number > FormPlural)
    {
        return std::nullopt;
    }

    return static_cast<size_t>((Cell.Number - 1) * PerNumber + (Cell.First - 1));
}

static std::string_view GetCue(const FTFormsKind& Kind, const FTFormsLanguage& Language, size_t CellIndex)
{
    return IsVerbKind(Kind) ? Language.Pronouns[CellIndex] : GetAgreementCues(Kind, Language)[CellIndex];
}

static std::string AttachSubject(const FTFormsKind& Kind, const FTFormsLanguage& Language, size_t CellIndex,
                                 const std::string& Form)
{
    const bool bElides = IsVerbKind(Kind) && CellIndex == 0 && !Language.ElidedFirstPerson.empty()
        && StartsWithVowelSound(Form);

    if (bElides)
    {
        return std::string(Language.ElidedFirstPerson) + Form;
    }

    return std::string(GetCue(Kind, Language, CellIndex)) + " " + Form;
}

static std::string MakeItemId(const std::string& CardId, const FTFormsCell& Cell)
{
    return CardId + ItemIdSeparator + std::to_string(Cell.First) + "-" + std::to_string(Cell.Number);
}

static std::optional<FTFormsCell> ReadCell(const std::string& CardId)
{
    const std::string ItemId = GetFormsItemId(CardId);
    const size_t SeparatorAt = ItemId.rfind(ItemIdSeparator);
    constexpr size_t CellLength = 3;

    if (SeparatorAt == std::string::npos || SeparatorAt + 1 + CellLength != ItemId.size()
        || ItemId[SeparatorAt + 2] != '-')
    {
        return std::nullopt;
    }

    return FTFormsCell{ItemId[SeparatorAt + 1] - '0', ItemId[SeparatorAt + 3] - '0'};
}

static void SplitVerbEnding(const FTFormsTense& Tense, size_t CellIndex, FTWordForm& Read)
{
    std::string_view Longest;

    for (const std::string_view Ending : Tense.Endings[CellIndex])
    {
        const bool bFits = !Ending.empty() && Ending.size() > Longest.size() && Ending.size() < Read.Form.size()
            && EndsWith(Read.Form, Ending);

        if (bFits)
        {
            Longest = Ending;
        }
    }

    if (Longest.empty())
    {
        Read.Ending = Read.Form;

        return;
    }

    Read.Stem = Read.Form.substr(0, Read.Form.size() - Longest.size());
    Read.Ending = std::string(Longest);
}

static void SplitFromHeadword(const std::string& Headword, FTWordForm& Read)
{
    constexpr unsigned char ContinuationMask = 0xC0;
    constexpr unsigned char ContinuationBits = 0x80;

    const size_t SpaceAt = Read.Form.rfind(' ');
    const size_t WordAt = SpaceAt == std::string::npos ? 0 : SpaceAt + 1;
    const std::string Word = FoldCase(Read.Form.substr(WordAt));
    const std::string Base = FoldCase(Headword);

    size_t Shared = 0;

    while (Shared < Word.size() && Shared < Base.size() && Word[Shared] == Base[Shared])
    {
        ++Shared;
    }

    while (Shared > 0 && Shared < Word.size()
           && (static_cast<unsigned char>(Word[Shared]) & ContinuationMask) == ContinuationBits)
    {
        --Shared;
    }

    Read.Stem = Read.Form.substr(0, WordAt + Shared);
    Read.Ending = Read.Form.substr(WordAt + Shared);
}

std::vector<std::string> ListFormsKinds()
{
    std::vector<std::string> Result;

    for (const FTFormsKind& Kind : FormsKinds)
    {
        Result.emplace_back(Kind.Name);
    }

    return Result;
}

bool SupportsForms(const std::string& Kind, const std::string& TargetLanguage)
{
    const FTFormsKind* Found = FindKind(Kind);
    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);

    return Found != nullptr && Language != nullptr && IsSupported(*Found, *Language);
}

std::vector<std::string> ListFormsTrainings()
{
    std::vector<std::string> Result;

    for (const FTFormsTraining& Training : FormsTrainings)
    {
        Result.emplace_back(Training.Name);
    }

    return Result;
}

std::vector<std::string> ListFormsTrainingKinds(const std::string& Training)
{
    std::vector<std::string> Result;

    for (const FTFormsKind& Kind : FormsKinds)
    {
        if (Kind.Training == Training)
        {
            Result.emplace_back(Kind.Name);
        }
    }

    return Result;
}

bool SupportsFormsTraining(const std::string& Training, const std::string& TargetLanguage)
{
    const std::vector<std::string> Kinds = ListFormsTrainingKinds(Training);

    return std::any_of(Kinds.begin(), Kinds.end(), [&TargetLanguage](const std::string& Kind)
    {
        return SupportsForms(Kind, TargetLanguage);
    });
}

int GetFormsTrainingSize(const std::string& Training)
{
    const FTFormsTraining* Found = FindTraining(Training);

    return Found != nullptr ? Found->WordsPerSession : 0;
}

std::string MakeFormsTrainingId(const std::string& Training, const std::string& TargetLanguage,
                                const std::string& NativeLanguage)
{
    if (FindTraining(Training) == nullptr || TargetLanguage.empty() || NativeLanguage.empty())
    {
        return {};
    }

    return std::string(TrainingIdPrefix) + Training + "-" + TargetLanguage + "-" + NativeLanguage;
}

bool IsFormsTrainingId(const std::string& TrainingId)
{
    return TrainingId.rfind(TrainingIdPrefix, 0) == 0 && !GetFormsTraining(TrainingId).empty();
}

std::string GetFormsTraining(const std::string& CardOrTrainingId)
{
    if (const FTFormsKind* Kind = FindKindOfId(CardOrTrainingId))
    {
        return std::string(Kind->Training);
    }

    if (CardOrTrainingId.rfind(TrainingIdPrefix, 0) != 0)
    {
        return {};
    }

    const std::string_view Rest = std::string_view(CardOrTrainingId).substr(TrainingIdPrefix.size());
    const FTFormsTraining* Found = FindTraining(Rest.substr(0, Rest.find('-')));

    return Found != nullptr ? std::string(Found->Name) : std::string();
}

std::string GetFormsCardIdPrefix(const std::string& Kind)
{
    const FTFormsKind* Found = FindKind(Kind);

    return Found != nullptr ? std::string(Found->IdPrefix) : std::string();
}

std::vector<std::string> ListFormsTenses(const std::string& TargetLanguage)
{
    std::vector<std::string> Result;

    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);

    if (Language == nullptr)
    {
        return Result;
    }

    for (const FTFormsTense& Tense : Language->Tenses)
    {
        if (!Tense.Name.empty())
        {
            Result.emplace_back(Tense.Name);
        }
    }

    return Result;
}

std::string GetFormsTenseLabel(const std::string& TargetLanguage, const std::string& Tense)
{
    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);
    const FTFormsTense* Found = Language != nullptr ? FindTense(*Language, Tense) : nullptr;

    return Found != nullptr ? std::string(Found->Label) : std::string();
}

std::string GetFormsTenseRequirement(const std::string& TargetLanguage, const std::string& Tense)
{
    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);
    const FTFormsTense* Found = Language != nullptr ? FindTense(*Language, Tense) : nullptr;

    return Found != nullptr ? std::string(Found->Requires) : std::string();
}

int GetFormsTenseCode(const std::string& TargetLanguage, const std::string& Tense)
{
    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);
    const FTFormsTense* Found = Language != nullptr ? FindTense(*Language, Tense) : nullptr;

    return Found != nullptr ? Found->Tense : 0;
}

std::string GetFormsTense(const std::string& CardId)
{
    const FTFormsKind* Kind = FindKindOfId(CardId);

    return Kind != nullptr ? std::string(ReadIdTense(*Kind, CardId)) : std::string();
}

std::string GetFormsCardTenseLabel(const std::string& CardId)
{
    const FTFormsKind* Kind = FindKindOfId(CardId);

    if (Kind == nullptr || !Kind->bHasTenses)
    {
        return {};
    }

    return GetFormsTenseLabel(std::string(ReadIdLanguage(*Kind, CardId)), std::string(ReadIdTense(*Kind, CardId)));
}

std::string GetFormsKind(const std::string& CardId)
{
    const FTFormsKind* Found = FindKindOfId(CardId);

    return Found != nullptr ? std::string(Found->Name) : std::string();
}

bool IsFormsCard(const FTCard& Subject)
{
    return FindKindOfId(Subject.Id) != nullptr;
}

bool IsFormsItemId(const std::string& CardId)
{
    return FindKindOfId(CardId) != nullptr && CardId.find(ItemIdSeparator) != std::string::npos;
}

std::string GetFormsCardId(const std::string& ItemId)
{
    return ItemId.substr(0, ItemId.find(ItemIdSeparator));
}

std::string MakeFormsRoundId(const std::string& ItemId, int Round)
{
    return GetFormsItemId(ItemId) + RoundIdSeparator + std::to_string(Round);
}

std::string GetFormsItemId(const std::string& CardId)
{
    return CardId.substr(0, CardId.find(RoundIdSeparator));
}

static std::string StripSubject(const std::string& Subject, const std::string& Text)
{
    const std::string Lead = Subject + " ";

    if (Text.compare(0, Lead.size(), Lead) == 0)
    {
        return Text.substr(Lead.size());
    }

    const size_t ElisionAt = Text.find('\'');

    return ElisionAt != std::string::npos ? Text.substr(ElisionAt + 1) : std::string();
}

static std::vector<std::string> SplitSubjectLabel(const std::string& Label)
{
    constexpr char SubjectSeparator = '/';

    std::vector<std::string> Subjects;
    size_t Start = 0;

    while (Start <= Label.size())
    {
        const size_t End = std::min(Label.find(SubjectSeparator, Start), Label.size());

        Subjects.push_back(Label.substr(Start, End - Start));

        Start = End + 1;
    }

    return Subjects;
}

std::optional<FTWordForm> ReadWordForm(const FTCard& Item)
{
    const FTFormsKind* Kind = FindKindOfId(Item.Id);

    if (Kind == nullptr)
    {
        return std::nullopt;
    }

    const FTFormsLanguage* Language = FindLanguage(ReadIdLanguage(*Kind, Item.Id));
    const std::optional<FTFormsCell> Cell = ReadCell(Item.Id);
    const std::optional<size_t> CellIndex = Cell.has_value() ? FindCellIndex(*Kind, *Cell) : std::nullopt;

    if (Language == nullptr || !CellIndex.has_value() || !IsSupported(*Kind, *Language))
    {
        return std::nullopt;
    }

    FTWordForm Result;
    Result.Subject = std::string(GetCue(*Kind, *Language, *CellIndex));
    Result.SubjectLabel = IsVerbKind(*Kind) ? std::string(Language->PronounLabels[*CellIndex]) : Result.Subject;
    Result.Base = Item.Translation;
    Result.Meaning = Item.Definition;
    Result.Form = StripSubject(Result.Subject, Item.TargetWord);
    Result.Headword = ReadIdHeadword(*Kind, Item.Id);
    Result.bFeminine = !IsVerbKind(*Kind) && Cell->First == FormGenderFeminine;
    Result.bPlural = Cell->Number == FormPlural;

    if (Result.Form.empty())
    {
        return std::nullopt;
    }

    const FTFormsTense* Tense = FindTense(*Language, ReadIdTense(*Kind, Item.Id));

    if (Tense != nullptr && !Tense->bCompound)
    {
        SplitVerbEnding(*Tense, *CellIndex, Result);
    }
    else
    {
        SplitFromHeadword(ReadIdHeadword(*Kind, Item.Id), Result);
    }

    return Result;
}

std::optional<FTFormsSentenceKey> ReadFormsSentenceKey(const FTCard& Item)
{
    const FTFormsKind* Kind = FindKindOfId(Item.Id);
    const std::optional<FTFormsCell> Cell = ReadCell(Item.Id);

    if (Kind == nullptr || !Cell.has_value() || !FindCellIndex(*Kind, *Cell).has_value())
    {
        return std::nullopt;
    }

    return FTFormsSentenceKey{ReadIdHeadword(*Kind, Item.Id), std::string(Kind->Name),
                              std::string(ReadIdTense(*Kind, Item.Id)), Cell->First, Cell->Number};
}

std::vector<std::string> ListAcceptedForms(const FTCard& Item)
{
    const std::optional<FTWordForm> Read = ReadWordForm(Item);

    if (!Read.has_value())
    {
        return {Item.TargetWord};
    }

    std::vector<std::string> Accepted{Read->Form, Item.TargetWord};

    for (const std::string& Subject : SplitSubjectLabel(Read->SubjectLabel))
    {
        if (Subject != Read->Subject)
        {
            Accepted.push_back(Subject + " " + Read->Form);
        }
    }

    for (const std::string& Spelling : Item.AlternativeSpellings)
    {
        Accepted.push_back(Spelling);

        const std::string Bare = StripSubject(Read->Subject, Spelling);

        if (!Bare.empty())
        {
            Accepted.push_back(Bare);
        }
    }

    return Accepted;
}

struct FTSentenceToken
{
    QString Text;
    qsizetype Start = 0;
    qsizetype End = 0;
};

static std::vector<FTSentenceToken> SplitSentenceTokens(const QString& Text)
{
    std::vector<FTSentenceToken> Tokens;
    qsizetype Index = 0;

    while (Index < Text.size())
    {
        if (!Text[Index].isLetter())
        {
            ++Index;

            continue;
        }

        const qsizetype Start = Index;

        while (Index < Text.size() && (Text[Index].isLetter() || Text[Index].isMark()))
        {
            ++Index;
        }

        Tokens.push_back({Text.mid(Start, Index - Start).toCaseFolded(), Start, Index});
    }

    return Tokens;
}

static QStringList SplitWordList(std::string_view Words)
{
    return QString::fromUtf8(Words.data(), static_cast<qsizetype>(Words.size()))
        .toCaseFolded()
        .split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

struct FTSentenceSubjects
{
    explicit FTSentenceSubjects(const FTFormsLanguage& Language)
        : Between(SplitWordList(Language.WordsBetweenSubjectAndVerb))
    {
        for (size_t CellIndex = 0; CellIndex < VerbCellCount; ++CellIndex)
        {
            Cells[CellIndex] = SplitWordList(Language.SubjectWords[CellIndex]);
        }
    }

    std::optional<size_t> FindCell(const QString& Word) const
    {
        for (size_t CellIndex = 0; CellIndex < VerbCellCount; ++CellIndex)
        {
            if (Cells[CellIndex].contains(Word))
            {
                return CellIndex;
            }
        }

        return std::nullopt;
    }

    std::array<QStringList, VerbCellCount> Cells;
    QStringList Between;
};

static QStringView ReadSeparator(const QString& Text, const std::vector<FTSentenceToken>& Tokens, size_t Right)
{
    const qsizetype Start = Right == 0 ? 0 : Tokens[Right - 1].End;

    return QStringView(Text).mid(Start, Tokens[Right].Start - Start);
}

static bool HasClauseBreak(QStringView Separator)
{
    static const QString Breaks = QStringLiteral(".!?;:,()\"«»–—“”");

    return std::any_of(Separator.begin(), Separator.end(), [](QChar Character) { return Breaks.contains(Character); });
}

static std::optional<size_t> FindSubjectBefore(const FTSentenceSubjects& Subjects, const QString& Text,
                                               const std::vector<FTSentenceToken>& Tokens, size_t First)
{
    std::optional<size_t> Subject;
    size_t Index = First;

    for (int Step = 0; Step <= MaximumWordsBetweenSubjectAndVerb && Index > 0; ++Step)
    {
        if (HasClauseBreak(ReadSeparator(Text, Tokens, Index)))
        {
            break;
        }

        const QString& Before = Tokens[Index - 1].Text;
        const std::optional<size_t> Cell = Subjects.FindCell(Before);

        if (Cell.has_value())
        {
            Subject = Cell;
        }
        else if (!Subjects.Between.contains(Before))
        {
            break;
        }

        --Index;
    }

    return Subject;
}

static std::optional<size_t> FindSubjectAfter(const FTSentenceSubjects& Subjects, const QString& Text,
                                              const std::vector<FTSentenceToken>& Tokens, size_t Last)
{
    const auto IsJoined = [&Text, &Tokens](size_t Right)
    {
        return Right < Tokens.size() && ReadSeparator(Text, Tokens, Right).trimmed() == QStringLiteral("-");
    };

    size_t Index = Last + 1;

    if (IsJoined(Index) && Tokens[Index].Text == QStringLiteral("t"))
    {
        ++Index;
    }

    return IsJoined(Index) ? Subjects.FindCell(Tokens[Index].Text) : std::nullopt;
}

static std::optional<size_t> FindSpokenSubject(const FTSentenceSubjects& Subjects, const QString& Text,
                                               const std::vector<FTSentenceToken>& Tokens, size_t First, size_t Last)
{
    if (const std::optional<size_t> Before = FindSubjectBefore(Subjects, Text, Tokens, First))
    {
        return Before;
    }

    return FindSubjectAfter(Subjects, Text, Tokens, Last);
}

static bool IsSpokenInCell(const FTFormsLanguage& Language, const QString& Text,
                           const std::vector<FTSentenceToken>& Tokens, size_t First,
                           const std::optional<size_t>& Subject, size_t CellIndex)
{
    if (Subject.has_value())
    {
        return *Subject == CellIndex;
    }

    if (Language.bOmitsSubject)
    {
        return true;
    }

    if (First == 0 || HasClauseBreak(ReadSeparator(Text, Tokens, First)))
    {
        return false;
    }

    return CellIndex % PersonsPerNumber == PersonsPerNumber - 1;
}

static bool MatchesTokens(const std::vector<FTSentenceToken>& Tokens, size_t First,
                          const std::vector<FTSentenceToken>& Wanted)
{
    for (size_t Offset = 0; Offset < Wanted.size(); ++Offset)
    {
        if (Tokens[First + Offset].Text != Wanted[Offset].Text)
        {
            return false;
        }
    }

    return true;
}

std::optional<FTFormGap> LocateFormGap(const FTCard& Item, const FTTreeSentence& Sentence)
{
    const FTFormsKind* Kind = FindKindOfId(Item.Id);
    const std::optional<FTWordForm> Read = ReadWordForm(Item);

    if (Kind == nullptr || !Read.has_value())
    {
        return std::nullopt;
    }

    const FTFormsLanguage* Language = FindLanguage(ReadIdLanguage(*Kind, Item.Id));
    const std::optional<FTFormsCell> Cell = ReadCell(Item.Id);
    const std::optional<size_t> CellIndex = Cell.has_value() ? FindCellIndex(*Kind, *Cell) : std::nullopt;

    if (Language == nullptr || !CellIndex.has_value())
    {
        return std::nullopt;
    }

    std::vector<std::string> Spellings{Read->Form};

    for (const std::string& Alternative : Item.AlternativeSpellings)
    {
        const std::string Bare = StripSubject(Read->Subject, Alternative);

        if (!Bare.empty())
        {
            Spellings.push_back(Bare);
        }
    }

    const QString Text = QString::fromStdString(Sentence.Text);
    const std::vector<FTSentenceToken> Tokens = SplitSentenceTokens(Text);
    const bool bVerb = IsVerbKind(*Kind);

    for (const std::string& Spelling : Spellings)
    {
        const std::vector<FTSentenceToken> Wanted = SplitSentenceTokens(QString::fromStdString(Spelling));

        if (Wanted.empty() || Wanted.size() > Tokens.size())
        {
            continue;
        }

        for (size_t First = 0; First + Wanted.size() <= Tokens.size(); ++First)
        {
            const size_t Last = First + Wanted.size() - 1;

            if (!MatchesTokens(Tokens, First, Wanted))
            {
                continue;
            }

            const std::optional<size_t> Subject =
                bVerb ? FindSpokenSubject(FTSentenceSubjects(*Language), Text, Tokens, First, Last) : std::nullopt;

            if (bVerb && !IsSpokenInCell(*Language, Text, Tokens, First, Subject, *CellIndex))
            {
                continue;
            }

            FTFormGap Gap;
            Gap.Start = static_cast<int>(Tokens[First].Start);
            Gap.Length = static_cast<int>(Tokens[Last].End - Tokens[First].Start);
            Gap.bSubjectSpoken = Subject.has_value();

            return Gap;
        }
    }

    return std::nullopt;
}

bool PlaceFormGap(const FTCard& Item, FTTreeSentence& Sentence)
{
    const std::optional<FTFormGap> Gap = LocateFormGap(Item, Sentence);

    if (!Gap.has_value())
    {
        return false;
    }

    Sentence.GapStart = Gap->Start;
    Sentence.GapLength = Gap->Length;

    return true;
}

std::string GetOptionalFormsPronoun(const std::string& TargetLanguage)
{
    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);

    return Language != nullptr ? std::string(Language->OptionalPronoun) : std::string();
}

bool IsOptionalForm(const FTCard& Item)
{
    const FTFormsKind* Kind = FindKindOfId(Item.Id);

    if (Kind == nullptr || !IsVerbKind(*Kind))
    {
        return false;
    }

    const FTFormsLanguage* Language = FindLanguage(ReadIdLanguage(*Kind, Item.Id));
    const std::optional<FTFormsCell> Cell = ReadCell(Item.Id);
    const std::optional<size_t> CellIndex = Cell.has_value() ? FindCellIndex(*Kind, *Cell) : std::nullopt;

    if (Language == nullptr || !CellIndex.has_value() || Language->OptionalPronoun.empty())
    {
        return false;
    }

    return Language->Pronouns[*CellIndex] == Language->OptionalPronoun;
}

bool IsIrregularVerb(const std::string& TargetLanguage, const FTDictionaryEntry& Entry)
{
    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);

    if (Language == nullptr || Entry.PartOfSpeech != FormsKindVerb)
    {
        return false;
    }

    return Language->FirstGroupEnding.empty() ? DeviatesFromRegularModel(*Language, Entry)
                                              : BelongsToThirdGroup(*Language, Entry);
}

std::string GetVerbGroup(const std::string& TargetLanguage, const FTDictionaryEntry& Entry)
{
    if (Entry.PartOfSpeech != FormsKindVerb || Entry.Forms.empty() || FindLanguage(TargetLanguage) == nullptr)
    {
        return {};
    }

    if (IsIrregularVerb(TargetLanguage, Entry))
    {
        return TargetLanguage == "fr" ? "third" : "irregular";
    }

    const std::string Headword = FoldAccents(Entry.Headword);
    if (TargetLanguage == "fr")
    {
        return EndsWith(Headword, "er") ? "first" : "second";
    }
    if (TargetLanguage == "it")
    {
        return EndsWith(Headword, "are") ? "first" : EndsWith(Headword, "ere") ? "second" : "third";
    }
    if (TargetLanguage == "es" || TargetLanguage == "pt")
    {
        return EndsWith(Headword, "ar") ? "first" : EndsWith(Headword, "er") ? "second" : "third";
    }
    return {};
}

bool IsDerivedVerb(const std::string& TargetLanguage, const std::string& Tense, const FTDictionaryEntry& Entry,
                   const FTDictionaryEntry& Base)
{
    const FTFormsKind* Kind = FindKind(FormsKindVerb);
    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);

    const bool bExtendsBase = Entry.Headword.size() > Base.Headword.size() && EndsWith(Entry.Headword, Base.Headword);

    if (Kind == nullptr || Language == nullptr || !bExtendsBase)
    {
        return false;
    }

    const std::optional<FTFormsTable> Forms = FindForms(*Kind, *Language, Tense, Entry);
    const std::optional<FTFormsTable> BaseForms = FindForms(*Kind, *Language, Tense, Base);

    if (!Forms.has_value() || !BaseForms.has_value() || Forms->Items.size() != BaseForms->Items.size())
    {
        return false;
    }

    const std::string Prefix = Entry.Headword.substr(0, Entry.Headword.size() - Base.Headword.size());

    for (size_t Index = 0; Index < Forms->Items.size(); ++Index)
    {
        std::string Expected = BaseForms->Items[Index].Form;
        const size_t LastSpace = Expected.rfind(' ');

        Expected.insert(LastSpace == std::string::npos ? 0 : LastSpace + 1, Prefix);

        if (Forms->Items[Index].Form != Expected)
        {
            return false;
        }
    }

    return true;
}

std::string MakeFormPrompt(const std::string& Cue)
{
    return Cue + " " + std::string(PromptBlank);
}

std::optional<FTCard> MakeFormsCard(const std::string& Kind, const FTDictionaryEntry& Entry,
                                    const std::string& TargetLanguage, const std::string& Translation,
                                    const std::string& Tense)
{
    const FTFormsKind* Found = FindKind(Kind);
    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);

    if (Found == nullptr || Language == nullptr)
    {
        return std::nullopt;
    }

    const std::optional<FTFormsTable> Table = FindForms(*Found, *Language, Tense, Entry);

    if (!Table.has_value() || Table->bRegular)
    {
        return std::nullopt;
    }

    FTCard Card;
    Card.Id = std::string(Found->IdPrefix) + "-" + TargetLanguage + "-" + Entry.Headword
        + (Found->bHasTenses ? "-" + Tense : std::string());
    Card.TargetWord = Entry.Headword;
    Card.Translation = Translation;

    return Card;
}

std::vector<FTCard> ExpandFormsCard(const FTCard& Stored, const FTDictionaryEntry& Entry,
                                    const std::string& TargetLanguage)
{
    const FTFormsKind* Kind = FindKindOfId(Stored.Id);
    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);

    if (Kind == nullptr || Language == nullptr)
    {
        return {};
    }

    const std::optional<FTFormsTable> Table = FindForms(*Kind, *Language, ReadIdTense(*Kind, Stored.Id), Entry);

    if (!Table.has_value())
    {
        return {};
    }

    std::vector<FTCard> Result;
    Result.reserve(Table->Items.size());

    for (const FTFormsItem& Item : Table->Items)
    {
        const std::optional<size_t> CellIndex = FindCellIndex(*Kind, Item.Cell);

        if (!CellIndex.has_value())
        {
            return {};
        }

        FTCard Form;
        Form.Id = MakeItemId(Stored.Id, Item.Cell);
        Form.TargetWord = AttachSubject(*Kind, *Language, *CellIndex, Item.Form);
        Form.Translation = Item.Base;
        Form.Definition = Stored.Translation;
        Form.LeitnerStage = Stored.LeitnerStage;

        for (const std::string& Alternative : Item.Alternatives)
        {
            Form.AlternativeSpellings.push_back(AttachSubject(*Kind, *Language, *CellIndex, Alternative));
        }

        Result.push_back(std::move(Form));
    }

    return Result;
}

std::vector<std::vector<int>> GroupCardsByWord(const std::vector<FTCardHandle>& Cards)
{
    std::vector<std::vector<int>> Groups;
    std::unordered_map<std::string, size_t> GroupByWord;

    for (int Index = 0; Index < static_cast<int>(Cards.size()); ++Index)
    {
        const std::string Key = GetFormsCardId(Cards[static_cast<size_t>(Index)]->Id);

        const auto [Found, bInserted] = GroupByWord.try_emplace(Key, Groups.size());

        if (bInserted)
        {
            Groups.emplace_back();
        }

        Groups[Found->second].push_back(Index);
    }

    return Groups;
}

std::vector<FTCardHandle> TakeFormsSession(const std::string& Training, const std::vector<FTCardHandle>& Cards)
{
    const size_t Count = std::min(Cards.size(), static_cast<size_t>(GetFormsTrainingSize(Training)));

    return std::vector<FTCardHandle>(Cards.begin(), Cards.begin() + static_cast<std::ptrdiff_t>(Count));
}

std::vector<std::string> ListFormsCues(const std::string& Kind, const std::string& TargetLanguage)
{
    const FTFormsKind* Found = FindKind(Kind);
    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);

    if (Found == nullptr || Language == nullptr || !IsSupported(*Found, *Language))
    {
        return {};
    }

    if (IsVerbKind(*Found))
    {
        return {Language->PronounLabels.begin(), Language->PronounLabels.end()};
    }

    const std::array<std::string_view, AgreementCellCount>& Cues = GetAgreementCues(*Found, *Language);

    return {Cues.begin(), Cues.end()};
}

std::string FindFormEnding(const std::string& TargetLanguage, const std::string& Headword, int Mood, int Tense,
                           int CellIndex, const std::string& Form)
{
    FTWordForm Read;
    Read.Form = Form;

    const FTFormsLanguage* Language = FindLanguage(TargetLanguage);

    if (Language != nullptr && Mood == MoodIndicative && CellIndex >= 0 && CellIndex < VerbCellCount)
    {
        for (const FTFormsTense& Candidate : Language->Tenses)
        {
            if (!Candidate.Name.empty() && Candidate.Tense == Tense && !Candidate.bCompound)
            {
                SplitVerbEnding(Candidate, static_cast<size_t>(CellIndex), Read);

                return Read.Ending;
            }
        }
    }

    if (Mood != 0)
    {
        return {};
    }

    SplitFromHeadword(Headword, Read);

    return Read.Ending;
}
