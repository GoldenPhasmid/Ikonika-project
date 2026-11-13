#pragma once

#include "core/FTCardStore.h"
#include "core/FTWordForms.h"
#include "core/FTDictionaryRepository.h"

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <optional>
#include <string>
#include <vector>

class FTCardRepository;

struct FTenseOffer
{
    std::string Tense;
    std::string Label;
    std::vector<std::string> WaitingCardIds;
    std::vector<FTCard> ReadyCards;
};

/**
 * The one way cards reach the rest of the app.
 *
 * Everything here is FTCardRepository's own work with one step added: filling in the runtime-only
 * facts a card cannot answer about itself. Today that is whether its spelling is also a verb, which
 * decides between "the need" and plain "need" and is a question for the dictionary rather than the
 * trainer database.
 *
 * A single funnel rather than the same annotation repeated at each of the dozen places that used to
 * load cards directly. The alternative was a card that displays correctly in most exercises and
 * quietly wrong in whichever one was added last.
 */
namespace AppCards
{
    std::vector<FTCardHandle> LoadCards();

    std::optional<FTCardHandle> LoadCard(FTCardRepository& CardRepository, const std::string& CardId);

    std::vector<FTCardHandle> ExpandForms(const std::vector<FTCardHandle>& Cards);

    bool IsFormSkipped(const FTCard& Form);

    std::vector<FTCardHandle> ExpandStudiedForms(const std::vector<FTCardHandle>& Cards);

    std::optional<FTCardHandle> LoadSessionCard(FTCardRepository& CardRepository, const std::string& CardId);

    QString MarkFormEnding(const FTWordForm& Read);

    QString MarkFormWord(const FTCard& Form);

    QVariantMap MakeMeaningEntry(const FTDictionarySense& Meaning);

    QString FindCardMeaningGist(const std::string& CardId);

    QString LabelFormLead(const FTCard& Form, const FTWordForm& Read);

    QVariantList BuildFormsTable(const FTCard& Stored);

    std::vector<FTenseOffer> ListTenseOffers();

    std::vector<FTCard> ListFormsCandidates(const std::string& Training);

    struct FFormsCountSources
    {
        const FTDictionaryRepository* Dictionary = nullptr;
        const FTCardRepository* Cards = nullptr;
        std::string TargetLanguage;
    };

    std::vector<FTCard> ListFormsCandidates(const std::string& Training, const FFormsCountSources& Sources);

    std::vector<FTenseOffer> ListTenseOffers(const FFormsCountSources& Sources);

    std::vector<FTCardHandle> LoadWaitingFormsCards(const FTCardRepository& CardRepository, const std::string& Training);

    int CountFormsWords(const std::string& Training, const FFormsCountSources& Sources);

    void PrioritizeIrregularVerbs(std::vector<FTCard>& Cards);

    QString GetVerbGroup(const std::string& Headword, const std::string& PartOfSpeech);
    QString GetVerbGroup(const FTCard& Card);

    int GetFormsLookupCount();

    void Prepare(const std::vector<FTCardHandle>& Cards);

    void Annotate(const std::vector<FTCardHandle>& Cards);

    void Annotate(FTCard& Card);

    void Annotate(std::vector<FTCard>& Cards);

    void AnnotateLoadedCards(FTCardRepository& CardRepository);

    /**
     * Fills in the part of speech and gender a card is being made without, from the dictionary.
     *
     * Adding a word by hand or from the dictionary screen supplies a spelling and a translation and
     * nothing else, so every noun added that way was stored with no gender and shown with no
     * article - "soldado" rather than "el soldado" - while a word collected by triage, which has
     * the reading in hand, was shown correctly. The article is part of the word being learned, so a
     * card missing it is teaching less than the one beside it.
     *
     * Only ever fills what is empty. A gender the user chose is not overwritten, and neither is one
     * a caller already knew.
     */
    void FillReadingFromDictionary(FTCard& Card);

    void FillMeaningFromDictionary(FTCard& Card);

    FTCard MakeAddedWord(const std::string& Word, const std::string& Translation,
                         const std::string& PartOfSpeech, int Gender, bool bReadingChosen,
                         const std::string& MeaningGist);

    /** Forgets what the dictionary said. Called when the language pair changes, since the answers
        were about the dictionary that was open at the time. */
    void ForgetAnnotations();

    bool UpdateCardContent(const QString& CardId, const QString& Word, const QString& Translation,
                           int Gender, const QString& PartOfSpeech, const QString& MeaningGist, const QString& Hint = QString());

    int FindWordTranslationStage(const QString& Word, const QString& Translation);

    QVariantMap DescribeCard(const FTCard& Card);
}
