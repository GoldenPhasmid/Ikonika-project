#include "AppCards.h"

#include "AppDatabase.h"
#include "AppDictionary.h"

#include "core/FTArticleRules.h"
#include "core/FTCardMeaning.h"
#include "core/FTWordForms.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QVariantMap>
#include <algorithm>
#include <array>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace
{
    std::unordered_map<std::string, bool> VerbSpellings;
    std::unordered_map<std::string, bool> StressedSpellings;
    std::unordered_map<std::string, bool> ElidingSpellings;

    std::unordered_map<std::string, std::optional<FTDictionaryEntry>> FormsEntries;
    std::unordered_map<std::string, std::optional<FTCard>> FormsCardPrototypes;
    uint64_t FormsCacheGeneration = 0;
    int FormsLookupCount = 0;

    void DropStaleFormsCaches()
    {
        const uint64_t Generation = AppDictionary::Get().GetGeneration();

        if (Generation == FormsCacheGeneration)
        {
            return;
        }

        FormsEntries.clear();
        FormsCardPrototypes.clear();
        FormsCacheGeneration = Generation;
    }

    const std::optional<FTDictionaryEntry>& FindFormsEntry(const std::string& Headword, const std::string& PartOfSpeech)
    {
        DropStaleFormsCaches();

        const std::string Key = Headword + "/" + PartOfSpeech;
        const auto Known = FormsEntries.find(Key);

        if (Known != FormsEntries.end())
        {
            return Known->second;
        }

        ++FormsLookupCount;

        return FormsEntries.emplace(Key, AppDictionary::Get().GetRepository().LookUpForForms(Headword, PartOfSpeech))
            .first->second;
    }

    const std::optional<FTCard>& FindFormsCardPrototype(const std::string& Headword, const std::string& PartOfSpeech,
                                                        const std::string& TargetLanguage)
    {
        const std::optional<FTDictionaryEntry>& Entry = FindFormsEntry(Headword, PartOfSpeech);

        const std::string Key = Headword + "/" + PartOfSpeech;
        const auto Known = FormsCardPrototypes.find(Key);

        if (Known != FormsCardPrototypes.end())
        {
            return Known->second;
        }

        std::optional<FTCard> Prototype;

        if (Entry.has_value())
        {
            Prototype = MakeFormsCard(PartOfSpeech, *Entry, TargetLanguage, std::string());
        }

        return FormsCardPrototypes.emplace(Key, std::move(Prototype)).first->second;
    }

    bool SpellingElidesArticle(const std::string& Headword)
    {
        if (Headword.empty() || !AppDictionary::Get().IsReady())
        {
            return false;
        }

        // Only an h is ever in doubt. Every other spelling is already settled by the vowel rule,
        // and answering yes for one would contract an article before a consonant.
        if (Headword.front() != 'h' && Headword.front() != 'H')
        {
            return false;
        }

        const auto Known = ElidingSpellings.find(Headword);

        if (Known != ElidingSpellings.end())
        {
            return Known->second;
        }

        bool bKeepsFullArticle = false;

        for (const FTDictionaryEntry& Entry : AppDictionary::Get().GetRepository().LookUp(Headword))
        {
            bKeepsFullArticle = bKeepsFullArticle
                || std::find(Entry.Tags.begin(), Entry.Tags.end(), "aspirated-h")
                       != Entry.Tags.end()
                || std::find(Entry.Tags.begin(), Entry.Tags.end(), "variable-h")
                       != Entry.Tags.end();
        }
        
        const bool bElides = !bKeepsFullArticle;

        ElidingSpellings.emplace(Headword, bElides);

        return bElides;
    }

    bool SpellingIsStressedFirst(const std::string& Headword)
    {
        if (Headword.empty() || !AppDictionary::Get().IsReady())
        {
            return false;
        }

        const auto Known = StressedSpellings.find(Headword);

        if (Known != StressedSpellings.end())
        {
            return Known->second;
        }

        bool bStressedFirst = false;

        for (const FTDictionaryEntry& Entry : AppDictionary::Get().GetRepository().LookUp(Headword))
        {
            if (IsFirstSyllableStressed(Entry.Phonetic))
            {
                bStressedFirst = true;

                break;
            }
        }

        StressedSpellings.emplace(Headword, bStressedFirst);

        return bStressedFirst;
    }

    bool SpellingIsAlsoAVerb(const std::string& Headword)
    {
        if (Headword.empty() || !AppDictionary::Get().IsReady())
        {
            return false;
        }

        const auto Known = VerbSpellings.find(Headword);

        if (Known != VerbSpellings.end())
        {
            return Known->second;
        }

        const bool bIsAlsoAVerb =
            AppDictionary::Get().GetRepository().HasReading(Headword, "verb");

        VerbSpellings.emplace(Headword, bIsAlsoAVerb);

        return bIsAlsoAVerb;
    }
}

