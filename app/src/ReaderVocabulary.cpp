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

static void MergeProgress(std::unordered_map<std::string, FTVocabularyProgress>& Progress, const std::string& Key,
                          const FTVocabularyProgress& Added)
{
    const auto [Entry, bInserted] = Progress.try_emplace(Key, Added);

    if (!bInserted)
    {
        Entry->second = MergeVocabularyProgress(Entry->second, Added);
    }
}

static std::optional<FTVocabularyProgress> FindProgress(const std::unordered_map<std::string, FTVocabularyProgress>& Progress,
                                                        const std::string& Key)
{
    const auto Entry = Progress.find(Key);

    return Entry == Progress.end() ? std::nullopt : std::optional<FTVocabularyProgress>{Entry->second};
}

ReaderVocabulary::ReaderVocabulary(QObject* Parent)
    : QObject(Parent)
    , bEnabled{AppReaderSettings::Get().IsHighlightingVocabulary()}
    , bHighlightUnknown{AppReaderSettings::Get().IsHighlightingUnknownWords()}
{
    connect(&AppReaderSettings::Get(), &AppReaderSettings::changed, this, [this]()
    {
        const bool bNowEnabled{AppReaderSettings::Get().IsHighlightingVocabulary()};

        const bool bNowHighlightUnknown{AppReaderSettings::Get().IsHighlightingUnknownWords()};

        if (bNowEnabled == bEnabled && bNowHighlightUnknown == bHighlightUnknown)
        {
            return;
        }

        bEnabled = bNowEnabled;
        bHighlightUnknown = bNowHighlightUnknown;
        BuiltRevision.clear();

        if (!bEnabled)
        {
            Marks.Clear();

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
    return bEnabled ? Marks.Find(Text, bHighlightUnknown) : std::vector<FTVocabularyMark>{};
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

    std::unordered_map<std::string, FTVocabularyProgress> Readings;
    std::unordered_map<std::string, FTVocabularyProgress> AnyReadings;
    std::unordered_set<std::string> Seen;
    std::vector<std::string> Headwords;
    int CardCount{0};

    for (const FTCardHandle& Card : AppCards::LoadCards())
    {
        if (Card == nullptr || IsFormsCard(*Card))
        {
            continue;
        }

        const FTVocabularyProgress Progress{IsCompleted(*Card), std::max(Card->LeitnerStage, PoolStage)};

        ++CardCount;

        Marks.AddSpelling(QString::fromStdString(Card->TargetWord), Progress);

        for (const std::string& Spelling : Card->AlternativeSpellings)
        {
            Marks.AddSpelling(QString::fromStdString(Spelling), Progress);
        }

        if (Card->PartOfSpeech.empty())
        {
            MergeProgress(AnyReadings, MakeHeadwordKey(Card->TargetWord), Progress);
        }
        else
        {
            MergeProgress(Readings, MakeReadingKey(Card->TargetWord, Card->PartOfSpeech), Progress);
        }

        if (Seen.insert(Card->TargetWord).second)
        {
            Headwords.push_back(Card->TargetWord);
        }
    }

    if (bHighlightUnknown && AppDictionary::Get().IsReady())
    {
        const int Cutoff{AppDatabase::Get().GetCardRepository().GetPlacementCutoff()};
        const FTVocabularyProgress Known{true};

        for (const FTRankedWord& Word : AppDictionary::Get().GetRepository().ListWordsUpToTeachingRank(Cutoff))
        {
            Marks.AddSpelling(QString::fromStdString(Word.Headword), Known);
            MergeProgress(Readings, MakeReadingKey(Word.Headword, Word.PartOfSpeech), Known);

            if (Seen.insert(Word.Headword).second)
            {
                Headwords.push_back(Word.Headword);
            }
        }
    }

    size_t FormCount{0};

    if (!Headwords.empty() && AppDictionary::Get().IsReady())
    {
        for (const FTHeadwordForm& Row : AppDictionary::Get().GetRepository().ListHeadwordForms(Headwords))
        {
            const std::optional<FTVocabularyProgress> ByReading{
                FindProgress(Readings, MakeReadingKey(Row.Headword, Row.PartOfSpeech))};
            const std::optional<FTVocabularyProgress> ByHeadword{FindProgress(AnyReadings, MakeHeadwordKey(Row.Headword))};

            if (!ByReading.has_value() && !ByHeadword.has_value())
            {
                continue;
            }

            const FTVocabularyProgress Progress{ByReading.has_value() && ByHeadword.has_value()
                ? MergeVocabularyProgress(*ByReading, *ByHeadword)
                : ByReading.value_or(ByHeadword.value_or(FTVocabularyProgress{}))};

            Marks.AddSpelling(QString::fromStdString(Row.Form), Progress);

            ++FormCount;
        }
    }

    qCInfo(LogApp) << "Reader: vocabulary highlighting built from" << CardCount << "cards and" << FormCount
                   << "forms in" << Timer.elapsed() << "ms";
}
