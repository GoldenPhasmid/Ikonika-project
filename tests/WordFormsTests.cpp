#include <catch2/catch_test_macros.hpp>

#include "core/FTDatabase.h"
#include "core/FTDictionaryPackage.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTSentenceCards.h"
#include "core/FTWordForms.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

static constexpr int MoodIndicative = 1;
static constexpr int TensePresent = 1;
static constexpr int TenseImperfect = 2;

static FTDictionaryForm MakeForm(const std::string& Text, int Person, int Number, int Tense = TensePresent)
{
    FTDictionaryForm Form;
    Form.Text = Text;
    Form.Mood = MoodIndicative;
    Form.Tense = Tense;
    Form.Person = Person;
    Form.Number = Number;

    return Form;
}

static FTDictionaryEntry MakeVerb(const std::string& Headword, const std::array<std::string, 6>& PresentForms)
{
    FTDictionaryEntry Verb;
    Verb.Headword = Headword;
    Verb.PartOfSpeech = "verb";

    for (size_t Index = 0; Index < PresentForms.size(); ++Index)
    {
        const int Person = static_cast<int>(Index % 3) + 1;
        const int Number = static_cast<int>(Index / 3) + 1;

        Verb.Forms.push_back(MakeForm(PresentForms[Index], Person, Number));
    }

    return Verb;
}

static FTDictionaryEntry MakeVouloir()
{
    return MakeVerb("vouloir", {"veux", "veux", "veut", "voulons", "voulez", "veulent"});
}

static FTDictionaryEntry MakeAimer()
{
    return MakeVerb("aimer", {"aime", "aimes", "aime", "aimons", "aimez", "aiment"});
}

static std::vector<FTCard> ExpandVerb(const FTDictionaryEntry& Verb, const std::string& Language = "fr")
{
    const std::optional<FTCard> Stored = MakeFormsCard(FormsKindVerb, Verb, Language, "translation");
    REQUIRE(Stored.has_value());

    return ExpandFormsCard(*Stored, Verb, Language);
}

static FTCardHandle MakeStoredHandle(const std::string& Verb, const std::string& Tense = "present")
{
    FTCard Card;
    Card.Id = "conjugation-fr-" + Verb + "-" + Tense;
    Card.TargetWord = Verb;

    return MakeCardHandle(std::move(Card));
}

TEST_CASE("A verb with six present forms becomes one stored card", "[conjugation]")
{
    const std::optional<FTCard> Card = MakeFormsCard(FormsKindVerb, MakeVouloir(), "fr", "want");

    REQUIRE(Card.has_value());
    CHECK(Card->Id == "conjugation-fr-vouloir-present");
    CHECK(Card->TargetWord == "vouloir");
    CHECK(Card->Translation == "want");
    CHECK(Card->LeitnerStage == PoolStage);

    CHECK(IsFormsCard(*Card));
    CHECK_FALSE(IsFormsItemId(Card->Id));
}

TEST_CASE("The stored card id is the same every time and differs per language", "[conjugation]")
{
    const FTDictionaryEntry Verb = MakeVerb("amar", {"amo", "amas", "ama", "amamos", "amáis", "aman"});

    const std::optional<FTCard> First = MakeFormsCard(FormsKindVerb, Verb, "es", "love");
    const std::optional<FTCard> Again = MakeFormsCard(FormsKindVerb, Verb, "es", "like");

    REQUIRE(First.has_value());
    REQUIRE(Again.has_value());
    CHECK(First->Id == Again->Id);
    CHECK(First->Id == "conjugation-es-amar-present");
}

static std::vector<std::string> ReadShownForms(const std::vector<FTCard>& Items)
{
    std::vector<std::string> Shown;

    for (const FTCard& Item : Items)
    {
        Shown.push_back(Item.TargetWord);
    }

    return Shown;
}

static bool Accepts(const FTCard& Item, const std::string& Typed)
{
    const std::vector<std::string> Accepted = ListAcceptedForms(Item);

    return std::find(Accepted.begin(), Accepted.end(), Typed) != Accepted.end();
}

static FTDictionaryEntry MakePayer()
{
    FTDictionaryEntry Payer = MakeVerb("payer", {"paye", "payes", "paye", "payons", "payez", "payent"});
    Payer.Forms.push_back(MakeForm("paie", 1, 1));
    Payer.Forms.push_back(MakeForm("paies", 2, 1));
    Payer.Forms.push_back(MakeForm("paie", 3, 1));
    Payer.Forms.push_back(MakeForm("paient", 3, 2));

    return Payer;
}

TEST_CASE("A verb with two spellings for a person shows the one furthest from the infinitive",
          "[conjugation][spellings]")
{
    const FTDictionaryEntry Payer = MakePayer();

    REQUIRE(MakeFormsCard(FormsKindVerb, Payer, "fr", "pay").has_value());

    const std::vector<FTCard> Items = ExpandVerb(Payer);
    REQUIRE(Items.size() == 6);

    CHECK(ReadShownForms(Items)
          == std::vector<std::string>{"je paie", "tu paies", "il paie", "nous payons", "vous payez", "ils paient"});

    CHECK(Items[0].AlternativeSpellings == std::vector<std::string>{"je paye"});
    CHECK(Items[1].AlternativeSpellings == std::vector<std::string>{"tu payes"});
    CHECK(Items[3].AlternativeSpellings.empty());
    CHECK(Items[5].AlternativeSpellings == std::vector<std::string>{"ils payent"});
}

TEST_CASE("Either spelling of a form is accepted when typed, with or without the pronoun",
          "[conjugation][spellings]")
{
    const std::vector<FTCard> Items = ExpandVerb(MakePayer());
    REQUIRE(Items.size() == 6);

    CHECK(Accepts(Items[0], "paie"));
    CHECK(Accepts(Items[0], "je paie"));
    CHECK(Accepts(Items[0], "paye"));
    CHECK(Accepts(Items[0], "je paye"));
    CHECK_FALSE(Accepts(Items[0], "payes"));

    CHECK(Accepts(Items[3], "payons"));
    CHECK(Accepts(Items[3], "nous payons"));
    CHECK(ListAcceptedForms(Items[3]).size() == 2);

    FTDictionaryEntry Essayer = MakeVerb("essayer", {"essaye", "essayes", "essaye", "essayons", "essayez", "essayent"});
    Essayer.Forms.push_back(MakeForm("essaie", 1, 1));

    const std::vector<FTCard> Elided = ExpandVerb(Essayer);
    REQUIRE(Elided.size() == 6);

    CHECK(Elided[0].TargetWord == "j'essaie");
    CHECK(Elided[0].AlternativeSpellings == std::vector<std::string>{"j'essaye"});
    CHECK(Accepts(Elided[0], "essaye"));
    CHECK(Accepts(Elided[0], "j'essaye"));
    CHECK(Accepts(Elided[0], "essaie"));

    FTCard Plain;
    Plain.TargetWord = "chien";

    CHECK(ListAcceptedForms(Plain) == std::vector<std::string>{"chien"});
}

TEST_CASE("The other persons follow the spelling chosen for the first", "[conjugation][spellings]")
{
    FTDictionaryEntry Asseoir =
        MakeVerb("asseoir", {"assieds", "assieds", "assied", "asseyons", "asseyez", "asseyent"});
    Asseoir.Forms.push_back(MakeForm("assois", 1, 1));
    Asseoir.Forms.push_back(MakeForm("assois", 2, 1));
    Asseoir.Forms.push_back(MakeForm("assoit", 3, 1));
    Asseoir.Forms.push_back(MakeForm("assoyons", 1, 2));
    Asseoir.Forms.push_back(MakeForm("assoyez", 2, 2));
    Asseoir.Forms.push_back(MakeForm("assoient", 3, 2));

    CHECK(ReadShownForms(ExpandVerb(Asseoir))
          == std::vector<std::string>{"j'assieds", "tu assieds", "il assied", "nous asseyons", "vous asseyez",
                                      "ils asseyent"});

    FTDictionaryEntry Esperer;
    Esperer.Headword = "espérer";
    Esperer.PartOfSpeech = "verb";

    const std::array<std::string, 6> Future{"espérerai", "espéreras", "espérera", "espérerons", "espérerez",
                                            "espéreront"};

    for (size_t Index = 0; Index < Future.size(); ++Index)
    {
        FTDictionaryForm Form = MakeForm(Future[Index], static_cast<int>(Index % 3) + 1, static_cast<int>(Index / 3) + 1);
        Form.Tense = 4;
        Esperer.Forms.push_back(Form);
    }

    FTDictionaryForm Reformed = MakeForm("espèreras", 2, 1);
    Reformed.Tense = 4;
    Esperer.Forms.push_back(Reformed);

    FTCard Stored;
    Stored.Id = "conjugation-fr-espérer-future";
    Stored.TargetWord = "espérer";

    const std::vector<FTCard> Items = ExpandFormsCard(Stored, Esperer, "fr");
    REQUIRE(Items.size() == 6);

    CHECK(Items[0].TargetWord == "j'espérerai");
    CHECK(Items[1].TargetWord == "tu espéreras");
    CHECK(Items[1].AlternativeSpellings == std::vector<std::string>{"tu espèreras"});
}

TEST_CASE("The same spelling listed twice for one person still counts as one form", "[conjugation]")
{
    FTDictionaryEntry Verb = MakeVouloir();
    Verb.Forms.push_back(MakeForm("veux", 1, 1));

    CHECK(MakeFormsCard(FormsKindVerb, Verb, "fr", "want").has_value());
}

TEST_CASE("A verb missing a person is not offered", "[conjugation]")
{
    FTDictionaryEntry Falloir;
    Falloir.Headword = "falloir";
    Falloir.PartOfSpeech = "verb";
    Falloir.Forms.push_back(MakeForm("faut", 3, 1));

    CHECK_FALSE(MakeFormsCard(FormsKindVerb, Falloir, "fr", "must").has_value());
}

TEST_CASE("Forms of another tense, gender or word count do not disturb the present tense", "[conjugation]")
{
    FTDictionaryEntry Verb = MakeVouloir();
    Verb.Forms.push_back(MakeForm("voulais", 1, 1, TenseImperfect));
    Verb.Forms.push_back(MakeForm("ai voulu", 1, 1));

    FTDictionaryForm Gendered = MakeForm("voulue", 1, 1);
    Gendered.Gender = 2;
    Verb.Forms.push_back(Gendered);

    FTDictionaryForm Subjunctive = MakeForm("veuille", 1, 1);
    Subjunctive.Mood = 2;
    Verb.Forms.push_back(Subjunctive);

    const std::vector<FTCard> Forms = ExpandVerb(Verb);

    REQUIRE(Forms.size() == 6);
    CHECK(Forms[0].TargetWord == "je veux");
}

TEST_CASE("Only a single-word verb of a supported language is offered", "[conjugation]")
{
    FTDictionaryEntry Noun = MakeVouloir();
    Noun.PartOfSpeech = "noun";

    FTDictionaryEntry Phrase = MakeVouloir();
    Phrase.Headword = "en vouloir";

    CHECK_FALSE(MakeFormsCard(FormsKindVerb, Noun, "fr", "want").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindVerb, Phrase, "fr", "want").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindVerb, MakeVouloir(), "de", "want").has_value());

    CHECK(SupportsForms(FormsKindVerb, "fr"));
    CHECK(SupportsForms(FormsKindVerb, "es"));
    CHECK_FALSE(SupportsForms(FormsKindVerb, "de"));
}

TEST_CASE("A stored card expands into six forms in person order", "[conjugation]")
{
    const FTDictionaryEntry Verb = MakeVouloir();

    FTCard Stored = *MakeFormsCard(FormsKindVerb, Verb, "fr", "want");
    Stored.LeitnerStage = 2;

    const std::vector<FTCard> Forms = ExpandFormsCard(Stored, Verb, "fr");

    REQUIRE(Forms.size() == 6);

    const std::array<std::string, 6> Words{"je veux", "tu veux", "il veut", "nous voulons", "vous voulez",
                                           "ils veulent"};
    const std::array<std::string, 6> Cells{"#1-1", "#2-1", "#3-1", "#1-2", "#2-2", "#3-2"};

    for (size_t Index = 0; Index < Forms.size(); ++Index)
    {
        CHECK(Forms[Index].TargetWord == Words[Index]);
        CHECK(Forms[Index].Id == Stored.Id + Cells[Index]);
        CHECK(Forms[Index].Translation == "vouloir");
        CHECK(Forms[Index].LeitnerStage == 2);
        CHECK(IsFormsItemId(Forms[Index].Id));
        CHECK(GetFormsCardId(Forms[Index].Id) == Stored.Id);
    }
}