namespace
{
    struct FNounFacts
    {
        std::unordered_set<std::string> VerbHeadwords;
        std::unordered_set<std::string> KeepsFullArticle;
        bool bWantsElision = false;
        bool bWantsStress = false;
    };

    bool StartsWithH(const std::string& Word)
    {
        return !Word.empty() && (Word.front() == 'h' || Word.front() == 'H');
    }

    FNounFacts LoadNounFacts(const std::vector<const FTCard*>& Cards)
    {
        const std::string& TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;

        FNounFacts Facts;
        Facts.bWantsElision = TargetLanguage == "fr";
        Facts.bWantsStress = TargetLanguage == "es";

        std::vector<std::string> NounHeadwords;
        std::vector<std::string> HeadwordsWithH;
        std::unordered_set<std::string> SeenNouns;

        for (const FTCard* Card : Cards)
        {
            if (Card->PartOfSpeech != "noun" || !SeenNouns.insert(Card->TargetWord).second)
            {
                continue;
            }

            NounHeadwords.push_back(Card->TargetWord);

            if (Facts.bWantsElision && StartsWithH(Card->TargetWord))
            {
                HeadwordsWithH.push_back(Card->TargetWord);
            }
        }

        if (NounHeadwords.empty())
        {
            return Facts;
        }

        FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();

        Facts.VerbHeadwords = Dictionary.FindHeadwordsWithReading(NounHeadwords, "verb");

        if (HeadwordsWithH.empty())
        {
            return Facts;
        }

        Facts.KeepsFullArticle = Dictionary.FindHeadwordsWithTag(HeadwordsWithH, "aspirated-h");

        for (const std::string& Headword : Dictionary.FindHeadwordsWithTag(HeadwordsWithH, "variable-h"))
        {
            Facts.KeepsFullArticle.insert(Headword);
        }

        return Facts;
    }

    void ApplyNounFacts(FTCard& Subject, const FNounFacts& Facts)
    {
        if (Subject.PartOfSpeech != "noun")
        {
            Subject.bSpellingIsAlsoAVerb = false;
            Subject.bStressedFirstSyllable = false;
            Subject.bElidesArticle = false;

            return;
        }

        Subject.bSpellingIsAlsoAVerb = Facts.VerbHeadwords.count(Subject.TargetWord) > 0;

        Subject.bStressedFirstSyllable = Facts.bWantsStress
            && Subject.Gender.value_or(GenderMasculine) == GenderFeminine
            && SpellingIsStressedFirst(Subject.TargetWord);

        Subject.bElidesArticle = Facts.bWantsElision && StartsWithH(Subject.TargetWord)
            && Facts.KeepsFullArticle.count(Subject.TargetWord) == 0;
    }

    std::optional<int> ToOptionalGender(int Gender)
    {
        return Gender >= 0 ? std::optional<int>{Gender} : std::nullopt;
    }
}

namespace AppCards
{
    void Annotate(FTCard& Card)
    {
        if (Card.PartOfSpeech != "noun")
        {
            Card.bSpellingIsAlsoAVerb = false;
            Card.bStressedFirstSyllable = false;
            Card.bElidesArticle = false;

            return;
        }

        const std::string& TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;

        Card.bSpellingIsAlsoAVerb = SpellingIsAlsoAVerb(Card.TargetWord);

        Card.bStressedFirstSyllable = TargetLanguage == "es"
            && Card.Gender.value_or(GenderMasculine) == GenderFeminine
            && SpellingIsStressedFirst(Card.TargetWord);

        Card.bElidesArticle = TargetLanguage == "fr" && SpellingElidesArticle(Card.TargetWord);
    }

