#include "YoutubeController.h"

#include "AppDatabase.h"
#include "AppYoutubeCaptions.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"
#include "core/FTYoutubeVideo.h"

#include <QVariantMap>

#include <algorithm>

namespace
{
    QVariantMap DocumentEntry(const FTYoutubeVideoRow& Row)
    {
        QVariantMap Entry;
        Entry[QStringLiteral("id")] = QString::fromStdString(Row.Id);
        Entry[QStringLiteral("videoId")] = QString::fromStdString(Row.VideoId);
        Entry[QStringLiteral("title")] = Row.Title.empty()
            ? QString::fromStdString(Row.VideoId)
            : QString::fromStdString(Row.Title);

        return Entry;
    }

    QVariantMap CueEntry(const FTCaptionCue& Cue)
    {
        QVariantMap Entry;
        Entry[QStringLiteral("startMs")] = static_cast<qlonglong>(Cue.StartMs);
        Entry[QStringLiteral("durationMs")] = static_cast<qlonglong>(Cue.DurationMs);
        Entry[QStringLiteral("text")] = QString::fromStdString(Cue.Text);

        return Entry;
    }
}

YoutubeController::YoutubeController(QObject* Parent)
    : QObject(Parent)
    , Fetcher(std::make_unique<AppYoutubeCaptions>())
{
    connect(Fetcher.get(), &AppYoutubeCaptions::titleFetched,
            this, &YoutubeController::OnTitleFetched);

    connect(Fetcher.get(), &AppYoutubeCaptions::captionVttFetched,
            this, &YoutubeController::OnCaptionTextFetched);

    connect(Fetcher.get(), &AppYoutubeCaptions::captionsUnavailable,
            this, &YoutubeController::OnCaptionsUnavailable);
}

YoutubeController::~YoutubeController() = default;

void YoutubeController::refresh()
{
    if (bWatching || !AppDatabase::Get().IsReady())
    {
        return;
    }

    Documents.clear();

    for (const FTYoutubeVideoRow& Row : AppDatabase::Get().GetYoutubeLibrary().ListVideos())
    {
        Documents.append(DocumentEntry(Row));
    }

    emit documentsChanged();
}

bool YoutubeController::addAndOpen(const QString& UrlOrId)
{
    if (!AppDatabase::Get().IsReady())
    {
        return false;
    }

    const std::optional<std::string> VideoId = ExtractYoutubeVideoId(UrlOrId.toStdString());

    if (!VideoId.has_value())
    {
        return false;
    }

    const std::optional<std::string> RowId =
        AppDatabase::Get().GetYoutubeLibrary().AddVideo(*VideoId, *VideoId);

    if (!RowId.has_value())
    {
        return false;
    }

    return openVideo(QString::fromStdString(*RowId));
}

bool YoutubeController::openVideo(const QString& Id)
{
    if (!AppDatabase::Get().IsReady())
    {
        return false;
    }

    FTYoutubeRepository& Library = AppDatabase::Get().GetYoutubeLibrary();

    const std::vector<FTYoutubeVideoRow> Videos = Library.ListVideos();
    const auto Found = std::find_if(Videos.begin(), Videos.end(), [&Id](const FTYoutubeVideoRow& Row)
    {
        return QString::fromStdString(Row.Id) == Id;
    });

    if (Found == Videos.end())
    {
        return false;
    }

    Library.TouchVideo(Id.toStdString());

    CurrentRowId = Id;
    CurrentVideoId = QString::fromStdString(Found->VideoId);
    CurrentTitle = QString::fromStdString(Found->Title);
    bWatching = true;
    bCaptionsLoading = true;
    bCaptionsUnavailable = false;
    Cues.clear();
    CuesVariant.clear();
    CurrentCueIndex = -1;

    qCInfo(LogYoutube) << "opened" << CurrentVideoId;

    emit currentVideoChanged();
    emit currentCueIndexChanged();

    Fetcher->FetchTitle(CurrentVideoId);
    StartCaptionFetch();

    return true;
}

void YoutubeController::closeVideo()
{
    if (!bWatching)
    {
        return;
    }

    CurrentRowId.clear();
    CurrentVideoId.clear();
    CurrentTitle.clear();
    bWatching = false;
    bCaptionsLoading = false;
    bCaptionsUnavailable = false;
    Cues.clear();
    CuesVariant.clear();
    CurrentCueIndex = -1;

    emit currentVideoChanged();
    emit currentCueIndexChanged();
}