TEST_CASE("The French first person elides before a vowel sound", "[conjugation]")
{
    CHECK(ExpandVerb(MakeAimer())[0].TargetWord == "j'aime");
    CHECK(ExpandVerb(MakeVerb("habiter", {"habite", "habites", "habite", "habitons", "habitez", "habitent"}))[0]
              .TargetWord == "j'habite");
    CHECK(ExpandVerb(MakeVerb("écouter", {"écoute", "écoutes", "écoute", "écoutons", "écoutez", "écoutent"}))[0]
              .TargetWord == "j'écoute");
    CHECK(ExpandVerb(MakeVouloir())[0].TargetWord == "je veux");
}

TEST_CASE("Spanish keeps its pronoun whole before a vowel", "[conjugation]")
{
    const std::vector<FTCard> Forms = ExpandVerb(MakeVerb("amar", {"amo", "amas", "ama", "amamos", "amáis", "aman"}),
                                                 "es");

    REQUIRE(Forms.size() == 6);
    CHECK(Forms[0].TargetWord == "yo amo");
    CHECK(Forms[3].TargetWord == "nosotros amamos");
}

TEST_CASE("A form reads back as subject, verb, stem and ending", "[conjugation]")
{
    const std::vector<FTCard> Forms = ExpandVerb(MakeVouloir());

    const std::optional<FTWordForm> First = ReadWordForm(Forms[0]);
    REQUIRE(First.has_value());
    CHECK(First->Subject == "je");
    CHECK(First->Base == "vouloir");
    CHECK(First->Form == "veux");
    CHECK(First->Stem == "veu");
    CHECK(First->Ending == "x");

    const std::optional<FTWordForm> Fourth = ReadWordForm(Forms[3]);
    REQUIRE(Fourth.has_value());
    CHECK(Fourth->Subject == "nous");
    CHECK(Fourth->Stem == "voul");
    CHECK(Fourth->Ending == "ons");

    const std::optional<FTWordForm> Sixth = ReadWordForm(Forms[5]);
    REQUIRE(Sixth.has_value());
    CHECK(Sixth->Subject == "ils");
    CHECK(Sixth->Stem == "veul");
    CHECK(Sixth->Ending == "ent");
}

TEST_CASE("An elided form reads back with the full pronoun", "[conjugation]")
{
    const std::optional<FTWordForm> Read = ReadWordForm(ExpandVerb(MakeAimer())[0]);

    REQUIRE(Read.has_value());
    CHECK(Read->Subject == "je");
    CHECK(Read->Form == "aime");
    CHECK(Read->Stem == "aim");
    CHECK(Read->Ending == "e");
}

TEST_CASE("A third person is labelled with both genders and accepts either pronoun", "[conjugation]")
{
    const std::vector<FTCard> Forms = ExpandVerb(MakeVouloir());

    const std::optional<FTWordForm> Singular = ReadWordForm(Forms[2]);
    REQUIRE(Singular.has_value());
    CHECK(Singular->Subject == "il");
    CHECK(Singular->SubjectLabel == "il/elle");

    const std::optional<FTWordForm> Plural = ReadWordForm(Forms[5]);
    REQUIRE(Plural.has_value());
    CHECK(Plural->SubjectLabel == "ils/elles");

    const std::optional<FTWordForm> First = ReadWordForm(Forms[0]);
    REQUIRE(First.has_value());
    CHECK(First->SubjectLabel == "je");

    const std::vector<std::string> Accepted = ListAcceptedForms(Forms[2]);

    CHECK(std::find(Accepted.begin(), Accepted.end(), "il veut") != Accepted.end());
    CHECK(std::find(Accepted.begin(), Accepted.end(), "elle veut") != Accepted.end());
    CHECK(std::find(Accepted.begin(), Accepted.end(), "veut") != Accepted.end());
}

TEST_CASE("A form with no standard ending is marked whole", "[conjugation]")
{
    const FTDictionaryEntry Faire = MakeVerb("faire", {"fais", "fais", "fait", "faisons", "faites", "font"});
    const std::vector<FTCard> Forms = ExpandVerb(Faire);

    const std::optional<FTWordForm> You = ReadWordForm(Forms[4]);
    REQUIRE(You.has_value());
    CHECK(You->Stem.empty());
    CHECK(You->Ending == "faites");

    const std::optional<FTWordForm> They = ReadWordForm(Forms[5]);
    REQUIRE(They.has_value());
    CHECK(They->Stem.empty());
    CHECK(They->Ending == "font");
}

TEST_CASE("A form as short as its ending is marked whole", "[conjugation]")
{
    const FTDictionaryEntry Verb = MakeVerb("savoir", {"s", "sais", "sait", "savons", "savez", "savent"});

    const std::optional<FTWordForm> Read = ReadWordForm(ExpandVerb(Verb)[0]);

    REQUIRE(Read.has_value());
    CHECK(Read->Stem.empty());
    CHECK(Read->Ending == "s");
}

TEST_CASE("The prompt blanks the form and names the verb", "[conjugation]")
{
    const std::optional<FTWordForm> Read = ReadWordForm(ExpandVerb(MakeVouloir())[0]);
    REQUIRE(Read.has_value());

    CHECK(MakeFormPrompt(Read->Subject) == "je ___");
}

TEST_CASE("A stored card and an ordinary card do not read as a form", "[conjugation]")
{
    const std::optional<FTCard> Stored = MakeFormsCard(FormsKindVerb, MakeVouloir(), "fr", "want");
    REQUIRE(Stored.has_value());

    FTCard Word;
    Word.Id = "some-card";
    Word.TargetWord = "le chien";
    Word.Translation = "dog";

    CHECK_FALSE(ReadWordForm(*Stored).has_value());
    CHECK_FALSE(ReadWordForm(Word).has_value());
    CHECK_FALSE(IsFormsCard(Word));
    CHECK_FALSE(IsFormsItemId(Word.Id));
}

TEST_CASE("Session forms group by verb in the order the verbs came", "[conjugation]")
{
    std::vector<FTCardHandle> Cards;

    for (const FTDictionaryEntry& Verb : {MakeVouloir(), MakeAimer()})
    {
        for (FTCard& Form : ExpandVerb(Verb))
        {
            Cards.push_back(MakeCardHandle(std::move(Form)));
        }
    }

    const std::vector<std::vector<int>> Groups = GroupCardsByWord(Cards);

    REQUIRE(Groups.size() == 2);
    CHECK(Groups[0] == std::vector<int>{0, 1, 2, 3, 4, 5});
    CHECK(Groups[1] == std::vector<int>{6, 7, 8, 9, 10, 11});
}

TEST_CASE("A card that is not a form gets a group of its own", "[conjugation]")
{
    FTCard Word;
    Word.Id = "some-card";
    Word.TargetWord = "le chien";

    FTCard Other = Word;
    Other.Id = "other-card";

    const std::vector<std::vector<int>> Groups =
        GroupCardsByWord({MakeCardHandle(std::move(Word)), MakeCardHandle(std::move(Other))});

    REQUIRE(Groups.size() == 2);
    CHECK(Groups[0] == std::vector<int>{0});
    CHECK(Groups[1] == std::vector<int>{1});
}

TEST_CASE("A training session takes the first four verbs", "[conjugation]")
{
    std::vector<FTCardHandle> Cards;

    for (const char* Verb : {"vouloir", "prendre", "dire", "faire", "voir", "savoir"})
    {
        Cards.push_back(MakeStoredHandle(Verb));
    }

    const std::vector<FTCardHandle> Taken = TakeFormsSession(FormsTrainingConjugation, Cards);

    REQUIRE(Taken.size() == static_cast<size_t>(GetFormsTrainingSize(FormsTrainingConjugation)));
    CHECK(Taken.front()->TargetWord == "vouloir");
    CHECK(Taken.back()->TargetWord == "faire");

    CHECK(TakeFormsSession(FormsTrainingConjugation, {Cards[0], Cards[1]}).size() == 2);
    CHECK(TakeFormsSession(FormsTrainingConjugation, {}).empty());
}

static FTDictionaryForm MakeAgreementForm(const std::string& Text, int Gender, int Number)
{
    FTDictionaryForm Form;
    Form.Text = Text;
    Form.Gender = Gender;
    Form.Number = Number;

    return Form;
}

static FTDictionaryEntry MakeAdjective(const std::string& Headword, const std::string& Feminine,
                                       const std::string& Plural, const std::string& FemininePlural)
{
    FTDictionaryEntry Adjective;
    Adjective.Headword = Headword;
    Adjective.PartOfSpeech = "adj";

    if (!Feminine.empty())
    {
        Adjective.Forms.push_back(MakeAgreementForm(Feminine, 2, 1));
    }

    if (!Plural.empty())
    {
        Adjective.Forms.push_back(MakeAgreementForm(Plural, 1, 2));
    }

    if (!FemininePlural.empty())
    {
        Adjective.Forms.push_back(MakeAgreementForm(FemininePlural, 2, 2));
    }

    return Adjective;
}

static FTDictionaryEntry MakeNoun(const std::string& Headword, int Gender, const std::string& Plural)
{
    FTDictionarySense Sense;
    Sense.Gender = Gender;

    FTDictionaryEntry Noun;
    Noun.Headword = Headword;
    Noun.PartOfSpeech = "noun";
    Noun.Senses.push_back(Sense);

    if (!Plural.empty())
    {
        Noun.Forms.push_back(MakeAgreementForm(Plural, 0, 2));
        Noun.Forms.push_back(MakeAgreementForm(Plural, Gender == GenderFeminine ? 2 : 1, 2));
    }

    return Noun;
}

static FTDictionaryEntry MakeBeau()
{
    return MakeAdjective("beau", "belle", "beaux", "belles");
}

static FTDictionaryEntry MakeVendeur()
{
    FTDictionaryEntry Vendeur = MakeNoun("vendeur", GenderMasculine, "vendeurs");
    Vendeur.Forms.push_back(MakeAgreementForm("vendeuse", 2, 1));

    return Vendeur;
}

static std::vector<FTCard> ExpandWord(const std::string& Kind, const FTDictionaryEntry& Entry)
{
    const std::optional<FTCard> Stored = MakeFormsCard(Kind, Entry, "fr", "translation");
    REQUIRE(Stored.has_value());

    return ExpandFormsCard(*Stored, Entry, "fr");
}

static std::vector<std::string> ReadTargetWords(const std::vector<FTCard>& Forms)
{
    std::vector<std::string> Words;

    for (const FTCard& Form : Forms)
    {
        Words.push_back(Form.TargetWord);
    }

    return Words;
}

TEST_CASE("Each kind of forms card has its own ids and belongs to one of two trainings", "[conjugation][forms]")
{
    CHECK(ListFormsKinds() == std::vector<std::string>{"verb", "adj", "noun"});
    CHECK(ListFormsTrainings() == std::vector<std::string>{"conjugation", "forms"});

    CHECK(ListFormsTrainingKinds(FormsTrainingConjugation) == std::vector<std::string>{"verb"});
    CHECK(ListFormsTrainingKinds(FormsTrainingAgreement) == std::vector<std::string>{"adj", "noun"});
    CHECK(ListFormsTrainingKinds("adverbs").empty());

    CHECK(GetFormsTrainingSize(FormsTrainingConjugation) == 4);
    CHECK(GetFormsTrainingSize(FormsTrainingAgreement) == 8);
    CHECK(GetFormsTrainingSize("adverbs") == 0);

    CHECK(MakeFormsTrainingId(FormsTrainingConjugation, "fr", "en") == "training-conjugation-fr-en");
    CHECK(MakeFormsTrainingId(FormsTrainingAgreement, "fr", "en") == "training-forms-fr-en");
    CHECK(MakeFormsTrainingId(FormsTrainingAgreement, "", "en").empty());
    CHECK(MakeFormsTrainingId("adverbs", "fr", "en").empty());

    CHECK(IsFormsTrainingId("training-conjugation-fr-en"));
    CHECK(IsFormsTrainingId("training-forms-es-en"));
    CHECK_FALSE(IsFormsTrainingId("learning-pool-fr-en"));
    CHECK_FALSE(IsFormsTrainingId("training-adverbs-fr-en"));
    CHECK_FALSE(IsFormsTrainingId("conjugation-fr-vouloir-present"));
    CHECK_FALSE(IsFormsTrainingId(""));

    CHECK(GetFormsTraining("training-forms-fr-en") == "forms");
    CHECK(GetFormsTraining("forms-adj-fr-beau#2-1") == "forms");
    CHECK(GetFormsTraining("forms-noun-fr-cheval") == "forms");
    CHECK(GetFormsTraining("conjugation-fr-vouloir-present") == "conjugation");
    CHECK(GetFormsTraining("some-card").empty());

    CHECK(GetFormsKind("forms-noun-fr-cheval") == "noun");
    CHECK(GetFormsKind("forms-adj-fr-beau#2-1") == "adj");
    CHECK(GetFormsKind("conjugation-fr-vouloir-present") == "verb");
    CHECK(GetFormsKind("training-conjugation-fr-en").empty());
    CHECK(GetFormsKind("some-card").empty());

    CHECK(GetFormsCardIdPrefix(FormsKindVerb) == "conjugation");
    CHECK(GetFormsCardIdPrefix("adv").empty());

    CHECK(SupportsFormsTraining(FormsTrainingConjugation, "fr"));
    CHECK(SupportsFormsTraining(FormsTrainingAgreement, "es"));
    CHECK_FALSE(SupportsFormsTraining(FormsTrainingAgreement, "de"));
    CHECK_FALSE(SupportsFormsTraining("adverbs", "fr"));

    CHECK(SupportsForms(FormsKindAdjective, "fr"));
    CHECK(SupportsForms(FormsKindNoun, "fr"));
    CHECK(SupportsForms(FormsKindVerb, "es"));
    CHECK(SupportsForms(FormsKindAdjective, "es"));
    CHECK(SupportsForms(FormsKindNoun, "es"));
    CHECK_FALSE(SupportsForms(FormsKindNoun, "de"));
    CHECK_FALSE(SupportsForms(FormsKindAdjective, "de"));
}