    void Annotate(const std::vector<FTCardHandle>& Cards)
    {
        const FTCardStore Store;

        if (!AppDictionary::Get().IsReady())
        {
            for (const FTCardHandle& Card : Cards)
            {
                Store.MutateCard(Card, [](FTCard& Subject) { Annotate(Subject); });
            }

            return;
        }

        std::vector<const FTCard*> Subjects;
        Subjects.reserve(Cards.size());

        for (const FTCardHandle& Card : Cards)
        {
            Subjects.push_back(Card.get());
        }

        const FNounFacts Facts = LoadNounFacts(Subjects);

        for (const FTCardHandle& Card : Cards)
        {
            Store.MutateCard(Card, [&Facts](FTCard& Subject) { ApplyNounFacts(Subject, Facts); });
        }
    }

    void Annotate(std::vector<FTCard>& Cards)
    {
        if (!AppDictionary::Get().IsReady())
        {
            for (FTCard& Card : Cards)
            {
                Annotate(Card);
            }

            return;
        }

        std::vector<const FTCard*> Subjects;
        Subjects.reserve(Cards.size());

        for (const FTCard& Card : Cards)
        {
            Subjects.push_back(&Card);
        }

        const FNounFacts Facts = LoadNounFacts(Subjects);

        for (FTCard& Card : Cards)
        {
            ApplyNounFacts(Card, Facts);
        }
    }

    void AnnotateLoadedCards(FTCardRepository& CardRepository)
    {
        CardRepository.SetCardAnnotator([](std::vector<FTCard>& Cards) { Annotate(Cards); });
    }

    void FillReadingFromDictionary(FTCard& Card)
    {
        const bool bWantsPartOfSpeech = Card.PartOfSpeech.empty();
        const bool bWantsGender = !Card.Gender.has_value()
            && !IsUserEdited(Card, EFTCardField::Gender);

        if (Card.TargetWord.empty() || IsFormsCard(Card) || (!bWantsPartOfSpeech && !bWantsGender))
        {
            return;
        }

        if (!AppDictionary::Get().IsReady())
        {
            return;
        }

        const std::vector<FTDictionaryEntry> Found =
            AppDictionary::Get().GetRepository().LookUp(Card.TargetWord);

        if (Found.empty())
        {
            return;
        }
        
        const auto SameReading = std::find_if(Found.begin(), Found.end(),
            [&Card](const FTDictionaryEntry& Candidate) { return Candidate.PartOfSpeech == Card.PartOfSpeech; });

        const FTDictionaryEntry& Entry = SameReading != Found.end() ? *SameReading : Found.front();

        if (bWantsPartOfSpeech)
        {
            Card.PartOfSpeech = Entry.PartOfSpeech;
        }

        if (bWantsGender)
        {
            for (const FTDictionarySense& Sense : Entry.Senses)
            {
                if (Sense.Gender.has_value())
                {
                    Card.Gender = Sense.Gender;

                    break;
                }
            }
        }
    }

    FTCard MakeAddedWord(const std::string& Word, const std::string& Translation,
                         const std::string& PartOfSpeech, int Gender, bool bReadingChosen,
                         const std::string& MeaningGist)
    {
        FTCard Card;
        Card.TargetWord = Word;
        Card.Translation = Translation;
        Card.PartOfSpeech = PartOfSpeech;
        Card.MeaningGist = MeaningGist;

        SetUserEdited(Card, EFTCardField::Translation, true);

        if (bReadingChosen)
        {
            Card.Gender = ToOptionalGender(Gender);

            SetUserEdited(Card, EFTCardField::Gender, true);
            SetUserEdited(Card, EFTCardField::PartOfSpeech, true);
        }

        FillReadingFromDictionary(Card);
        FillMeaningFromDictionary(Card);

        return Card;
    }

