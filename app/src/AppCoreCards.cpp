#include "AppCoreCards.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"

#include "core/FTCard.h"
#include "core/FTCardRepository.h"
#include "core/FTCoreCards.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QFile>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    constexpr int ExistingCardLookupLimit = 20;

    QString MakeCoreListPath(const QString& TargetLanguage, const QString& NativeLanguage)
    {
        return QStringLiteral(":/corecards/%1-%2.tsv").arg(TargetLanguage, NativeLanguage);
    }

    bool HasCard(const FTCardRepository& Cards, const FTCoreCard& Core)
    {
        const std::vector<FTCard> Found = Cards.FindCardsByHeadword(Core.Word, ExistingCardLookupLimit);

        return std::any_of(Found.begin(), Found.end(), [&Core](const FTCard& Card)
        {
            return Card.TargetWord == Core.Word && Card.PartOfSpeech == Core.PartOfSpeech;
        });
    }

    bool HasMeaning(const FTDictionaryRepository& Dictionary, const FTCoreCard& Core)
    {
        const std::vector<FTDictionarySense> Meanings = Dictionary.LoadMeanings(Core.Word, Core.PartOfSpeech);

        return std::any_of(Meanings.begin(), Meanings.end(), [&Core](const FTDictionarySense& Meaning)
        {
            return Meaning.Gist == Core.MeaningGist;
        });
    }
}

bool AppCoreCards::Seed(const QString& TargetLanguage)
{
    const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

    if (Rules.TargetLanguage != TargetLanguage.toStdString() || !AppDictionary::Get().IsReady()
        || !AppDatabase::Get().IsReady())
    {
        qCWarning(LogApp) << "Core cards: the dictionary of" << TargetLanguage << "is not the active one, nothing added";

        return false;
    }

    const QString NativeLanguage = QString::fromStdString(Rules.NativeLanguage);
    QFile List(MakeCoreListPath(TargetLanguage, NativeLanguage));

    if (!List.open(QIODevice::ReadOnly))
    {
        qCInfo(LogApp) << "Core cards:" << TargetLanguage << "->" << NativeLanguage << "has no core list";

        return true;
    }

    const FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();
    FTCardRepository& Cards = AppDatabase::Get().GetCardRepository();

    int Added = 0;
    int Skipped = 0;

    for (const FTCoreCard& Core : ParseCoreCards(List.readAll().toStdString()))
    {
        if (HasCard(Cards, Core))
        {
            continue;
        }

        std::string Gist = Core.MeaningGist;

        if (!Gist.empty() && !HasMeaning(Dictionary, Core))
        {
            qCWarning(LogApp) << "Core cards: the dictionary has no meaning" << QString::fromStdString(Gist) << "for"
                              << QString::fromStdString(Core.Word);

            Gist.clear();
        }

        const FTCard Card = AppCards::MakeAddedWord(Core.Word, Core.Translation, Core.PartOfSpeech, -1, false, Gist);

        if (!Cards.AddCard(Card).has_value())
        {
            qCWarning(LogApp) << "Core cards: could not add" << QString::fromStdString(Core.Word) << "-"
                              << QString::fromStdString(Cards.GetLastError());

            ++Skipped;

            continue;
        }

        ++Added;
    }

    qCInfo(LogApp) << "Core cards:" << Added << "added to the pool for" << TargetLanguage << "," << Skipped << "skipped";

    return true;
}