TEST_CASE("An adjective with an irregular feminine becomes one stored card", "[conjugation][forms]")
{
    const std::optional<FTCard> Card = MakeFormsCard(FormsKindAdjective, MakeBeau(), "fr", "beautiful");

    REQUIRE(Card.has_value());
    CHECK(Card->Id == "forms-adj-fr-beau");
    CHECK(Card->TargetWord == "beau");
    CHECK(Card->Translation == "beautiful");
    CHECK(IsFormsCard(*Card));
    CHECK_FALSE(IsFormsItemId(Card->Id));

    CHECK_FALSE(MakeFormsCard(FormsKindVerb, MakeBeau(), "fr", "beautiful").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeBeau(), "fr", "beautiful").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, MakeBeau(), "de", "beautiful").has_value());
}

TEST_CASE("An adjective expands into four forms: masculine, feminine, then their plurals", "[conjugation][forms]")
{
    const std::vector<FTCard> Forms = ExpandWord(FormsKindAdjective, MakeBeau());

    REQUIRE(Forms.size() == 4);
    CHECK(ReadTargetWords(Forms) == std::vector<std::string>{"il beau", "elle belle", "ils beaux",
                                                             "elles belles"});

    CHECK(Forms[0].Id == "forms-adj-fr-beau#1-1");
    CHECK(Forms[1].Id == "forms-adj-fr-beau#2-1");
    CHECK(Forms[2].Id == "forms-adj-fr-beau#1-2");
    CHECK(Forms[3].Id == "forms-adj-fr-beau#2-2");

    for (const FTCard& Form : Forms)
    {
        CHECK(Form.Translation == "beau");
        CHECK(GetFormsCardId(Form.Id) == "forms-adj-fr-beau");
    }
}

static FTCard MakeStoredAdjective(const std::string& Headword)
{
    FTCard Stored;
    Stored.Id = "forms-adj-fr-" + Headword;
    Stored.TargetWord = Headword;

    return Stored;
}

TEST_CASE("An adjective card reads its forms from a determiner entry, whose plural serves both genders",
          "[conjugation][forms]")
{
    FTDictionaryEntry Mon;
    Mon.Headword = "mon";
    Mon.PartOfSpeech = "det";
    Mon.Forms.push_back(MakeAgreementForm("ma", 2, 1));
    Mon.Forms.push_back(MakeAgreementForm("mes", 0, 2));

    CHECK(ReadTargetWords(ExpandFormsCard(MakeStoredAdjective("mon"), Mon, "fr"))
          == std::vector<std::string>{"il mon", "elle ma", "ils mes", "elles mes"});

    FTDictionaryEntry Tout;
    Tout.Headword = "tout";
    Tout.PartOfSpeech = "det";
    Tout.Forms.push_back(MakeAgreementForm("tous", 1, 2));
    Tout.Forms.push_back(MakeAgreementForm("toute", 2, 1));
    Tout.Forms.push_back(MakeAgreementForm("toutes", 2, 2));

    CHECK(ReadTargetWords(ExpandFormsCard(MakeStoredAdjective("tout"), Tout, "fr"))
          == std::vector<std::string>{"il tout", "elle toute", "ils tous", "elles toutes"});
}

TEST_CASE("An adjective's genderless plural does not stand in for its feminine plural", "[conjugation][forms]")
{
    FTDictionaryEntry Beau = MakeAdjective("beau", "belle", "", "");
    Beau.Forms.push_back(MakeAgreementForm("beaux", 0, 2));

    CHECK(ReadTargetWords(ExpandFormsCard(MakeStoredAdjective("beau"), Beau, "fr"))
          == std::vector<std::string>{"il beau", "elle belle", "ils beaux", "elles belles"});
}

TEST_CASE("A noun card does not read a determiner entry", "[conjugation][forms]")
{
    FTDictionaryEntry Mon;
    Mon.Headword = "mon";
    Mon.PartOfSpeech = "det";
    Mon.Forms.push_back(MakeAgreementForm("ma", 2, 1));
    Mon.Forms.push_back(MakeAgreementForm("mes", 0, 2));

    FTCard Stored;
    Stored.Id = "forms-noun-fr-mon";
    Stored.TargetWord = "mon";

    CHECK(ExpandFormsCard(Stored, Mon, "fr").empty());
}

TEST_CASE("A regular adjective is not offered but still expands once stored", "[conjugation][forms]")
{
    const FTDictionaryEntry Grand = MakeAdjective("grand", "grande", "grands", "grandes");

    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, Grand, "fr", "big").has_value());

    FTCard Stored;
    Stored.Id = "forms-adj-fr-grand";
    Stored.TargetWord = "grand";

    CHECK(ExpandFormsCard(Stored, Grand, "fr").size() == 4);
}

TEST_CASE("An adjective regular in the feminine but not in the plural is offered", "[conjugation][forms]")
{
    const FTDictionaryEntry Normal = MakeAdjective("normal", "normale", "normaux", "normales");

    CHECK(MakeFormsCard(FormsKindAdjective, Normal, "fr", "normal").has_value());
}

TEST_CASE("An adjective that does not change for gender is not offered", "[conjugation][forms]")
{
    FTDictionaryEntry Rouge = MakeAdjective("rouge", "", "", "");
    Rouge.Forms.push_back(MakeAgreementForm("rouges", 0, 2));

    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, Rouge, "fr", "red").has_value());
}

TEST_CASE("An adjective ending in s or x keeps its spelling in the masculine plural", "[conjugation][forms]")
{
    const std::vector<FTCard> Forms = ExpandWord(FormsKindAdjective,
                                                 MakeAdjective("heureux", "heureuse", "", "heureuses"));

    CHECK(ReadTargetWords(Forms) == std::vector<std::string>{"il heureux", "elle heureuse",
                                                             "ils heureux", "elles heureuses"});
}

TEST_CASE("A missing feminine plural is the feminine with the plural ending", "[conjugation][forms]")
{
    const std::vector<FTCard> Forms = ExpandWord(FormsKindAdjective, MakeAdjective("blanc", "blanche", "blancs", ""));

    REQUIRE(Forms.size() == 4);
    CHECK(Forms[3].TargetWord == "elles blanches");
}

TEST_CASE("An adjective with a broken cell is not offered", "[conjugation][forms]")
{
    FTDictionaryEntry Bon = MakeAdjective("bon", "bonne", "bons", "bonnes");
    Bon.Forms.push_back(MakeAgreementForm("bones", 2, 2));

    const FTDictionaryEntry Paysan = MakeAdjective("paysan", "paysanne", "", "");

    FTDictionaryEntry Phrase = MakeBeau();
    Phrase.Headword = "bien beau";

    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, Bon, "fr", "good").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, Paysan, "fr", "peasant").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, Phrase, "fr", "beautiful").has_value());
}

TEST_CASE("The second masculine form and inclusive spellings are ignored", "[conjugation][forms]")
{
    FTDictionaryEntry Beau = MakeBeau();
    Beau.Forms.push_back(MakeAgreementForm("bel", 1, 1));
    Beau.Forms.push_back(MakeAgreementForm("beau\xC2\xB7x", 1, 2));

    const std::vector<FTCard> Forms = ExpandWord(FormsKindAdjective, Beau);

    REQUIRE(Forms.size() == 4);
    CHECK(Forms[0].TargetWord == "il beau");
    CHECK(Forms[2].TargetWord == "ils beaux");
}

TEST_CASE("An agreement form is marked where it departs from the headword", "[conjugation][forms]")
{
    const std::vector<FTCard> Forms = ExpandWord(FormsKindAdjective, MakeBeau());

    const std::optional<FTWordForm> Masculine = ReadWordForm(Forms[0]);
    REQUIRE(Masculine.has_value());
    CHECK(Masculine->Subject == "il");
    CHECK(Masculine->Base == "beau");
    CHECK(Masculine->Form == "beau");
    CHECK(Masculine->Stem == "beau");
    CHECK(Masculine->Ending.empty());

    const std::optional<FTWordForm> Feminine = ReadWordForm(Forms[1]);
    REQUIRE(Feminine.has_value());
    CHECK(Feminine->Subject == "elle");
    CHECK(Feminine->Form == "belle");
    CHECK(Feminine->Stem == "be");
    CHECK(Feminine->Ending == "lle");

    const std::optional<FTWordForm> Plural = ReadWordForm(Forms[3]);
    REQUIRE(Plural.has_value());
    CHECK(Plural->Subject == "elles");
    CHECK(Plural->Stem == "be");
    CHECK(Plural->Ending == "lles");

    CHECK(MakeFormPrompt(Feminine->Subject) == "elle ___");
}

TEST_CASE("The marked part never starts in the middle of an accented letter", "[conjugation][forms]")
{
    const std::vector<FTCard> Cher = ExpandWord(FormsKindAdjective, MakeAdjective("cher", "chère", "chers", "chères"));

    const std::optional<FTWordForm> Feminine = ReadWordForm(Cher[1]);
    REQUIRE(Feminine.has_value());
    CHECK(Feminine->Stem == "ch");
    CHECK(Feminine->Ending == "ère");

    const std::vector<FTCard> Made = ExpandWord(FormsKindAdjective, MakeAdjective("prét", "prète", "préts", "prètes"));

    const std::optional<FTWordForm> Accent = ReadWordForm(Made[1]);
    REQUIRE(Accent.has_value());
    CHECK(Accent->Stem == "pr");
    CHECK(Accent->Ending == "ète");
}

TEST_CASE("A noun with an irregular plural becomes a card of two forms", "[conjugation][forms]")
{
    const FTDictionaryEntry Cheval = MakeNoun("cheval", GenderMasculine, "chevaux");

    const std::optional<FTCard> Card = MakeFormsCard(FormsKindNoun, Cheval, "fr", "horse");
    REQUIRE(Card.has_value());
    CHECK(Card->Id == "forms-noun-fr-cheval");

    const std::vector<FTCard> Forms = ExpandFormsCard(*Card, Cheval, "fr");

    REQUIRE(Forms.size() == 2);
    CHECK(Forms[0].TargetWord == "un cheval");
    CHECK(Forms[0].Id == "forms-noun-fr-cheval#1-1");
    CHECK(Forms[1].TargetWord == "des chevaux");
    CHECK(Forms[1].Id == "forms-noun-fr-cheval#1-2");
    CHECK(Forms[1].Translation == "cheval");

    const std::optional<FTWordForm> Plural = ReadWordForm(Forms[1]);
    REQUIRE(Plural.has_value());
    CHECK(Plural->Subject == "des");
    CHECK(Plural->Stem == "cheva");
    CHECK(Plural->Ending == "ux");
    CHECK(MakeFormPrompt(Plural->Subject) == "des ___");
}

TEST_CASE("A feminine noun takes the feminine article", "[conjugation][forms]")
{
    const std::vector<FTCard> Forms = ExpandWord(FormsKindNoun, MakeNoun("eau", GenderFeminine, "eaux"));

    REQUIRE(Forms.size() == 2);
    CHECK(Forms[0].TargetWord == "une eau");
    CHECK(Forms[0].Id == "forms-noun-fr-eau#2-1");
    CHECK(Forms[1].TargetWord == "des eaux");
    CHECK(Forms[1].Id == "forms-noun-fr-eau#2-2");
}

TEST_CASE("A noun of either gender is treated as masculine", "[conjugation][forms]")
{
    const std::vector<FTCard> Forms = ExpandWord(FormsKindNoun, MakeNoun("journal", GenderCommon, "journaux"));

    REQUIRE(Forms.size() == 2);
    CHECK(Forms[0].TargetWord == "un journal");
}

TEST_CASE("A noun with a plain plural is not offered", "[conjugation][forms]")
{
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("chat", GenderMasculine, "chats"), "fr", "cat").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("temps", GenderMasculine, ""), "fr", "time").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("maison", GenderFeminine, "maisons"), "fr", "house")
                    .has_value());
}

