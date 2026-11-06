#include "WordListController.h"

#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppCards.h"
#include "AppSpeech.h"
#include "CefrFormatting.h"
#include "WordRowGrouping.h"

#include "core/FTCard.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"

#include <QVariantMap>

namespace
{
    std::optional<FTDictionarySearchMatch> FindExactMatch(const std::string& Headword,
                                                           const std::string& PartOfSpeech)
    {
        if (!AppDictionary::Get().IsReady())
        {
            return std::nullopt;
        }

        return AppDictionary::Get().GetRepository().LookUpExact(Headword, PartOfSpeech);
    }

    QVariantMap BuildCardEntry(const FTCard& Card)
    {
        const std::optional<FTDictionarySearchMatch> Match =
            FindExactMatch(Card.TargetWord, Card.PartOfSpeech);

        QVariantMap Entry;
        Entry[QStringLiteral("word")] = QString::fromStdString(
            GetDisplayWord(Card, FTSettings::Get().GetLanguageRules().TargetLanguage));
        Entry[QStringLiteral("translation")] = QString::fromStdString(Card.Translation);
        Entry[QStringLiteral("gender")] = Card.Gender.value_or(-1);
        Entry[QStringLiteral("verbGroup")] = AppCards::GetVerbGroup(Card);
        Entry[QStringLiteral("cefrLevel")] = Match ? CefrLetter(Match->CefrLevel) : QString();
        Entry[QStringLiteral("stage")] = Card.LeitnerStage;
        Entry[QStringLiteral("completed")] = Card.CompletedDate.has_value();
        Entry[QStringLiteral("targetWord")] = QString::fromStdString(Card.TargetWord);
        Entry[QStringLiteral("partOfSpeech")] = QString::fromStdString(Card.PartOfSpeech);

        return Entry;
    }
}

WordListController::WordListController(QObject* Parent)
    : QObject(Parent)
{
}

void WordListController::load(const QString& Source)
{
    Entries.clear();

    if (!AppDatabase::Get().IsReady())
    {
        emit entriesChanged();

        return;
    }

    if (Source == QStringLiteral("studied"))
    {
        for (const FTCard& Card : AppDatabase::Get().GetCardRepository().LoadRecentlyStudiedCards(RecentLimit))
        {
            Entries.append(BuildCardEntry(Card));
        }
    }
    else if (Source.startsWith(QStringLiteral("due:")))
    {
        const int DaysFromToday = Source.mid(4).toInt();
        const FTDayNumber Today = GetTodayDayNumber();
        FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

        const std::vector<FTCardHandle> Cards = DaysFromToday <= 0
            ? CardRepository.LoadDueCards(Today)
            : CardRepository.LoadCardsDueOnDay(Today + DaysFromToday);

        for (const FTCardHandle& Card : Cards)
        {
            if (Card)
            {
                Entries.append(BuildCardEntry(*Card));
            }
        }
    }
    else
    {
        const bool bDictionaryReady = AppDictionary::Get().IsReady();

        for (const FTSearchHistoryEntry& Row : AppDatabase::Get().GetCardRepository().LoadRecentSearches(RecentLimit))
        {
            std::string Headword = Row.Term;
            std::string PartOfSpeech = Row.PartOfSpeech;

            if (Row.WordId != 0 && bDictionaryReady)
            {
                const std::optional<FTDictionaryEntry> Resolved =
                    AppDictionary::Get().GetRepository().LookUpById(Row.WordId);

                if (Resolved.has_value())
                {
                    Headword = Resolved->Headword;
                    PartOfSpeech = Resolved->PartOfSpeech;
                }
            }

            const std::optional<FTDictionarySearchMatch> Match = FindExactMatch(Headword, PartOfSpeech);

            QVariantMap Entry;
            Entry[QStringLiteral("word")] = QString::fromStdString(GetDisplayHeadword(
                Headword, PartOfSpeech, FTSettings::Get().GetLanguageRules().TargetLanguage));
            Entry[QStringLiteral("translation")] =
                Match ? QString::fromStdString(Match->Translation) : QString();
            Entry[QStringLiteral("cefrLevel")] = Match ? CefrLetter(Match->CefrLevel) : QString();
            Entry[QStringLiteral("stage")] = AppDatabase::Get().GetCardRepository()
                .FindWordStage(Headword, PartOfSpeech).value_or(-1);
            Entry[QStringLiteral("completed")] =
                AppDatabase::Get().GetCardRepository().IsWordCompleted(Headword, PartOfSpeech);
            Entry[QStringLiteral("verbGroup")] = AppCards::GetVerbGroup(Headword, PartOfSpeech);
            Entry[QStringLiteral("targetWord")] = QString::fromStdString(Headword);
            Entry[QStringLiteral("partOfSpeech")] = QString::fromStdString(PartOfSpeech);

            Entries.append(Entry);
        }
    }

    Entries = GroupRowsByHeadword(Entries);

    emit entriesChanged();
}

int WordListController::count(const QString& Source) const
{
    if (!AppDatabase::Get().IsReady())
    {
        return 0;
    }

    if (Source == QStringLiteral("studied"))
    {
        return static_cast<int>(AppDatabase::Get().GetCardRepository().LoadRecentlyStudiedCards(RecentLimit).size());
    }

    if (Source == QStringLiteral("history"))
    {
        return static_cast<int>(AppDatabase::Get().GetCardRepository().LoadRecentSearches(RecentLimit).size());
    }

    qCWarning(LogApp) << "WordListController cannot count" << Source;

    return 0;
}

void WordListController::speakWord(const QString& Word) const
{
    qCInfo(LogSpeech) << "WordListController speaks:" << Word;

    AppSpeech::Get().Speak(Word,
        QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage));
}

QVariantList WordListController::GetEntries() const
{
    return Entries;
}