    void FillMeaningFromDictionary(FTCard& Card)
    {
        if (!Card.MeaningGist.empty() || Card.PartOfSpeech.empty() || !AppDictionary::Get().IsReady())
        {
            return;
        }

        const FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();

        if (!Dictionary.HasMeaningExamples())
        {
            return;
        }

        const std::vector<FTDictionarySense> Meanings = Dictionary.LoadMeanings(Card.TargetWord, Card.PartOfSpeech);

        if (Meanings.empty())
        {
            return;
        }

        const std::string MatchedGist = MatchMeaningGist(Card, Meanings);

        Card.MeaningGist = MatchedGist.empty() ? PickPopularMeaningGist(Card, Meanings) : MatchedGist;
    }

    void Prepare(const std::vector<FTCardHandle>& Cards)
    {
        Annotate(Cards);
    }

    std::optional<FTCardHandle> LoadCard(FTCardRepository& CardRepository, const std::string& CardId)
    {
        return CardRepository.LoadCard(CardId);
    }

    std::vector<FTCardHandle> ExpandForms(const std::vector<FTCardHandle>& Cards)
    {
        std::vector<FTCardHandle> Result;

        const std::string TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;

        for (const FTCardHandle& Card : Cards)
        {
            if (!IsFormsCard(*Card) || IsFormsItemId(Card->Id))
            {
                Result.push_back(Card);

                continue;
            }

            std::vector<FTCard> Forms;

            if (AppDictionary::Get().IsReady())
            {
                const std::string Kind = GetFormsKind(Card->Id);
                const std::vector<FTDictionaryEntry> Entries = AppDictionary::Get().GetRepository().LookUp(Card->TargetWord);

                for (const bool bOwnKind : {true, false})
                {
                    for (const FTDictionaryEntry& Entry : Entries)
                    {
                        if ((Entry.PartOfSpeech == Kind) != bOwnKind)
                        {
                            continue;
                        }

                        Forms = ExpandFormsCard(*Card, Entry, TargetLanguage);

                        if (!Forms.empty())
                        {
                            if (!bOwnKind)
                            {
                                qCInfo(LogApp) << "Forms of" << QString::fromStdString(Card->TargetWord) << "read from its"
                                               << QString::fromStdString(Entry.PartOfSpeech) << "entry";
                            }

                            break;
                        }
                    }

                    if (!Forms.empty())
                    {
                        break;
                    }
                }
            }

            if (Forms.empty())
            {
                qCWarning(LogApp) << "No forms in the dictionary for"
                                  << QString::fromStdString(Card->TargetWord) << "- left out of the session";

                continue;
            }

            for (FTCard& Form : Forms)
            {
                Result.push_back(MakeCardHandle(std::move(Form)));
            }
        }

        return Result;
    }

    bool IsFormSkipped(const FTCard& Form)
    {
        return !FTSettings::Get().GetFormsRules().bOptionalPronoun && IsOptionalForm(Form);
    }

    std::vector<FTCardHandle> ExpandStudiedForms(const std::vector<FTCardHandle>& Cards)
    {
        std::vector<FTCardHandle> Result;

        for (const FTCardHandle& Form : ExpandForms(Cards))
        {
            if (!IsFormSkipped(*Form))
            {
                Result.push_back(Form);
            }
        }

        return Result;
    }

    QString MarkFormEnding(const FTWordForm& Read)
    {
        const QString Stem = QString::fromStdString(Read.Stem).toHtmlEscaped();

        if (Read.Ending.empty())
        {
            return Stem;
        }

        return Stem + QStringLiteral("<a href=\"ending\">") + QString::fromStdString(Read.Ending).toHtmlEscaped()
            + QStringLiteral("</a>");
    }

    QString LabelFormLead(const FTCard& Form, const FTWordForm& Read)
    {
        const std::string Lead = Form.TargetWord.substr(0, Form.TargetWord.size() - Read.Form.size());
        const std::string SpokenLead = Read.Subject + " ";

        return QString::fromStdString(Lead == SpokenLead ? Read.SubjectLabel + " " : Lead);
    }

    QString MarkFormWord(const FTCard& Form)
    {
        const std::optional<FTWordForm> Read = ReadWordForm(Form);

        if (!Read.has_value())
        {
            return {};
        }

        return LabelFormLead(Form, *Read).toHtmlEscaped() + MarkFormEnding(*Read);
    }