TEST_CASE("A noun with no plural or no gender is not offered", "[conjugation][forms]")
{
    FTDictionaryEntry Genderless = MakeNoun("cheval", GenderMasculine, "chevaux");
    Genderless.Senses.clear();

    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("nord", GenderMasculine, ""), "fr", "north").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, Genderless, "fr", "horse").has_value());
}

TEST_CASE("A noun with a feminine partner gets four forms", "[conjugation][forms]")
{
    const std::vector<FTCard> Forms = ExpandWord(FormsKindNoun, MakeVendeur());

    REQUIRE(Forms.size() == 4);
    CHECK(ReadTargetWords(Forms) == std::vector<std::string>{"un vendeur", "une vendeuse", "des vendeurs",
                                                             "des vendeuses"});

    CHECK(Forms[0].Id == "forms-noun-fr-vendeur#1-1");
    CHECK(Forms[1].Id == "forms-noun-fr-vendeur#2-1");
    CHECK(Forms[2].Id == "forms-noun-fr-vendeur#1-2");
    CHECK(Forms[3].Id == "forms-noun-fr-vendeur#2-2");
}

TEST_CASE("The feminine plural is asked from the feminine, so each plural prompt has one answer",
          "[conjugation][forms]")
{
    const std::vector<FTCard> Forms = ExpandWord(FormsKindNoun, MakeVendeur());

    REQUIRE(Forms.size() == 4);
    CHECK(Forms[1].Translation == "vendeur");
    CHECK(Forms[2].Translation == "vendeur");
    CHECK(Forms[3].Translation == "vendeuse");

    const std::optional<FTWordForm> Plural = ReadWordForm(Forms[2]);
    const std::optional<FTWordForm> FemininePlural = ReadWordForm(Forms[3]);

    REQUIRE(Plural.has_value());
    REQUIRE(FemininePlural.has_value());

    CHECK(MakeFormPrompt(Plural->Subject) == "des ___");
    CHECK(MakeFormPrompt(FemininePlural->Subject) == "des ___");

    CHECK(FemininePlural->Form == "vendeuses");
    CHECK(FemininePlural->Stem == "vendeu");
    CHECK(FemininePlural->Ending == "ses");
}

TEST_CASE("A stored feminine plural wins over the built one", "[conjugation][forms]")
{
    FTDictionaryEntry Vendeur = MakeVendeur();
    Vendeur.Forms.push_back(MakeAgreementForm("vendeuzes", 2, 2));

    const std::vector<FTCard> Forms = ExpandWord(FormsKindNoun, Vendeur);

    REQUIRE(Forms.size() == 4);
    CHECK(Forms[3].TargetWord == "des vendeuzes");
}

TEST_CASE("A plain feminine partner alone does not make a noun worth a card", "[conjugation][forms]")
{
    FTDictionaryEntry Ami = MakeNoun("ami", GenderMasculine, "amis");
    Ami.Forms.push_back(MakeAgreementForm("amie", 2, 1));

    FTDictionaryEntry General = MakeNoun("général", GenderMasculine, "généraux");
    General.Forms.push_back(MakeAgreementForm("générale", 2, 1));

    CHECK_FALSE(MakeFormsCard(FormsKindNoun, Ami, "fr", "friend").has_value());

    REQUIRE(MakeFormsCard(FormsKindNoun, General, "fr", "general").has_value());
    CHECK(ExpandWord(FormsKindNoun, General).size() == 4);
}

TEST_CASE("A capital letter alone does not make a noun worth a card", "[conjugation][forms]")
{
    FTDictionaryEntry Noir = MakeNoun("noir", GenderMasculine, "Noirs");
    Noir.Forms.push_back(MakeAgreementForm("Noire", 2, 1));
    Noir.Forms.push_back(MakeAgreementForm("Noires", 2, 2));

    FTDictionaryEntry Chinois = MakeNoun("chinois", GenderMasculine, "");
    Chinois.Forms.push_back(MakeAgreementForm("Chinoise", 2, 1));
    Chinois.Forms.push_back(MakeAgreementForm("Chinoises", 2, 2));

    CHECK_FALSE(MakeFormsCard(FormsKindNoun, Noir, "fr", "black").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, Chinois, "fr", "Chinese").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, MakeAdjective("écossais", "Écossaise", "", "Écossaises"), "fr",
                              "Scottish").has_value());
}

TEST_CASE("A capitalised form is marked from where its letters depart from the headword", "[conjugation][forms]")
{
    FTDictionaryEntry Vendeur = MakeNoun("vendeur", GenderMasculine, "Vendeurs");
    Vendeur.Forms.push_back(MakeAgreementForm("Vendeuse", 2, 1));

    const std::vector<FTCard> Forms = ExpandWord(FormsKindNoun, Vendeur);

    REQUIRE(Forms.size() == 4);

    const std::optional<FTWordForm> Feminine = ReadWordForm(Forms[1]);
    REQUIRE(Feminine.has_value());
    CHECK(Feminine->Stem == "Vendeu");
    CHECK(Feminine->Ending == "se");
}

TEST_CASE("A noun with two feminines is not offered", "[conjugation][forms]")
{
    FTDictionaryEntry Vendeur = MakeVendeur();
    Vendeur.Forms.push_back(MakeAgreementForm("venderesse", 2, 1));

    CHECK_FALSE(MakeFormsCard(FormsKindNoun, Vendeur, "fr", "seller").has_value());
}

TEST_CASE("A feminine noun ignores feminine singular forms listed under it", "[conjugation][forms]")
{
    FTDictionaryEntry Eau = MakeNoun("eau", GenderFeminine, "eaux");
    Eau.Forms.push_back(MakeAgreementForm("eaulette", 2, 1));

    CHECK(ExpandWord(FormsKindNoun, Eau).size() == 2);
}

TEST_CASE("Forms of one word group together whatever their prompt names", "[conjugation][forms]")
{
    std::vector<FTCardHandle> Cards;

    for (FTCard& Form : ExpandWord(FormsKindNoun, MakeVendeur()))
    {
        Cards.push_back(MakeCardHandle(std::move(Form)));
    }

    for (FTCard& Form : ExpandWord(FormsKindNoun, MakeNoun("cheval", GenderMasculine, "chevaux")))
    {
        Cards.push_back(MakeCardHandle(std::move(Form)));
    }

    const std::vector<std::vector<int>> Groups = GroupCardsByWord(Cards);

    REQUIRE(Groups.size() == 2);
    CHECK(Groups[0] == std::vector<int>{0, 1, 2, 3});
    CHECK(Groups[1] == std::vector<int>{4, 5});
}

TEST_CASE("A session takes as many words as its training allows", "[conjugation][forms]")
{
    std::vector<FTCardHandle> Cards;

    for (int Index = 0; Index < 12; ++Index)
    {
        FTCard Card;
        Card.Id = "forms-adj-fr-word" + std::to_string(Index);

        Cards.push_back(MakeCardHandle(std::move(Card)));
    }

    CHECK(TakeFormsSession(FormsTrainingAgreement, Cards).size() == 8);
    CHECK(TakeFormsSession(FormsTrainingConjugation, Cards).size() == 4);
    CHECK(TakeFormsSession("adverbs", Cards).empty());
}

static constexpr int TenseFuture = 4;
static constexpr int TensePastParticiple = 8;

static void AddTense(FTDictionaryEntry& Verb, int Tense, const std::array<std::string, 6>& Forms)
{
    for (size_t Index = 0; Index < Forms.size(); ++Index)
    {
        const int Person = static_cast<int>(Index % 3) + 1;
        const int Number = static_cast<int>(Index / 3) + 1;

        Verb.Forms.push_back(MakeForm(Forms[Index], Person, Number, Tense));
    }
}

static void AddParticiple(FTDictionaryEntry& Verb, const std::string& Participle)
{
    FTDictionaryForm Form;
    Form.Text = Participle;
    Form.Tense = TensePastParticiple;

    Verb.Forms.push_back(Form);
}

static FTDictionaryEntry MakeParler()
{
    FTDictionaryEntry Verb = MakeVerb("parler", {"parle", "parles", "parle", "parlons", "parlez", "parlent"});

    AddTense(Verb, TenseImperfect, {"parlais", "parlais", "parlait", "parlions", "parliez", "parlaient"});
    AddTense(Verb, TenseFuture, {"parlerai", "parleras", "parlera", "parlerons", "parlerez", "parleront"});
    AddParticiple(Verb, "parlé");

    return Verb;
}

static std::vector<FTCard> ExpandTense(const FTDictionaryEntry& Verb, const std::string& Tense)
{
    const std::optional<FTCard> Stored = MakeFormsCard(FormsKindVerb, Verb, "fr", "translation", Tense);
    REQUIRE(Stored.has_value());

    return ExpandFormsCard(*Stored, Verb, "fr");
}

TEST_CASE("French has four tenses and every later one opens from the present", "[conjugation][tenses]")
{
    CHECK(ListFormsTenses("fr") == std::vector<std::string>{"present", "imperfect", "future", "perfect"});
    CHECK(ListFormsTenses("es")
          == std::vector<std::string>{"present", "imperfect", "preterite", "future", "perfect"});
    CHECK(ListFormsTenses("de").empty());

    CHECK(GetFormsTenseLabel("fr", "present") == "présent");
    CHECK(GetFormsTenseLabel("fr", "imperfect") == "imparfait");
    CHECK(GetFormsTenseLabel("fr", "future") == "futur");
    CHECK(GetFormsTenseLabel("fr", "perfect") == "passé composé");
    CHECK(GetFormsTenseLabel("fr", "pluperfect").empty());

    CHECK(GetFormsTenseRequirement("fr", "present").empty());
    CHECK(GetFormsTenseRequirement("fr", "imperfect") == "present");
    CHECK(GetFormsTenseRequirement("fr", "future") == "present");
    CHECK(GetFormsTenseRequirement("fr", "perfect") == "present");
}

TEST_CASE("A deck tense has the dictionary's tense code", "[conjugation][tenses]")
{
    CHECK(GetFormsTenseCode("fr", "present") == 1);
    CHECK(GetFormsTenseCode("fr", "imperfect") == 2);
    CHECK(GetFormsTenseCode("fr", "future") == 4);
    CHECK(GetFormsTenseCode("fr", "perfect") == 8);
    CHECK(GetFormsTenseCode("fr", "preterite") == 0);

    CHECK(GetFormsTenseCode("es", "preterite") == 3);
    CHECK(GetFormsTenseCode("es", "perfect") == 8);

    CHECK(GetFormsTenseCode("de", "present") == 0);
    CHECK(GetFormsTenseCode("fr", "") == 0);
}

TEST_CASE("A card's tense is the last part of its id", "[conjugation][tenses]")
{
    CHECK(GetFormsTense("conjugation-fr-parler-present") == "present");
    CHECK(GetFormsTense("conjugation-fr-parler-imperfect#1-2") == "imperfect");
    CHECK(GetFormsTense("conjugation-fr-grand-père-future") == "future");
    CHECK(GetFormsTense("forms-adj-fr-beau").empty());
    CHECK(GetFormsTense("some-card").empty());

    CHECK(GetFormsCardTenseLabel("conjugation-fr-parler-perfect#3-2") == "passé composé");
    CHECK(GetFormsCardTenseLabel("forms-noun-fr-cheval").empty());
}

TEST_CASE("Each tense of a verb is its own stored card", "[conjugation][tenses]")
{
    const FTDictionaryEntry Verb = MakeParler();

    const std::optional<FTCard> Present = MakeFormsCard(FormsKindVerb, Verb, "fr", "speak");
    const std::optional<FTCard> Imperfect = MakeFormsCard(FormsKindVerb, Verb, "fr", "speak", "imperfect");
    const std::optional<FTCard> Perfect = MakeFormsCard(FormsKindVerb, Verb, "fr", "speak", "perfect");

    REQUIRE(Present.has_value());
    REQUIRE(Imperfect.has_value());
    REQUIRE(Perfect.has_value());

    CHECK(Present->Id == "conjugation-fr-parler-present");
    CHECK(Imperfect->Id == "conjugation-fr-parler-imperfect");
    CHECK(Perfect->Id == "conjugation-fr-parler-perfect");
    CHECK(Imperfect->TargetWord == "parler");
    CHECK(Imperfect->Translation == "speak");

    CHECK_FALSE(MakeFormsCard(FormsKindVerb, Verb, "fr", "speak", "pluperfect").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindVerb, Verb, "fr", "speak", "preterite").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindVerb, MakeVouloir(), "fr", "want", "imperfect").has_value());
}