void YoutubeController::removeVideo(const QString& Id)
{
    if (!AppDatabase::Get().IsReady())
    {
        return;
    }

    AppDatabase::Get().GetYoutubeLibrary().RemoveVideo(Id.toStdString());

    if (CurrentRowId == Id)
    {
        closeVideo();
    }

    refresh();
}

void YoutubeController::reportTime(double Seconds)
{
    if (Cues.empty())
    {
        return;
    }

    const int64_t Ms = static_cast<int64_t>(Seconds * 1000.0);

    int Found = -1;

    for (size_t Index = 0; Index < Cues.size(); ++Index)
    {
        const FTCaptionCue& Cue = Cues[Index];

        if (Ms >= Cue.StartMs && Ms < Cue.StartMs + Cue.DurationMs)
        {
            Found = static_cast<int>(Index);

            break;
        }
    }

    if (Found != CurrentCueIndex)
    {
        CurrentCueIndex = Found;

        emit currentCueIndexChanged();
    }
}

void YoutubeController::StartCaptionFetch()
{
    if (!bWatching || CurrentVideoId.isEmpty())
    {
        return;
    }

    const std::string Preferred = FTSettings::Get().GetLanguageRules().TargetLanguage;
    Fetcher->FetchCaptionTrack(CurrentVideoId, QString::fromStdString(Preferred));
}

void YoutubeController::OnCaptionTextFetched(const QString& VideoId, const QString& RawText)
{
    receiveCaptionText(VideoId, RawText);
}

void YoutubeController::OnCaptionsUnavailable(const QString& VideoId)
{
    reportCaptionsUnavailable(VideoId);
}

void YoutubeController::receiveCaptionText(const QString& VideoId, const QString& RawText)
{
    if (!bWatching || VideoId != CurrentVideoId)
    {
        return;
    }

    Cues = ParseWebVtt(RawText.toStdString());

    if (Cues.empty())
    {
        qCWarning(LogYoutube) << "caption track for" << VideoId
                              << "fetched but produced no cues";

        bCaptionsLoading = false;
        bCaptionsUnavailable = true;

        emit currentVideoChanged();

        return;
    }

    CuesVariant.clear();

    for (const FTCaptionCue& Cue : Cues)
    {
        CuesVariant.append(CueEntry(Cue));
    }

    bCaptionsLoading = false;
    bCaptionsUnavailable = false;

    qCInfo(LogYoutube) << "captions loaded for" << VideoId << "-" << Cues.size() << "lines";

    emit currentVideoChanged();
}

void YoutubeController::reportCaptionsUnavailable(const QString& VideoId)
{
    if (!bWatching || VideoId != CurrentVideoId)
    {
        return;
    }

    bCaptionsLoading = false;
    bCaptionsUnavailable = true;

    emit currentVideoChanged();
}

QVariantList YoutubeController::GetDocuments() const
{
    return Documents;
}

QString YoutubeController::GetCurrentVideoId() const
{
    return CurrentVideoId;
}

QString YoutubeController::GetCurrentTitle() const
{
    return CurrentTitle;
}

bool YoutubeController::IsWatching() const
{
    return bWatching;
}

bool YoutubeController::AreCaptionsLoading() const
{
    return bCaptionsLoading;
}

bool YoutubeController::AreCaptionsUnavailable() const
{
    return bCaptionsUnavailable;
}

QVariantList YoutubeController::GetCues() const
{
    return CuesVariant;
}

int YoutubeController::GetCurrentCueIndex() const
{
    return CurrentCueIndex;
}

void YoutubeController::OnTitleFetched(const QString& VideoId, const QString& Title)
{
    if (!bWatching || VideoId != CurrentVideoId || Title.isEmpty())
    {
        return;
    }

    CurrentTitle = Title;

    if (!CurrentRowId.isEmpty() && AppDatabase::Get().IsReady())
    {
        AppDatabase::Get().GetYoutubeLibrary().AddVideo(VideoId.toStdString(),
                                                        Title.toStdString());
    }

    emit currentVideoChanged();
}