    QVariantMap MakeMeaningEntry(const FTDictionarySense& Meaning)
    {
        QStringList Translations;

        for (const FTDictionaryTranslation& Translation : Meaning.Translations)
        {
            Translations.append(QString::fromStdString(Translation.Text));
        }

        QVariantMap Entry;
        Entry["gist"] = QString::fromStdString(Meaning.Gist);
        Entry["definition"] = QString::fromStdString(Meaning.Definition);
        Entry["nativeDefinition"] = QString::fromStdString(Meaning.NativeDefinition);
        Entry["example"] = QString::fromStdString(Meaning.Example);
        Entry["exampleTranslation"] = QString::fromStdString(Meaning.ExampleTranslation);
        Entry["gender"] = Meaning.Gender.has_value() ? *Meaning.Gender : -1;
        Entry["translations"] = Translations;

        return Entry;
    }

    QString FindCardMeaningGist(const std::string& CardId)
    {
        if (CardId.empty() || !AppDatabase::Get().IsReady())
        {
            return {};
        }

        const std::optional<FTCardHandle> Card = AppDatabase::Get().GetCardRepository().LoadCard(CardId);

        return Card.has_value() ? QString::fromStdString((*Card)->MeaningGist) : QString();
    }

    QVariantList BuildFormsTable(const FTCard& Stored)
    {
        const std::vector<FTCardHandle> Forms = ExpandForms({MakeCardHandle(Stored)});
        const size_t PerNumber = Forms.size() / 2;

        QVariantList Rows;

        for (size_t RowIndex = 0; RowIndex < PerNumber; ++RowIndex)
        {
            QVariantList Cells;

            for (const size_t FormIndex : {RowIndex, RowIndex + PerNumber})
            {
                const FTCard& Form = *Forms[FormIndex];
                const std::optional<FTWordForm> Read = ReadWordForm(Form);

                if (!Read.has_value())
                {
                    continue;
                }

                const bool bSkipped = IsFormSkipped(Form);

                QVariantMap Cell;
                Cell[QStringLiteral("cue")] = bSkipped ? QString() : LabelFormLead(Form, *Read).trimmed();
                Cell[QStringLiteral("form")] = bSkipped ? QString() : MarkFormEnding(*Read);

                Cells.append(Cell);
            }

            QVariantMap Row;
            Row[QStringLiteral("cells")] = Cells;

            Rows.append(Row);
        }

        return Rows;
    }

    std::vector<FTenseOffer> ListTenseOffers()
    {
        std::vector<FTenseOffer> Offers;

        const std::string TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;

        if (!AppDatabase::Get().IsReady())
        {
            return Offers;
        }

        std::vector<FTCardHandle> Verbs;
        std::unordered_set<std::string> Stored;

        for (const FTCardHandle& Card : AppDatabase::Get().GetCardRepository().LoadFormsCards())
        {
            if (GetFormsKind(Card->Id) == FormsKindVerb)
            {
                Verbs.push_back(Card);
                Stored.insert(Card->Id);
            }
        }

        for (const std::string& Tense : ListFormsTenses(TargetLanguage))
        {
            FTenseOffer Offer;
            Offer.Tense = Tense;
            Offer.Label = GetFormsTenseLabel(TargetLanguage, Tense);

            Offers.push_back(std::move(Offer));
        }

        const bool bDictionaryReady = AppDictionary::Get().IsReady();

        for (const FTCardHandle& Card : Verbs)
        {
            const std::string CardTense = GetFormsTense(Card->Id);

            if (!IsCompleted(*Card))
            {
                for (FTenseOffer& Offer : Offers)
                {
                    if (Offer.Tense == CardTense)
                    {
                        Offer.WaitingCardIds.push_back(Card->Id);
                    }
                }

                continue;
            }

            if (!bDictionaryReady)
            {
                continue;
            }

            const FTDictionaryEntry* Verb = nullptr;

            for (FTenseOffer& Offer : Offers)
            {
                if (GetFormsTenseRequirement(TargetLanguage, Offer.Tense) != CardTense)
                {
                    continue;
                }

                if (Verb == nullptr)
                {
                    const std::optional<FTDictionaryEntry>& Found = FindFormsEntry(Card->TargetWord, FormsKindVerb);

                    Verb = Found.has_value() ? &*Found : nullptr;
                }

                if (Verb == nullptr)
                {
                    break;
                }

                std::optional<FTCard> Next =
                    MakeFormsCard(FormsKindVerb, *Verb, TargetLanguage, Card->Translation, Offer.Tense);

                if (Next.has_value() && Stored.find(Next->Id) == Stored.end())
                {
                    Offer.ReadyCards.push_back(std::move(*Next));
                }
            }
        }

        return Offers;
    }