TEST_CASE("A stored card expands into the forms of its own tense", "[conjugation][tenses]")
{
    const FTDictionaryEntry Verb = MakeParler();

    CHECK(ReadTargetWords(ExpandTense(Verb, "imperfect"))
          == std::vector<std::string>{"je parlais", "tu parlais", "il parlait", "nous parlions", "vous parliez",
                                      "ils parlaient"});
    CHECK(ReadTargetWords(ExpandTense(Verb, "future"))
          == std::vector<std::string>{"je parlerai", "tu parleras", "il parlera", "nous parlerons", "vous parlerez",
                                      "ils parleront"});

    const std::vector<FTCard> Imperfect = ExpandTense(Verb, "imperfect");

    CHECK(Imperfect[3].Id == "conjugation-fr-parler-imperfect#1-2");
    CHECK(Imperfect[3].Translation == "parler");
    CHECK(GetFormsCardId(Imperfect[3].Id) == "conjugation-fr-parler-imperfect");
}

TEST_CASE("Each tense marks its own endings", "[conjugation][tenses]")
{
    const FTDictionaryEntry Verb = MakeParler();

    const std::optional<FTWordForm> Imperfect = ReadWordForm(ExpandTense(Verb, "imperfect")[5]);
    REQUIRE(Imperfect.has_value());
    CHECK(Imperfect->Subject == "ils");
    CHECK(Imperfect->Base == "parler");
    CHECK(Imperfect->Stem == "parl");
    CHECK(Imperfect->Ending == "aient");

    const std::optional<FTWordForm> Future = ReadWordForm(ExpandTense(Verb, "future")[0]);
    REQUIRE(Future.has_value());
    CHECK(Future->Stem == "parler");
    CHECK(Future->Ending == "ai");

    const std::optional<FTWordForm> Present = ReadWordForm(ExpandTense(Verb, "present")[3]);
    REQUIRE(Present.has_value());
    CHECK(Present->Stem == "parl");
    CHECK(Present->Ending == "ons");
}

TEST_CASE("The passé composé is built from avoir and the past participle", "[conjugation][tenses]")
{
    const std::vector<FTCard> Forms = ExpandTense(MakeParler(), "perfect");

    CHECK(ReadTargetWords(Forms)
          == std::vector<std::string>{"j'ai parlé", "tu as parlé", "il a parlé", "nous avons parlé", "vous avez parlé",
                                      "ils ont parlé"});

    const std::optional<FTWordForm> First = ReadWordForm(Forms[0]);
    REQUIRE(First.has_value());
    CHECK(First->Subject == "je");
    CHECK(First->Form == "ai parlé");
    CHECK(First->Stem == "ai parl");
    CHECK(First->Ending == "é");
    CHECK(MakeFormPrompt(First->Subject) == "je ___");

    const std::optional<FTWordForm> Fourth = ReadWordForm(Forms[3]);
    REQUIRE(Fourth.has_value());
    CHECK(Fourth->Subject == "nous");
    CHECK(Fourth->Form == "avons parlé");
}

TEST_CASE("A verb of movement takes être and its participle agrees in the plural", "[conjugation][tenses]")
{
    FTDictionaryEntry Venir = MakeVerb("venir", {"viens", "viens", "vient", "venons", "venez", "viennent"});
    AddParticiple(Venir, "venu");

    const std::vector<FTCard> Forms = ExpandTense(Venir, "perfect");

    CHECK(ReadTargetWords(Forms)
          == std::vector<std::string>{"je suis venu", "tu es venu", "il est venu", "nous sommes venus",
                                      "vous êtes venus", "ils sont venus"});

    const std::optional<FTWordForm> Last = ReadWordForm(Forms[5]);
    REQUIRE(Last.has_value());
    CHECK(Last->Form == "sont venus");
    CHECK(Last->Stem == "sont ven");
    CHECK(Last->Ending == "us");
}

TEST_CASE("A tense the dictionary lacks is not offered, and a second participle is an alternative",
          "[conjugation][tenses]")
{
    FTDictionaryEntry Verb = MakeVerb("parler", {"parle", "parles", "parle", "parlons", "parlez", "parlent"});

    CHECK_FALSE(MakeFormsCard(FormsKindVerb, Verb, "fr", "speak", "imperfect").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindVerb, Verb, "fr", "speak", "perfect").has_value());

    AddParticiple(Verb, "parlet");
    AddParticiple(Verb, "parlé");

    const std::optional<FTCard> Perfect = MakeFormsCard(FormsKindVerb, Verb, "fr", "speak", "perfect");
    REQUIRE(Perfect.has_value());

    const std::vector<FTCard> PerfectItems = ExpandFormsCard(*Perfect, Verb, "fr");
    REQUIRE(PerfectItems.size() == 6);
    CHECK(PerfectItems[0].TargetWord == "j'ai parlé");
    CHECK(PerfectItems[0].AlternativeSpellings == std::vector<std::string>{"j'ai parlet"});
    CHECK(PerfectItems[5].TargetWord == "ils ont parlé");

    FTCard Stored;
    Stored.Id = "conjugation-fr-parler-future";
    Stored.TargetWord = "parler";

    CHECK(ExpandFormsCard(Stored, Verb, "fr").empty());
}

TEST_CASE("The helper verbs are offered like any other verb", "[conjugation][tenses]")
{
    const FTDictionaryEntry Etre = MakeVerb("être", {"suis", "es", "est", "sommes", "êtes", "sont"});
    const FTDictionaryEntry Avoir = MakeVerb("avoir", {"ai", "as", "a", "avons", "avez", "ont"});

    CHECK(MakeFormsCard(FormsKindVerb, Etre, "fr", "be").has_value());

    const std::optional<FTCard> Stored = MakeFormsCard(FormsKindVerb, Avoir, "fr", "have");
    REQUIRE(Stored.has_value());

    const std::vector<FTCard> Forms = ExpandFormsCard(*Stored, Avoir, "fr");

    REQUIRE(Forms.size() == 6);
    CHECK(Forms[0].TargetWord == "j'ai");
}

static constexpr int TensePreterite = 3;

static std::vector<FTCard> ExpandSpanish(const std::string& Kind, const FTDictionaryEntry& Entry,
                                         const std::string& Tense = FormsTensePresent)
{
    const std::optional<FTCard> Stored = MakeFormsCard(Kind, Entry, "es", "translation", Tense);
    REQUIRE(Stored.has_value());

    return ExpandFormsCard(*Stored, Entry, "es");
}

static FTDictionaryEntry MakeHablar()
{
    FTDictionaryEntry Verb = MakeVerb("hablar", {"hablo", "hablas", "habla", "hablamos", "habláis", "hablan"});

    AddTense(Verb, TenseImperfect, {"hablaba", "hablabas", "hablaba", "hablábamos", "hablabais", "hablaban"});
    AddTense(Verb, TensePreterite, {"hablé", "hablaste", "habló", "hablamos", "hablasteis", "hablaron"});
    AddTense(Verb, TenseFuture, {"hablaré", "hablarás", "hablará", "hablaremos", "hablaréis", "hablarán"});
    AddParticiple(Verb, "hablado");

    return Verb;
}

TEST_CASE("Spanish has five tenses and every later one opens from the present", "[conjugation][spanish]")
{
    CHECK(GetFormsTenseLabel("es", "present") == "presente");
    CHECK(GetFormsTenseLabel("es", "imperfect") == "imperfecto");
    CHECK(GetFormsTenseLabel("es", "preterite") == "indefinido");
    CHECK(GetFormsTenseLabel("es", "future") == "futuro");
    CHECK(GetFormsTenseLabel("es", "perfect") == "pretérito perfecto");

    for (const std::string Tense : {"imperfect", "preterite", "future", "perfect"})
    {
        CHECK(GetFormsTenseRequirement("es", Tense) == "present");
    }

    CHECK(GetFormsTenseRequirement("es", "present").empty());
    CHECK(GetFormsTenseLabel("fr", "preterite").empty());
}

TEST_CASE("A Spanish verb expands into each of its tenses", "[conjugation][spanish]")
{
    const FTDictionaryEntry Verb = MakeHablar();

    CHECK(ReadTargetWords(ExpandSpanish(FormsKindVerb, Verb))
          == std::vector<std::string>{"yo hablo", "tú hablas", "él habla", "nosotros hablamos", "vosotros habláis",
                                      "ellos hablan"});
    CHECK(ReadTargetWords(ExpandSpanish(FormsKindVerb, Verb, "imperfect")).front() == "yo hablaba");
    CHECK(ReadTargetWords(ExpandSpanish(FormsKindVerb, Verb, "preterite"))
          == std::vector<std::string>{"yo hablé", "tú hablaste", "él habló", "nosotros hablamos",
                                      "vosotros hablasteis", "ellos hablaron"});
    CHECK(ReadTargetWords(ExpandSpanish(FormsKindVerb, Verb, "future")).back() == "ellos hablarán");
    CHECK(ReadTargetWords(ExpandSpanish(FormsKindVerb, Verb, "perfect"))
          == std::vector<std::string>{"yo he hablado", "tú has hablado", "él ha hablado", "nosotros hemos hablado",
                                      "vosotros habéis hablado", "ellos han hablado"});

    CHECK(ExpandSpanish(FormsKindVerb, Verb, "preterite")[2].Id == "conjugation-es-hablar-preterite#3-1");
}

TEST_CASE("Spanish labels the third persons with usted and ustedes, and accepts them typed",
          "[conjugation][spanish]")
{
    const std::vector<FTCard> Forms = ExpandSpanish(FormsKindVerb, MakeHablar());

    CHECK(ReadWordForm(Forms[2])->SubjectLabel == "él/usted");
    CHECK(ReadWordForm(Forms[5])->SubjectLabel == "ellos/ustedes");

    const std::vector<std::string> Accepted = ListAcceptedForms(Forms[5]);

    CHECK(std::find(Accepted.begin(), Accepted.end(), "ustedes hablan") != Accepted.end());
}

TEST_CASE("Only the Spanish vosotros form is optional", "[conjugation][spanish]")
{
    CHECK(GetOptionalFormsPronoun("es") == "vosotros");
    CHECK(GetOptionalFormsPronoun("fr").empty());
    CHECK(GetOptionalFormsPronoun("pt").empty());

    std::vector<std::string> Optional;

    for (const FTCard& Form : ExpandSpanish(FormsKindVerb, MakeHablar(), "preterite"))
    {
        if (IsOptionalForm(Form))
        {
            Optional.push_back(Form.TargetWord);
        }
    }

    CHECK(Optional == std::vector<std::string>{"vosotros hablasteis"});
}

TEST_CASE("Spanish tenses mark their own endings", "[conjugation][spanish]")
{
    const FTDictionaryEntry Verb = MakeHablar();

    const std::optional<FTWordForm> Imperfect = ReadWordForm(ExpandSpanish(FormsKindVerb, Verb, "imperfect")[3]);
    REQUIRE(Imperfect.has_value());
    CHECK(Imperfect->Subject == "nosotros");
    CHECK(Imperfect->Stem == "habl");
    CHECK(Imperfect->Ending == "ábamos");

    const std::optional<FTWordForm> Preterite = ReadWordForm(ExpandSpanish(FormsKindVerb, Verb, "preterite")[2]);
    REQUIRE(Preterite.has_value());
    CHECK(Preterite->Stem == "habl");
    CHECK(Preterite->Ending == "ó");

    const std::optional<FTWordForm> Future = ReadWordForm(ExpandSpanish(FormsKindVerb, Verb, "future")[0]);
    REQUIRE(Future.has_value());
    CHECK(Future->Stem == "hablar");
    CHECK(Future->Ending == "é");

    const std::optional<FTWordForm> Perfect = ReadWordForm(ExpandSpanish(FormsKindVerb, Verb, "perfect")[0]);
    REQUIRE(Perfect.has_value());
    CHECK(Perfect->Subject == "yo");
    CHECK(Perfect->Form == "he hablado");
    CHECK(Perfect->Stem == "he habla");
    CHECK(Perfect->Ending == "do");
    CHECK(MakeFormPrompt(Perfect->Subject) == "yo ___");
}

TEST_CASE("A Spanish verb with a second spelling for tú shows the plain one and accepts both",
          "[conjugation][spanish]")
{
    FTDictionaryEntry Verb = MakeHablar();
    Verb.Forms.push_back(MakeForm("hablás", 2, 1));

    REQUIRE(MakeFormsCard(FormsKindVerb, Verb, "es", "speak").has_value());
    CHECK(MakeFormsCard(FormsKindVerb, Verb, "es", "speak", "imperfect").has_value());

    const std::vector<FTCard> Items = ExpandSpanish(FormsKindVerb, Verb);
    REQUIRE(Items.size() == 6);

    CHECK(Items[1].TargetWord == "tú hablas");
    CHECK(Items[1].AlternativeSpellings == std::vector<std::string>{"tú hablás"});
}

