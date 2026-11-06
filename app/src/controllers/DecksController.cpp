#include "DecksController.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"

#include "core/FTCard.h"
#include "core/FTDeckProgress.h"
#include "core/FTLog.h"

#include <QElapsedTimer>
#include <QVariantMap>

namespace
{
    constexpr int64_t FirstExamDeckId{33};
}

DecksController::DecksController(QObject* Parent)
    : QObject(Parent)
{
    connect(&AppDictionary::Notifier(), &AppDictionaryNotifier::languagesChanged, this, &DecksController::refresh);

    refresh();
}

void DecksController::refresh()
{
    QElapsedTimer Timer;
    Timer.start();

    if (!AppDictionary::Get().IsReady() || !AppDatabase::Get().IsReady())
    {
        Decks.clear();
        LoadedGeneration.reset();

        emit decksChanged();

        return;
    }

    LoadDeckData();

    const FTDeckProgressMatcher Matcher(AppCards::LoadCards(), AppDatabase::Get().GetCardRepository().ListKnownWords());
    const std::vector<FTDeckWord> NoReadings;

    QVariantList Updated;

    for (const FTDeck& Deck : DeckList)
    {
        const auto Found = Readings.find(Deck.Id);

        int NewCount = 0;
        int LearningCount = 0;
        int LearnedCount = 0;
        int UntriagedCount = 0;

        for (const FTDeckWordProgress& Progress : Matcher.Match(Found != Readings.end() ? Found->second : NoReadings))
        {
            if (Progress.bKnown || (Progress.Card && IsCompleted(*Progress.Card)))
            {
                ++LearnedCount;
            }
            else if (Progress.Card && !IsInPool(*Progress.Card))
            {
                ++LearningCount;
            }
            else
            {
                ++NewCount;
            }

            if (!Progress.bKnown && !Progress.Card)
            {
                ++UntriagedCount;
            }
        }

        QVariantMap Entry;
        Entry[QStringLiteral("id")] = static_cast<qint64>(Deck.Id);
        Entry[QStringLiteral("name")] = QString::fromStdString(Deck.Name);
        Entry[QStringLiteral("category")] = QString::fromStdString(Deck.Category);
        Entry[QStringLiteral("icon")] = Deck.Id >= FirstExamDeckId ? QStringLiteral("deck_exam")
                                                                   : QStringLiteral("deck_%1").arg(Deck.Id);
        Entry[QStringLiteral("wordCount")] = Deck.WordCount;
        Entry[QStringLiteral("newCount")] = NewCount;
        Entry[QStringLiteral("learningCount")] = LearningCount;
        Entry[QStringLiteral("learnedCount")] = LearnedCount;
        Entry[QStringLiteral("untriagedCount")] = UntriagedCount;

        Updated.append(Entry);
    }

    qCInfo(LogApp) << "Decks: the dictionary has" << Updated.size() << "decks, progress took" << Timer.elapsed() << "ms";

    if (Updated == Decks)
    {
        return;
    }

    Decks = std::move(Updated);

    emit decksChanged();
}

void DecksController::LoadDeckData()
{
    const uint64_t Generation = AppDictionary::Get().GetGeneration();

    if (LoadedGeneration == Generation)
    {
        return;
    }

    FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();

    DeckList = Dictionary.ListDecks();
    Readings = Dictionary.LoadDeckReadings();
    LoadedGeneration = Generation;

    qCDebug(LogApp) << "Decks: read" << static_cast<int>(DeckList.size()) << "decks from dictionary generation" << Generation;
}

bool DecksController::IsAvailable() const
{
    return !Decks.isEmpty();
}

QVariantList DecksController::GetDecks() const
{
    return Decks;
}