    std::vector<FTCard> ListFormsCandidates(const std::string& Training)
    {
        std::vector<FTCard> Candidates;

        if (!AppDatabase::Get().IsReady() || !AppDictionary::Get().IsReady())
        {
            return Candidates;
        }

        const std::string TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;
        const std::vector<std::string> Kinds = ListFormsTrainingKinds(Training);

        FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

        for (const FTStudiedHeadword& Studied : CardRepository.ListStudiedHeadwords())
        {
            if (std::find(Kinds.begin(), Kinds.end(), Studied.PartOfSpeech) == Kinds.end())
            {
                continue;
            }

            const std::optional<FTCard>& Prototype =
                FindFormsCardPrototype(Studied.Headword, Studied.PartOfSpeech, TargetLanguage);

            if (!Prototype.has_value() || CardRepository.LoadFormsCard(Prototype->Id).has_value())
            {
                continue;
            }

            FTCard Card = *Prototype;
            Card.Translation = Studied.Translation;

            Candidates.push_back(std::move(Card));
        }

        return Candidates;
    }

    QString GetVerbGroup(const std::string& Headword, const std::string& PartOfSpeech)
    {
        if (PartOfSpeech != "verb" || !AppDictionary::Get().IsReady())
        {
            return {};
        }
        const auto& Entry = FindFormsEntry(Headword, "verb");
        return Entry.has_value() ? QString::fromStdString(::GetVerbGroup(
            FTSettings::Get().GetLanguageRules().TargetLanguage, *Entry)) : QString{};
    }

    QString GetVerbGroup(const FTCard& Card)
    {
        return GetVerbGroup(Card.TargetWord, Card.PartOfSpeech);
    }

    void PrioritizeIrregularVerbs(std::vector<FTCard>& Cards)
    {
        if (!AppDatabase::Get().IsReady() || !AppDictionary::Get().IsReady())
        {
            return;
        }

        constexpr int IrregularGroup = 0;
        constexpr int DerivedGroup = 1;
        constexpr int OtherGroup = 2;

        struct FRankedVerb
        {
            FTCard Card;
            int Group = OtherGroup;
            int Frequency = std::numeric_limits<int>::max();
        };

        const std::string TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;

        std::vector<std::pair<std::string, std::string>> Bases;

        for (const FTCardHandle& Stored : AppDatabase::Get().GetCardRepository().LoadFormsCards())
        {
            if (GetFormsKind(Stored->Id) == FormsKindVerb)
            {
                Bases.emplace_back(Stored->TargetWord, GetFormsTense(Stored->Id));
            }
        }

        for (const FTCard& Card : Cards)
        {
            if (GetFormsKind(Card.Id) == FormsKindVerb)
            {
                Bases.emplace_back(Card.TargetWord, GetFormsTense(Card.Id));
            }
        }

        const auto HasKnownBase = [&TargetLanguage, &Bases](const FTCard& Card, const FTDictionaryEntry& Entry)
        {
            const std::string Tense = GetFormsTense(Card.Id);

            return std::any_of(Bases.begin(), Bases.end(),
                [&TargetLanguage, &Card, &Entry, &Tense](const std::pair<std::string, std::string>& Base)
            {
                if (Base.second != Tense || Base.first.size() >= Card.TargetWord.size()
                    || !Card.TargetWord.ends_with(Base.first))
                {
                    return false;
                }

                const std::optional<FTDictionaryEntry>& BaseEntry = FindFormsEntry(Base.first, FormsKindVerb);

                return BaseEntry.has_value() && IsDerivedVerb(TargetLanguage, Tense, Entry, *BaseEntry);
            });
        };

        std::vector<FRankedVerb> Ranked;
        Ranked.reserve(Cards.size());

        for (FTCard& Card : Cards)
        {
            FRankedVerb Verb{std::move(Card)};

            if (GetFormsKind(Verb.Card.Id) == FormsKindVerb)
            {
                const std::optional<FTDictionaryEntry>& Entry = FindFormsEntry(Verb.Card.TargetWord, FormsKindVerb);

                if (Entry.has_value() && IsIrregularVerb(TargetLanguage, *Entry))
                {
                    Verb.Group = HasKnownBase(Verb.Card, *Entry) ? DerivedGroup : IrregularGroup;
                    Verb.Frequency = Entry->FrequencyRank.value_or(std::numeric_limits<int>::max());
                }
            }

            Ranked.push_back(std::move(Verb));
        }

        std::stable_sort(Ranked.begin(), Ranked.end(), [](const FRankedVerb& Left, const FRankedVerb& Right)
        {
            return std::tie(Left.Group, Left.Frequency) < std::tie(Right.Group, Right.Frequency);
        });

        for (size_t Index = 0; Index < Cards.size(); ++Index)
        {
            Cards[Index] = std::move(Ranked[Index].Card);
        }
    }