TEST_CASE("Regular Spanish adjectives are not offered", "[conjugation][spanish]")
{
    FTDictionaryEntry Grande = MakeAdjective("grande", "", "", "");
    Grande.Forms.push_back(MakeAgreementForm("grandes", 0, 2));

    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, MakeAdjective("bueno", "buena", "buenos", "buenas"), "es", "good")
                    .has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, MakeAdjective("español", "española", "españoles", "españolas"),
                              "es", "Spanish").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, Grande, "es", "big").has_value());
}

TEST_CASE("A Spanish adjective with one gender and a spelling shift gets two forms", "[conjugation][spanish]")
{
    const std::vector<FTCard> Forms = ExpandSpanish(FormsKindAdjective, MakeAdjective("feliz", "", "felices", ""));

    REQUIRE(Forms.size() == 2);
    CHECK(Forms[0].TargetWord == "él feliz");
    CHECK(Forms[0].Id == "forms-adj-es-feliz#1-1");
    CHECK(Forms[1].TargetWord == "ellos felices");
    CHECK(Forms[1].Id == "forms-adj-es-feliz#1-2");

    const std::optional<FTWordForm> Plural = ReadWordForm(Forms[1]);
    REQUIRE(Plural.has_value());
    CHECK(Plural->Subject == "ellos");
    CHECK(Plural->Stem == "feli");
    CHECK(Plural->Ending == "ces");
}

TEST_CASE("A Spanish adjective that drops its accent gets four forms", "[conjugation][spanish]")
{
    const std::vector<FTCard> Forms =
        ExpandSpanish(FormsKindAdjective, MakeAdjective("francés", "francesa", "franceses", "francesas"));

    CHECK(ReadTargetWords(Forms) == std::vector<std::string>{"él francés", "ella francesa",
                                                             "ellos franceses", "ellas francesas"});

    const std::optional<FTWordForm> Feminine = ReadWordForm(Forms[1]);
    REQUIRE(Feminine.has_value());
    CHECK(Feminine->Stem == "franc");
    CHECK(Feminine->Ending == "esa");
}

TEST_CASE("A Spanish noun is offered only when its plural is not the plain one", "[conjugation][spanish]")
{
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("libro", GenderMasculine, "libros"), "es", "book").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("papel", GenderMasculine, "papeles"), "es", "paper")
                    .has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("casa", GenderFeminine, "casas"), "es", "house").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("lunes", GenderMasculine, ""), "es", "Monday").has_value());

    const std::vector<FTCard> Pencil = ExpandSpanish(FormsKindNoun, MakeNoun("lápiz", GenderMasculine, "lápices"));

    REQUIRE(Pencil.size() == 2);
    CHECK(Pencil[0].TargetWord == "un lápiz");
    CHECK(Pencil[1].TargetWord == "unos lápices");

    const std::vector<FTCard> Song = ExpandSpanish(FormsKindNoun, MakeNoun("canción", GenderFeminine, "canciones"));

    REQUIRE(Song.size() == 2);
    CHECK(Song[0].TargetWord == "una canción");
    CHECK(Song[1].TargetWord == "unas canciones");
}

TEST_CASE("A Spanish noun with a feminine partner gets four forms", "[conjugation][spanish]")
{
    FTDictionaryEntry Rey = MakeNoun("rey", GenderMasculine, "reyes");
    Rey.Forms.push_back(MakeAgreementForm("reina", 2, 1));
    Rey.Forms.push_back(MakeAgreementForm("reinas", 2, 2));

    const std::vector<FTCard> Forms = ExpandSpanish(FormsKindNoun, Rey);

    CHECK(ReadTargetWords(Forms) == std::vector<std::string>{"un rey", "una reina", "unos reyes", "unas reinas"});
    CHECK(Forms[3].Translation == "reina");

    FTDictionaryEntry Profesor = MakeNoun("profesor", GenderMasculine, "profesores");
    Profesor.Forms.push_back(MakeAgreementForm("profesora", 2, 1));
    Profesor.Forms.push_back(MakeAgreementForm("profesoras", 2, 2));

    CHECK_FALSE(MakeFormsCard(FormsKindNoun, Profesor, "es", "teacher").has_value());

    FTDictionaryEntry Amigo = MakeNoun("amigo", GenderMasculine, "amigos");
    Amigo.Forms.push_back(MakeAgreementForm("amiga", 2, 1));

    CHECK_FALSE(MakeFormsCard(FormsKindNoun, Amigo, "es", "friend").has_value());
}

TEST_CASE("The masculine plural listed under the feminine plural is ignored", "[conjugation][spanish]")
{
    FTDictionaryEntry Jefe = MakeNoun("jefe", GenderMasculine, "jefes");
    Jefe.Forms.push_back(MakeAgreementForm("jefa", 2, 1));
    Jefe.Forms.push_back(MakeAgreementForm("jefes", 2, 2));
    Jefe.Forms.push_back(MakeAgreementForm("jefas", 2, 2));

    const std::vector<FTCard> Forms = ExpandSpanish(FormsKindNoun, Jefe);

    CHECK(ReadTargetWords(Forms) == std::vector<std::string>{"un jefe", "una jefa", "unos jefes", "unas jefas"});
}

TEST_CASE("A noun takes the gender most of its meanings have", "[conjugation][spanish]")
{
    FTDictionaryEntry Cabeza = MakeNoun("voz", GenderMasculine, "voces");

    FTDictionarySense Feminine;
    Feminine.Gender = GenderFeminine;

    FTDictionarySense Genderless;

    Cabeza.Senses.push_back(Genderless);
    Cabeza.Senses.push_back(Feminine);
    Cabeza.Senses.push_back(Feminine);
    Cabeza.Forms.push_back(MakeAgreementForm("voces", 2, 2));

    const std::vector<FTCard> Forms = ExpandSpanish(FormsKindNoun, Cabeza);

    REQUIRE(Forms.size() == 2);
    CHECK(Forms[0].TargetWord == "una voz");
    CHECK(Forms[1].TargetWord == "unas voces");
}

TEST_CASE("A French adjective with no feminine gets no card, whatever its plural", "[conjugation][forms]")
{
    FTDictionaryEntry Tout = MakeAdjective("tout", "", "tous", "");

    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, Tout, "fr", "all").has_value());

    FTCard Stored;
    Stored.Id = "forms-adj-fr-tout";
    Stored.TargetWord = "tout";

    CHECK(ExpandFormsCard(Stored, Tout, "fr").empty());
}

static std::vector<FTCard> ExpandPortuguese(const std::string& Kind, const FTDictionaryEntry& Entry,
                                            const std::string& Tense = FormsTensePresent)
{
    const std::optional<FTCard> Stored = MakeFormsCard(Kind, Entry, "pt", "translation", Tense);
    REQUIRE(Stored.has_value());

    return ExpandFormsCard(*Stored, Entry, "pt");
}

static FTDictionaryEntry MakeFalar()
{
    FTDictionaryEntry Verb = MakeVerb("falar", {"falo", "falas", "fala", "falamos", "falais", "falam"});

    AddTense(Verb, TenseImperfect, {"falava", "falavas", "falava", "falávamos", "faláveis", "falavam"});
    AddTense(Verb, TensePreterite, {"falei", "falaste", "falou", "falámos", "falastes", "falaram"});
    AddTense(Verb, TenseFuture, {"falarei", "falarás", "falará", "falaremos", "falareis", "falarão"});
    AddParticiple(Verb, "falado");

    return Verb;
}

TEST_CASE("Portuguese has five tenses and every later one opens from the present", "[conjugation][portuguese]")
{
    CHECK(ListFormsTenses("pt")
          == std::vector<std::string>{"present", "preterite", "imperfect", "future", "perfect"});

    CHECK(GetFormsTenseLabel("pt", "present") == "presente");
    CHECK(GetFormsTenseLabel("pt", "preterite") == "pretérito perfeito");
    CHECK(GetFormsTenseLabel("pt", "imperfect") == "imperfeito");
    CHECK(GetFormsTenseLabel("pt", "future") == "futuro");
    CHECK(GetFormsTenseLabel("pt", "perfect") == "perfeito composto");

    for (const std::string Tense : {"preterite", "imperfect", "future", "perfect"})
    {
        CHECK(GetFormsTenseRequirement("pt", Tense) == "present");
    }

    CHECK(GetFormsTenseCode("pt", "preterite") == TensePreterite);
    CHECK(GetFormsTenseCode("pt", "perfect") == TensePastParticiple);

    CHECK(SupportsFormsTraining(FormsTrainingConjugation, "pt"));
    CHECK(SupportsFormsTraining(FormsTrainingAgreement, "pt"));
}

TEST_CASE("A Portuguese verb expands into each of its tenses", "[conjugation][portuguese]")
{
    const FTDictionaryEntry Verb = MakeFalar();

    CHECK(ReadTargetWords(ExpandPortuguese(FormsKindVerb, Verb))
          == std::vector<std::string>{"eu falo", "tu falas", "ele fala", "nós falamos", "vós falais", "eles falam"});
    CHECK(ReadTargetWords(ExpandPortuguese(FormsKindVerb, Verb, "preterite"))
          == std::vector<std::string>{"eu falei", "tu falaste", "ele falou", "nós falámos", "vós falastes",
                                      "eles falaram"});
    CHECK(ReadTargetWords(ExpandPortuguese(FormsKindVerb, Verb, "imperfect")).front() == "eu falava");
    CHECK(ReadTargetWords(ExpandPortuguese(FormsKindVerb, Verb, "future")).back() == "eles falarão");
    CHECK(ReadTargetWords(ExpandPortuguese(FormsKindVerb, Verb, "perfect"))
          == std::vector<std::string>{"eu tenho falado", "tu tens falado", "ele tem falado", "nós temos falado",
                                      "vós tendes falado", "eles têm falado"});

    CHECK(ExpandPortuguese(FormsKindVerb, Verb, "preterite")[3].Id == "conjugation-pt-falar-preterite#1-2");
}

TEST_CASE("Portuguese tenses mark their own endings", "[conjugation][portuguese]")
{
    const FTDictionaryEntry Verb = MakeFalar();

    const std::optional<FTWordForm> Preterite = ReadWordForm(ExpandPortuguese(FormsKindVerb, Verb, "preterite")[3]);
    REQUIRE(Preterite.has_value());
    CHECK(Preterite->Subject == "nós");
    CHECK(Preterite->Stem == "fal");
    CHECK(Preterite->Ending == "ámos");

    const std::optional<FTWordForm> Imperfect = ReadWordForm(ExpandPortuguese(FormsKindVerb, Verb, "imperfect")[4]);
    REQUIRE(Imperfect.has_value());
    CHECK(Imperfect->Stem == "fal");
    CHECK(Imperfect->Ending == "áveis");

    const std::optional<FTWordForm> Future = ReadWordForm(ExpandPortuguese(FormsKindVerb, Verb, "future")[5]);
    REQUIRE(Future.has_value());
    CHECK(Future->Stem == "falar");
    CHECK(Future->Ending == "ão");

    const std::optional<FTWordForm> Perfect = ReadWordForm(ExpandPortuguese(FormsKindVerb, Verb, "perfect")[0]);
    REQUIRE(Perfect.has_value());
    CHECK(Perfect->Subject == "eu");
    CHECK(Perfect->Form == "tenho falado");
}

TEST_CASE("A Portuguese adjective is offered only when a form departs from the plain endings",
          "[conjugation][portuguese]")
{
    FTDictionaryEntry Grande = MakeAdjective("grande", "", "", "");
    Grande.Forms.push_back(MakeAgreementForm("grandes", 0, 2));

    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, MakeAdjective("novo", "nova", "novos", "novas"), "pt", "new")
                    .has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, Grande, "pt", "big").has_value());

    CHECK(ReadTargetWords(ExpandPortuguese(FormsKindAdjective, MakeAdjective("bom", "boa", "bons", "boas")))
          == std::vector<std::string>{"ele bom", "ela boa", "eles bons", "elas boas"});

    const std::vector<FTCard> Easy = ExpandPortuguese(FormsKindAdjective, MakeAdjective("fácil", "", "fáceis", ""));

    REQUIRE(Easy.size() == 2);
    CHECK(Easy[1].TargetWord == "eles fáceis");
}

TEST_CASE("A Portuguese noun is offered only when its plural is not the plain one", "[conjugation][portuguese]")
{
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("livro", GenderMasculine, "livros"), "pt", "book").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("mulher", GenderFeminine, "mulheres"), "pt", "woman")
                    .has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("mão", GenderFeminine, "mãos"), "pt", "hand").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("irmã", GenderFeminine, "irmãs"), "pt", "sister").has_value());

    CHECK(ReadTargetWords(ExpandPortuguese(FormsKindNoun, MakeNoun("pão", GenderMasculine, "pães")))
          == std::vector<std::string>{"um pão", "uns pães"});
    CHECK(ReadTargetWords(ExpandPortuguese(FormsKindNoun, MakeNoun("viagem", GenderFeminine, "viagens")))
          == std::vector<std::string>{"uma viagem", "umas viagens"});
    CHECK(ReadTargetWords(ExpandPortuguese(FormsKindNoun, MakeNoun("animal", GenderMasculine, "animais")))
          == std::vector<std::string>{"um animal", "uns animais"});
}

