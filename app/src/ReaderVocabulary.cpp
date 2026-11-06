#include "ReaderVocabulary.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppReaderSettings.h"

#include "core/FTCard.h"
#include "core/FTLog.h"
#include "core/FTWordForms.h"

#include <QElapsedTimer>

#include <optional>
#include <unordered_map>
#include <unordered_set>

static std::string MakeHeadwordKey(const std::string& Headword)
{
    return QString::fromStdString(Headword).toCaseFolded().toStdString();
}

static std::string MakeReadingKey(const std::string& Headword, const std::string& PartOfSpeech)
{
    return MakeHeadwordKey(Headword) + '\x1F' + PartOfSpeech;
}

static void MergeKnown(std::unordered_map<std::string, bool>& Known, const std::string& Key, bool bKnown)
{
    const auto [Entry, bInserted] = Known.try_emplace(Key, bKnown);

    if (!bInserted)
    {
        Entry->second = Entry->second && bKnown;
    }
}

static std::optional<bool> FindKnown(const std::unordered_map<std::string, bool>& Known, const std::string& Key)
{
    const auto Entry = Known.find(Key);

    return Entry == Known.end() ? std::nullopt : std::optional<bool>{Entry->second};
}

ReaderVocabulary::ReaderVocabulary(QObject* Parent)
    : QObject(Parent)
    , bEnabled{AppReaderSettings::Get().IsHighlightingVocabulary()}
{
    connect(&AppReaderSettings::Get(), &AppReaderSettings::changed, this, [this]()
    {
        const bool bNowEnabled{AppReaderSettings::Get().IsHighlightingVocabulary()};

        if (bNowEnabled == bEnabled)
        {
            return;
        }

        bEnabled = bNowEnabled;

        if (!bEnabled)
        {
            Marks.Clear();
            BuiltRevision.clear();

            qCInfo(LogApp) << "Reader: vocabulary highlighting turned off";

            emit changed();

            return;
        }

        qCInfo(LogApp) << "Reader: vocabulary highlighting turned on";

        refresh();
    });

    connect(&AppDictionary::Notifier(), &AppDictionaryNotifier::languagesChanged, this, [this]()
    {
        BuiltRevision.clear();

        refresh();
    });
}

void ReaderVocabulary::refresh()
{
    if (!bEnabled)
    {
        return;
    }

    const std::string Revision{MakeRevision()};

    if (Revision.empty() || Revision == BuiltRevision)
    {
        return;
    }

    Rebuild();

    BuiltRevision = Revision;

    emit changed();
}

bool ReaderVocabulary::IsEnabled() const
{
    return bEnabled;
}

std::vector<FTVocabularyMark> ReaderVocabulary::Find(const QString& Text) const
{
    return bEnabled ? Marks.Find(Text) : std::vector<FTVocabularyMark>{};
}

std::string ReaderVocabulary::MakeRevision() const
{
    const std::string Data{AppDatabase::Get().GetDataRevision()};

    if (Data.empty())
    {
        return {};
    }

    return Data + "|" + std::to_string(AppDictionary::Get().GetGeneration());
}

void ReaderVocabulary::Rebuild()
{
    QElapsedTimer Timer;
    Timer.start();

    Marks.Clear();

    std::unordered_map<std::string, bool> Readings;
    std::unordered_map<std::string, bool> AnyReadings;
    std::unordered_set<std::string> Seen;
    std::vector<std::string> Headwords;
    int CardCount{0};

    for (const FTCardHandle& Card : AppCards::LoadCards())
    {
        if (Card == nullptr || IsFormsCard(*Card) || IsInPool(*Card))
        {
            continue;
        }

        const bool bKnown{IsCompleted(*Card)};

        ++CardCount;

        Marks.AddSpelling(QString::fromStdString(Card->TargetWord), bKnown);

        for (const std::string& Spelling : Card->AlternativeSpellings)
        {
            Marks.AddSpelling(QString::fromStdString(Spelling), bKnown);
        }

        if (Card->PartOfSpeech.empty())
        {
            MergeKnown(AnyReadings, MakeHeadwordKey(Card->TargetWord), bKnown);
        }
        else
        {
            MergeKnown(Readings, MakeReadingKey(Card->TargetWord, Card->PartOfSpeech), bKnown);
        }

        if (Seen.insert(Card->TargetWord).second)
        {
            Headwords.push_back(Card->TargetWord);
        }
    }

    size_t FormCount{0};

    if (!Headwords.empty() && AppDictionary::Get().IsReady())
    {
        for (const FTHeadwordForm& Row : AppDictionary::Get().GetRepository().ListHeadwordForms(Headwords))
        {
            const std::optional<bool> ByReading{FindKnown(Readings, MakeReadingKey(Row.Headword, Row.PartOfSpeech))};
            const std::optional<bool> ByHeadword{FindKnown(AnyReadings, MakeHeadwordKey(Row.Headword))};

            if (!ByReading.has_value() && !ByHeadword.has_value())
            {
                continue;
            }

            Marks.AddSpelling(QString::fromStdString(Row.Form), ByReading.value_or(true) && ByHeadword.value_or(true));

            ++FormCount;
        }
    }

    qCInfo(LogApp) << "Reader: vocabulary highlighting built from" << CardCount << "cards and" << FormCount
                   << "forms in" << Timer.elapsed() << "ms";
}