    int GetFormsLookupCount()
    {
        return FormsLookupCount;
    }

    std::optional<FTCardHandle> LoadSessionCard(FTCardRepository& CardRepository, const std::string& CardId)
    {
        if (GetFormsKind(CardId).empty())
        {
            return LoadCard(CardRepository, CardId);
        }

        const std::optional<FTCardHandle> Stored = CardRepository.LoadFormsCard(GetFormsCardId(CardId));

        if (!Stored.has_value() || !IsFormsItemId(CardId))
        {
            return Stored;
        }

        for (const FTCardHandle& Form : ExpandForms({*Stored}))
        {
            if (Form->Id == CardId)
            {
                return Form;
            }

            if (Form->Id == GetFormsItemId(CardId))
            {
                FTCard Round = *Form;
                Round.Id = CardId;

                return MakeCardHandle(std::move(Round));
            }
        }

        return std::nullopt;
    }

    std::vector<FTCardHandle> LoadCards()
    {
        if (!AppDatabase::Get().IsReady())
        {
            return {};
        }

        return AppDatabase::Get().GetCardRepository().LoadCards();
    }

    void ForgetAnnotations()
    {
        VerbSpellings.clear();
        StressedSpellings.clear();
        ElidingSpellings.clear();
    }

    bool UpdateCardContent(const QString& CardId, const QString& Word, const QString& Translation,
                           int Gender, const QString& PartOfSpeech, const QString& MeaningGist, const QString& Hint)
    {
        if (CardId.isEmpty() || !AppDatabase::Get().IsReady())
        {
            return false;
        }

        const QString TrimmedWord = Word.trimmed();
        const QString TrimmedTranslation = Translation.trimmed();

        if (TrimmedWord.isEmpty() || TrimmedTranslation.isEmpty())
        {
            return false;
        }

        if (!AppDatabase::Get().GetCardRepository().UpdateCardContent(CardId.toStdString(),
            TrimmedWord.toStdString(), TrimmedTranslation.toStdString(), ToOptionalGender(Gender),
            PartOfSpeech.toStdString(), MeaningGist.toStdString(), Hint.toStdString()))
        {
            qCWarning(LogApp) << "Word update failed:" << CardId;

            return false;
        }

        qCInfo(LogApp) << "Word updated:" << CardId << "meaning" << MeaningGist;

        return true;
    }

    int FindWordTranslationStage(const QString& Word, const QString& Translation)
    {
        return AppDatabase::Get().GetCardRepository().FindWordTranslationStageAnyReading(
            Word.toStdString(), Translation.toStdString()).value_or(-1);
    }

    QVariantMap DescribeCard(const FTCard& Card)
    {
        QVariantMap Entry;
        Entry[QStringLiteral("id")] = QString::fromStdString(Card.Id);
        Entry[QStringLiteral("targetWord")] = QString::fromStdString(Card.TargetWord);
        Entry[QStringLiteral("translation")] = QString::fromStdString(Card.Translation);
        Entry[QStringLiteral("partOfSpeech")] = QString::fromStdString(Card.PartOfSpeech);
        Entry[QStringLiteral("gender")] = Card.Gender.value_or(-1);
        Entry[QStringLiteral("box")] = Card.LeitnerStage;

        return Entry;
    }
}