static std::vector<FTCard> ExpandItalian(const std::string& Kind, const FTDictionaryEntry& Entry,
                                         const std::string& Tense = FormsTensePresent)
{
    const std::optional<FTCard> Stored = MakeFormsCard(Kind, Entry, "it", "translation", Tense);
    REQUIRE(Stored.has_value());

    return ExpandFormsCard(*Stored, Entry, "it");
}

static FTDictionaryEntry MakeParlare()
{
    FTDictionaryEntry Verb = MakeVerb("parlare", {"parlo", "parli", "parla", "parliamo", "parlate", "parlano"});

    AddTense(Verb, TenseImperfect, {"parlavo", "parlavi", "parlava", "parlavamo", "parlavate", "parlavano"});
    AddTense(Verb, TensePreterite, {"parlai", "parlasti", "parlò", "parlammo", "parlaste", "parlarono"});
    AddTense(Verb, TenseFuture, {"parlerò", "parlerai", "parlerà", "parleremo", "parlerete", "parleranno"});
    AddParticiple(Verb, "parlato");

    return Verb;
}

TEST_CASE("Italian has five tenses and every later one opens from the present", "[conjugation][italian]")
{
    CHECK(ListFormsTenses("it")
          == std::vector<std::string>{"present", "perfect", "imperfect", "future", "preterite"});

    CHECK(GetFormsTenseLabel("it", "present") == "presente");
    CHECK(GetFormsTenseLabel("it", "perfect") == "passato prossimo");
    CHECK(GetFormsTenseLabel("it", "imperfect") == "imperfetto");
    CHECK(GetFormsTenseLabel("it", "future") == "futuro");
    CHECK(GetFormsTenseLabel("it", "preterite") == "passato remoto");

    for (const std::string Tense : {"perfect", "imperfect", "future", "preterite"})
    {
        CHECK(GetFormsTenseRequirement("it", Tense) == "present");
    }

    CHECK(GetFormsTenseRequirement("it", "present").empty());
}

TEST_CASE("An Italian verb expands into each of its tenses", "[conjugation][italian]")
{
    const FTDictionaryEntry Verb = MakeParlare();

    CHECK(ReadTargetWords(ExpandItalian(FormsKindVerb, Verb))
          == std::vector<std::string>{"io parlo", "tu parli", "lui parla", "noi parliamo", "voi parlate",
                                      "loro parlano"});
    CHECK(ReadTargetWords(ExpandItalian(FormsKindVerb, Verb, "perfect"))
          == std::vector<std::string>{"io ho parlato", "tu hai parlato", "lui ha parlato", "noi abbiamo parlato",
                                      "voi avete parlato", "loro hanno parlato"});
    CHECK(ReadTargetWords(ExpandItalian(FormsKindVerb, Verb, "imperfect")).front() == "io parlavo");
    CHECK(ReadTargetWords(ExpandItalian(FormsKindVerb, Verb, "future")).back() == "loro parleranno");
    CHECK(ReadTargetWords(ExpandItalian(FormsKindVerb, Verb, "preterite"))[2] == "lui parlò");

    CHECK(ReadWordForm(ExpandItalian(FormsKindVerb, Verb)[2])->SubjectLabel == "lui/lei");
}

TEST_CASE("An Italian verb of movement takes essere and its participle agrees in the plural",
          "[conjugation][italian]")
{
    FTDictionaryEntry Andare = MakeVerb("andare", {"vado", "vai", "va", "andiamo", "andate", "vanno"});
    AddParticiple(Andare, "andato");

    CHECK(ReadTargetWords(ExpandItalian(FormsKindVerb, Andare, "perfect"))
          == std::vector<std::string>{"io sono andato", "tu sei andato", "lui è andato", "noi siamo andati",
                                      "voi siete andati", "loro sono andati"});

    FTDictionaryEntry Essere = MakeVerb("essere", {"sono", "sei", "è", "siamo", "siete", "sono"});
    AddParticiple(Essere, "stato");

    CHECK(ReadTargetWords(ExpandItalian(FormsKindVerb, Essere, "perfect")).back() == "loro sono stati");
}

TEST_CASE("Italian nouns and adjectives are not offered for agreement", "[conjugation][italian]")
{
    CHECK_FALSE(MakeFormsCard(FormsKindAdjective, MakeAdjective("bello", "bella", "belli", "belle"), "it",
                              "beautiful").has_value());
    CHECK_FALSE(MakeFormsCard(FormsKindNoun, MakeNoun("uomo", GenderMasculine, "uomini"), "it", "man").has_value());
}

TEST_CASE("An Italian verb is irregular when its stem or endings leave the model", "[conjugation][irregular]")
{
    CHECK_FALSE(IsIrregularVerb("it", MakeParlare()));
    CHECK_FALSE(IsIrregularVerb("it", MakeVerb("credere", {"credo", "credi", "crede", "crediamo", "credete", "credono"})));
    CHECK_FALSE(IsIrregularVerb("it", MakeVerb("dormire", {"dormo", "dormi", "dorme", "dormiamo", "dormite", "dormono"})));
    CHECK_FALSE(IsIrregularVerb("it", MakeVerb("finire", {"finisco", "finisci", "finisce", "finiamo", "finite", "finiscono"})));

    FTDictionaryEntry Cercare = MakeVerb("cercare", {"cerco", "cerchi", "cerca", "cerchiamo", "cercate", "cercano"});
    AddTense(Cercare, TenseFuture, {"cercherò", "cercherai", "cercherà", "cercheremo", "cercherete", "cercheranno"});
    CHECK_FALSE(IsIrregularVerb("it", Cercare));

    CHECK(IsIrregularVerb("it", MakeVerb("andare", {"vado", "vai", "va", "andiamo", "andate", "vanno"})));
    CHECK(IsIrregularVerb("it", MakeVerb("essere", {"sono", "sei", "è", "siamo", "siete", "sono"})));

    FTDictionaryEntry Vedere = MakeVerb("vedere", {"vedo", "vedi", "vede", "vediamo", "vedete", "vedono"});
    AddTense(Vedere, TenseFuture, {"vedrò", "vedrai", "vedrà", "vedremo", "vedrete", "vedranno"});
    CHECK(IsIrregularVerb("it", Vedere));
}

TEST_CASE("A French verb is irregular when it is not of the first or second group", "[conjugation][irregular]")
{
    CHECK_FALSE(IsIrregularVerb("fr", MakeAimer()));
    CHECK_FALSE(IsIrregularVerb("fr", MakeVerb("payer", {"paie", "paies", "paie", "payons", "payez", "paient"})));
    CHECK_FALSE(IsIrregularVerb("fr", MakeVerb("finir", {"finis", "finis", "finit", "finissons", "finissez", "finissent"})));

    CHECK(IsIrregularVerb("fr", MakeVouloir()));
    CHECK(IsIrregularVerb("fr", MakeVerb("partir", {"pars", "pars", "part", "partons", "partez", "partent"})));
    CHECK(IsIrregularVerb("fr", MakeVerb("aller", {"vais", "vas", "va", "allons", "allez", "vont"})));
    CHECK(IsIrregularVerb("fr", MakeVerb("rendre", {"rends", "rends", "rend", "rendons", "rendez", "rendent"})));
}

TEST_CASE("A Spanish verb is irregular when its stem or endings leave the model", "[conjugation][irregular]")
{
    CHECK_FALSE(IsIrregularVerb("es", MakeVerb("amar", {"amo", "amas", "ama", "amamos", "amáis", "aman"})));
    CHECK_FALSE(IsIrregularVerb("es", MakeVerb("comer", {"como", "comes", "come", "comemos", "coméis", "comen"})));
    CHECK_FALSE(IsIrregularVerb("es", MakeVerb("vivir", {"vivo", "vives", "vive", "vivimos", "vivís", "viven"})));

    CHECK(IsIrregularVerb("es", MakeVerb("pensar", {"pienso", "piensas", "piensa", "pensamos", "pensáis", "piensan"})));
    CHECK(IsIrregularVerb("es", MakeVerb("tener", {"tengo", "tienes", "tiene", "tenemos", "tenéis", "tienen"})));
    CHECK(IsIrregularVerb("es", MakeVerb("ser", {"soy", "eres", "es", "somos", "sois", "son"})));
}

TEST_CASE("A Spanish verb that only changes its spelling stays regular", "[conjugation][irregular]")
{
    FTDictionaryEntry Buscar = MakeVerb("buscar", {"busco", "buscas", "busca", "buscamos", "buscáis", "buscan"});
    AddTense(Buscar, TensePreterite, {"busqué", "buscaste", "buscó", "buscamos", "buscasteis", "buscaron"});

    FTDictionaryEntry Leer = MakeVerb("leer", {"leo", "lees", "lee", "leemos", "leéis", "leen"});
    AddTense(Leer, TensePreterite, {"leí", "leíste", "leyó", "leímos", "leísteis", "leyeron"});

    CHECK_FALSE(IsIrregularVerb("es", Buscar));
    CHECK_FALSE(IsIrregularVerb("es", Leer));
}

TEST_CASE("A Spanish verb with one irregular tense is irregular", "[conjugation][irregular]")
{
    FTDictionaryEntry Andar = MakeVerb("andar", {"ando", "andas", "anda", "andamos", "andáis", "andan"});
    AddTense(Andar, TensePreterite, {"anduve", "anduviste", "anduvo", "anduvimos", "anduvisteis", "anduvieron"});

    CHECK(IsIrregularVerb("es", Andar));
}

TEST_CASE("A Portuguese verb is irregular when its stem or endings leave the model", "[conjugation][irregular]")
{
    CHECK_FALSE(IsIrregularVerb("pt", MakeVerb("falar", {"falo", "falas", "fala", "falamos", "falais", "falam"})));
    CHECK_FALSE(IsIrregularVerb("pt", MakeVerb("vender", {"vendo", "vendes", "vende", "vendemos", "vendeis", "vendem"})));

    CHECK(IsIrregularVerb("pt", MakeVerb("fazer", {"faço", "fazes", "faz", "fazemos", "fazeis", "fazem"})));
    CHECK(IsIrregularVerb("pt", MakeVerb("dormir", {"durmo", "dormes", "dorme", "dormimos", "dormis", "dormem"})));
    CHECK(IsIrregularVerb("pt", MakeVerb("pôr", {"ponho", "pões", "põe", "pomos", "pondes", "põem"})));
}

TEST_CASE("Only a verb can be irregular", "[conjugation][irregular]")
{
    CHECK_FALSE(IsIrregularVerb("fr", MakeNoun("cheval", GenderMasculine, "chevaux")));
    CHECK_FALSE(IsIrregularVerb("de", MakeVouloir()));
}

TEST_CASE("A verb that conjugates as a prefix plus another verb is derived from it", "[conjugation][irregular]")
{
    const FTDictionaryEntry Prendre = MakeVerb("prendre", {"prends", "prends", "prend", "prenons", "prenez", "prennent"});
    const FTDictionaryEntry Comprendre =
        MakeVerb("comprendre", {"comprends", "comprends", "comprend", "comprenons", "comprenez", "comprennent"});
    const FTDictionaryEntry Tener = MakeVerb("tener", {"tengo", "tienes", "tiene", "tenemos", "tenéis", "tienen"});
    const FTDictionaryEntry Obtener =
        MakeVerb("obtener", {"obtengo", "obtienes", "obtiene", "obtenemos", "obtenéis", "obtienen"});

    CHECK(IsDerivedVerb("fr", FormsTensePresent, Comprendre, Prendre));
    CHECK(IsDerivedVerb("es", FormsTensePresent, Obtener, Tener));

    CHECK_FALSE(IsDerivedVerb("fr", FormsTensePresent, Prendre, Comprendre));
    CHECK_FALSE(IsDerivedVerb("fr", FormsTensePresent, Prendre, Prendre));
}

TEST_CASE("A verb that only ends like another verb is not derived from it", "[conjugation][irregular]")
{
    const FTDictionaryEntry Avoir = MakeVerb("avoir", {"ai", "as", "a", "avons", "avez", "ont"});
    const FTDictionaryEntry Savoir = MakeVerb("savoir", {"sais", "sais", "sait", "savons", "savez", "savent"});

    CHECK_FALSE(IsDerivedVerb("fr", FormsTensePresent, Savoir, Avoir));
}

TEST_CASE("A derived verb in a compound tense adds its prefix to the participle", "[conjugation][irregular]")
{
    FTDictionaryEntry Prendre = MakeVerb("prendre", {"prends", "prends", "prend", "prenons", "prenez", "prennent"});
    AddParticiple(Prendre, "pris");

    FTDictionaryEntry Comprendre =
        MakeVerb("comprendre", {"comprends", "comprends", "comprend", "comprenons", "comprenez", "comprennent"});
    AddParticiple(Comprendre, "compris");

    FTDictionaryEntry Surprendre =
        MakeVerb("surprendre", {"surprends", "surprends", "surprend", "surprenons", "surprenez", "surprennent"});
    AddParticiple(Surprendre, "surprendu");

    CHECK(IsDerivedVerb("fr", "perfect", Comprendre, Prendre));
    CHECK_FALSE(IsDerivedVerb("fr", "perfect", Surprendre, Prendre));
}

static bool PlacesGap(const FTCard& Form, const std::string& Text, const std::string& Expected = {})
{
    FTTreeSentence Sentence;
    Sentence.Text = Text;

    if (!PlaceFormGap(Form, Sentence))
    {
        return false;
    }

    return Expected.empty() || GetGapWord(Sentence) == Expected;
}

static constexpr size_t FirstSingular = 0;
static constexpr size_t ThirdSingular = 2;
static constexpr size_t FirstPlural = 3;
static constexpr size_t SecondPlural = 4;

TEST_CASE("A verb form is placed in a sentence only where its person speaks it", "[conjugation][gap]")
{
    const std::vector<FTCard> Aimer = ExpandVerb(MakeAimer());

    CHECK(PlacesGap(Aimer[FirstSingular], "J'aime les pommes.", "aime"));
    CHECK_FALSE(PlacesGap(Aimer[ThirdSingular], "J'aime les pommes."));

    CHECK(PlacesGap(Aimer[ThirdSingular], "Marie aime le chocolat.", "aime"));
    CHECK_FALSE(PlacesGap(Aimer[FirstSingular], "Marie aime le chocolat."));

    CHECK(PlacesGap(Aimer[ThirdSingular], "Il nous aime beaucoup.", "aime"));
    CHECK(PlacesGap(Aimer[FirstSingular], "Je ne l'aime pas.", "aime"));
    CHECK(PlacesGap(Aimer[FirstPlural], "Nous vous aimons.", "aimons"));
}

TEST_CASE("A French form that opens a clause with no subject is an imperative and is left out", "[conjugation][gap]")
{
    const std::vector<FTCard> Vouloir = ExpandVerb(MakeVouloir());

    CHECK_FALSE(PlacesGap(Vouloir[FirstPlural], "Voulons la paix."));
    CHECK_FALSE(PlacesGap(Vouloir[FirstPlural], "Tom, voulons la paix."));
    CHECK(PlacesGap(Vouloir[FirstPlural], "Nous voulons la paix.", "voulons"));
}

TEST_CASE("An inverted subject names the person of the form", "[conjugation][gap]")
{
    const std::vector<FTCard> Vouloir = ExpandVerb(MakeVouloir());

    CHECK(PlacesGap(Vouloir[SecondPlural], "Voulez-vous danser ?", "Voulez"));
    CHECK(PlacesGap(Vouloir[ThirdSingular], "Que veut-il dire ?", "veut"));
}

TEST_CASE("A Spanish form needs no subject in its sentence", "[conjugation][gap]")
{
    const std::vector<FTCard> Hablar =
        ExpandVerb(MakeVerb("hablar", {"hablo", "hablas", "habla", "hablamos", "habláis", "hablan"}), "es");

    CHECK(PlacesGap(Hablar[FirstPlural], "Hablamos español en casa.", "Hablamos"));
    CHECK_FALSE(PlacesGap(Hablar[FirstPlural], "Ellos hablamos."));
}

TEST_CASE("A noun or adjective form is placed wherever it stands", "[forms][gap]")
{
    const std::vector<FTCard> Beau = ExpandWord(FormsKindAdjective, MakeBeau());

    const auto Feminine = std::find_if(Beau.begin(), Beau.end(), [](const FTCard& Form)
    {
        const std::optional<FTWordForm> Read = ReadWordForm(Form);

        return Read.has_value() && Read->bFeminine && !Read->bPlural;
    });

    REQUIRE(Feminine != Beau.end());

    CHECK(PlacesGap(*Feminine, "Belle journée, n'est-ce pas ?", "Belle"));
    CHECK(PlacesGap(*Feminine, "Elle est très belle.", "belle"));
    CHECK_FALSE(PlacesGap(*Feminine, "Ils sont beaux."));
}

TEST_CASE("A round id reads back as its form", "[conjugation][gap]")
{
    const std::vector<FTCard> Vouloir = ExpandVerb(MakeVouloir());

    FTCard Round = Vouloir[FirstPlural];
    Round.Id = MakeFormsRoundId(Round.Id, 2);

    const std::optional<FTWordForm> Read = ReadWordForm(Round);

    REQUIRE(Read.has_value());
    CHECK(Read->Form == "voulons");
    CHECK(GetFormsItemId(Round.Id) == Vouloir[FirstPlural].Id);
    CHECK(GetFormsCardId(Round.Id) == "conjugation-fr-vouloir-present");
}

static std::optional<FTFormGap> LocateGap(const FTCard& Form, const std::string& Text)
{
    FTTreeSentence Sentence;
    Sentence.Text = Text;

    return LocateFormGap(Form, Sentence);
}

TEST_CASE("A gap says whether the sentence speaks the pronoun of its form", "[conjugation][gap]")
{
    const std::vector<FTCard> Hablar = ExpandSpanish(FormsKindVerb, MakeHablar());

    const std::optional<FTFormGap> Spoken = LocateGap(Hablar[SecondPlural], "¿Vosotras habláis ladino?");
    REQUIRE(Spoken.has_value());
    CHECK(Spoken->bSubjectSpoken);

    const std::optional<FTFormGap> Dropped = LocateGap(Hablar[SecondPlural], "No habláis ladino.");
    REQUIRE(Dropped.has_value());
    CHECK_FALSE(Dropped->bSubjectSpoken);

    CHECK_FALSE(LocateGap(Hablar[FirstSingular], "Ella habla ladino.").has_value());

    const std::vector<FTCard> Aimer = ExpandVerb(MakeAimer());

    const std::optional<FTFormGap> French = LocateGap(Aimer[FirstSingular], "Je l'aime beaucoup.");
    REQUIRE(French.has_value());
    CHECK(French->bSubjectSpoken);
}

static std::vector<std::string> ListRejectedLockedSentences(const FTDictionaryPackage& Package, int& CheckedCount)
{
    std::optional<FTDatabase> Database = FTDatabase::OpenReadOnly(Package.FilePath);
    REQUIRE(Database.has_value());

    const FTDictionaryRepository Repository(*Database, "en");

    std::vector<std::string> Rejected;

    FTStatement Query = Database->Prepare("SELECT DISTINCT headword, kind, tense FROM form_sentences");
    REQUIRE(Query.IsValid());

    while (Query.Step())
    {
        const std::string Headword = Query.GetText(0);
        const std::string Kind = Query.GetText(1);
        const std::string Tense = Query.GetText(2);
        const std::string Label = Headword + " " + Kind + " " + Tense;

        std::optional<FTDictionaryEntry> Entry = Repository.LookUpForForms(Headword, Kind);

        if (!Entry.has_value() && Kind == FormsKindAdjective)
        {
            Entry = Repository.LookUpForForms(Headword, "det");
        }

        const std::optional<FTCard> Stored = Entry.has_value()
            ? MakeFormsCard(Kind, *Entry, Package.TargetLanguage, "translation", Tense)
            : std::nullopt;

        if (!Stored.has_value())
        {
            Rejected.push_back(Label + ": no forms card");

            continue;
        }

        const std::vector<FTCard> Forms = ExpandFormsCard(*Stored, *Entry, Package.TargetLanguage);
        const std::vector<FTFormSentence> Rows = Repository.FindFormSentences(Headword, Kind, Tense);

        std::vector<uint32_t> SentenceIds;

        for (const FTFormSentence& Row : Rows)
        {
            SentenceIds.push_back(Row.SentenceId);
        }

        std::unordered_map<uint32_t, FTTreeSentence> LoadedById;

        for (FTTreeSentence& Sentence : Repository.LoadTreeSentences(SentenceIds))
        {
            const uint32_t Id = Sentence.Id;
            LoadedById.emplace(Id, std::move(Sentence));
        }

        for (const FTFormSentence& Row : Rows)
        {
            ++CheckedCount;

            const auto Form = std::find_if(Forms.begin(), Forms.end(), [&Row](const FTCard& Candidate)
            {
                const std::optional<FTFormsSentenceKey> Key = ReadFormsSentenceKey(Candidate);

                return Key.has_value() && Key->First == Row.First && Key->Number == Row.Number;
            });

            const auto Sentence = LoadedById.find(Row.SentenceId);

            if (Form == Forms.end() || Sentence == LoadedById.end() || !LocateFormGap(*Form, Sentence->second))
            {
                Rejected.push_back(Label + " " + std::to_string(Row.First) + "-"
                                   + std::to_string(Row.Number) + ": "
                                   + (Sentence != LoadedById.end() ? Sentence->second.Text : "missing"));
            }
        }
    }

    return Rejected;
}

TEST_CASE("Every locked sentence of a built dictionary holds its form where the app looks for it",
          "[conjugation][gap][slow]")
{
    int PackageCount = 0;

    for (const FTDictionaryPackage& Package : FindDictionaryPackages(DICTIONARY_PACKAGES_PATH))
    {
        if (!SupportsForms(FormsKindVerb, Package.TargetLanguage))
        {
            continue;
        }

        std::optional<FTDatabase> Database = FTDatabase::OpenReadOnly(Package.FilePath);
        REQUIRE(Database.has_value());

        if (!FTDictionaryRepository(*Database, "en").HasFormSentences())
        {
            continue;
        }

        ++PackageCount;

        int CheckedCount = 0;
        const std::vector<std::string> Rejected = ListRejectedLockedSentences(Package, CheckedCount);

        INFO(Package.TargetLanguage << ": " << Rejected.size() << " rejected of " << CheckedCount
                                    << ", first: " << (Rejected.empty() ? std::string() : Rejected.front()));
        CHECK(CheckedCount > 10000);
        CHECK(Rejected.empty());
    }

    if (PackageCount == 0)
    {
        SUCCEED("no built dictionary has form sentences");
    }
}

TEST_CASE("Verb colours classify conjugation groups across languages", "[conjugation][verbgroup]")
{
    CHECK(GetVerbGroup("fr", MakeAimer()) == "first");
    CHECK(GetVerbGroup("fr", MakeVerb("finir", {"finis", "finis", "finit", "finissons", "finissez", "finissent"})) == "second");
    CHECK(GetVerbGroup("fr", MakeVouloir()) == "third");
    CHECK(GetVerbGroup("fr", MakeVerb("aller", {"vais", "vas", "va", "allons", "allez", "vont"})) == "third");
    CHECK(GetVerbGroup("es", MakeVerb("amar", {"amo", "amas", "ama", "amamos", "amáis", "aman"})) == "first");
    CHECK(GetVerbGroup("es", MakeVerb("comer", {"como", "comes", "come", "comemos", "coméis", "comen"})) == "second");
    CHECK(GetVerbGroup("es", MakeVerb("vivir", {"vivo", "vives", "vive", "vivimos", "vivís", "viven"})) == "third");
    CHECK(GetVerbGroup("es", MakeVerb("tener", {"tengo", "tienes", "tiene", "tenemos", "tenéis", "tienen"})) == "irregular");
    CHECK(GetVerbGroup("it", MakeParlare()) == "first");
    CHECK(GetVerbGroup("it", MakeVerb("credere", {"credo", "credi", "crede", "crediamo", "credete", "credono"})) == "second");
    CHECK(GetVerbGroup("it", MakeVerb("finire", {"finisco", "finisci", "finisce", "finiamo", "finite", "finiscono"})) == "third");
    CHECK(GetVerbGroup("it", MakeVerb("andare", {"vado", "vai", "va", "andiamo", "andate", "vanno"})) == "irregular");
    CHECK(GetVerbGroup("pt", MakeVerb("falar", {"falo", "falas", "fala", "falamos", "falais", "falam"})) == "first");
    CHECK(GetVerbGroup("pt", MakeVerb("vender", {"vendo", "vendes", "vende", "vendemos", "vendeis", "vendem"})) == "second");
    CHECK(GetVerbGroup("pt", MakeVerb("partir", {"parto", "partes", "parte", "partimos", "partis", "partem"})) == "third");
    CHECK(GetVerbGroup("pt", MakeVerb("fazer", {"faço", "fazes", "faz", "fazemos", "fazeis", "fazem"})) == "irregular");
    CHECK(GetVerbGroup("fr", MakeNoun("cheval", GenderMasculine, "chevaux")).empty());
    CHECK(GetVerbGroup("de", MakeVouloir()).empty());
}
