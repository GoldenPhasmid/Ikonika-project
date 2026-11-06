#include "AppYoutubeCaptions.h"

#include "core/FTLog.h"
#include "core/FTYoutubeCaptions.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include <utility>

namespace
{
    constexpr int TimeoutMilliseconds = 15000;

    constexpr const char* DesktopUserAgent =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

    constexpr const char* IosUserAgent =
        "com.google.ios.youtube/20.10.38 (iPhone16,2; U; CPU iOS 18_3_2 like Mac OS X)";

    // The public InnerTube API key used by YouTube's own clients. Caption tracks come through
    // this player endpoint because the browser-facing timedtext URLs the watch page provides are
    // signed against a playback token and return empty bodies without one.
    constexpr const char* InnerTubePlayerKey = "AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8";

    QByteArray InnerTubePlayerUrl()
    {
        return "https://www.youtube.com/youtubei/v1/player?key="
            + QByteArray(InnerTubePlayerKey) + "&prettyPrint=false";
    }

    QNetworkRequest MakeRequest(const QUrl& Url)
    {
        QNetworkRequest Request(Url);
        Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        Request.setTransferTimeout(TimeoutMilliseconds);
        Request.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(DesktopUserAgent));

        return Request;
    }

    bool IsVideoPlayable(const QByteArray& PlayerResponse)
    {
        const QJsonObject Object = QJsonDocument::fromJson(PlayerResponse).object();
        const QString Status =
            Object.value(QLatin1String("playabilityStatus")).toObject()
                  .value(QLatin1String("status")).toString();

        return Status == QLatin1String("OK");
    }

    QString VttUrlFor(const FTCaptionTrack& Track)
    {
        return QString::fromStdString(Track.BaseUrl)
            + ((Track.BaseUrl.find('?') != std::string::npos) ? "&fmt=vtt" : "?fmt=vtt");
    }
}

AppYoutubeCaptions::AppYoutubeCaptions(QObject* Parent)
    : QObject(Parent)
    , Network(std::make_unique<QNetworkAccessManager>())
{
}

AppYoutubeCaptions::~AppYoutubeCaptions() = default;

void AppYoutubeCaptions::FetchTitle(const QString& VideoId)
{
    QUrl Url("https://www.youtube.com/oembed");
    QUrlQuery Query;
    Query.addQueryItem("url", "https://www.youtube.com/watch?v=" + VideoId);
    Query.addQueryItem("format", "json");
    Url.setQuery(Query);

    QNetworkReply* Reply = Network->get(MakeRequest(Url));

    connect(Reply, &QNetworkReply::finished, this, [this, Reply, VideoId]()
    {
        const QByteArray Body = Reply->readAll();
        const bool bOk = Reply->error() == QNetworkReply::NoError;
        Reply->deleteLater();

        if (!bOk)
        {
            qCWarning(LogYoutube) << "title fetch failed for" << VideoId;

            emit titleFetched(VideoId, QString());

            return;
        }

        const QJsonObject Object = QJsonDocument::fromJson(Body).object();

        emit titleFetched(VideoId, Object.value(QLatin1String("title")).toString());
    });
}

void AppYoutubeCaptions::FetchCaptionTrack(const QString& VideoId,
                                           const QString& PreferredLanguage)
{
    QNetworkRequest Request(QUrl(QString::fromLatin1(InnerTubePlayerUrl())));
    Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    Request.setTransferTimeout(TimeoutMilliseconds);
    Request.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(IosUserAgent));
    Request.setRawHeader("Content-Type", "application/json");
    Request.setRawHeader("X-Youtube-Client-Name", "5");
    Request.setRawHeader("X-Youtube-Client-Version", "20.10.38");

    const QByteArray Body =
        "{\"context\":{\"client\":{\"clientName\":\"IOS\",\"clientVersion\":\"20.10.38\","
        "\"deviceMake\":\"Apple\",\"deviceModel\":\"iPhone16,2\",\"osName\":\"iPhone\","
        "\"osVersion\":\"18.3.2.22D82\",\"hl\":\"en\",\"gl\":\"US\",\"utcOffsetMinutes\":0}},"
        "\"videoId\":\"" + VideoId.toUtf8() + "\"}";

    QNetworkReply* Reply = Network->post(Request, Body);

    connect(Reply, &QNetworkReply::finished, this, [this, Reply, VideoId, PreferredLanguage]()
    {
        const QByteArray PlayerResponse = Reply->readAll();
        const bool bOk = Reply->error() == QNetworkReply::NoError;
        Reply->deleteLater();

        if (!bOk)
        {
            qCWarning(LogYoutube) << "video info fetch failed for" << VideoId;
            emit captionsUnavailable(VideoId);

            return;
        }

        if (!IsVideoPlayable(PlayerResponse))
        {
            qCWarning(LogYoutube) << "video not playable for caption fetch:" << VideoId;
            emit captionsUnavailable(VideoId);

            return;
        }

        const std::vector<FTCaptionTrack> Tracks =
            ParseCaptionTrackList(PlayerResponse.toStdString());
        const FTCaptionTrack* Chosen = PickCaptionTrack(Tracks, PreferredLanguage.toStdString());

        if (Chosen == nullptr)
        {
            qCInfo(LogYoutube) << "no caption track for" << VideoId;
            emit captionsUnavailable(VideoId);

            return;
        }

        qCInfo(LogYoutube) << "fetching caption track" << QString::fromStdString(Chosen->LanguageCode)
                           << "for" << VideoId;

        QNetworkRequest VttRequest(VttUrlFor(*Chosen));
        VttRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                QNetworkRequest::NoLessSafeRedirectPolicy);
        VttRequest.setTransferTimeout(TimeoutMilliseconds);
        VttRequest.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(IosUserAgent));

        QNetworkReply* VttReply = Network->get(VttRequest);

        connect(VttReply, &QNetworkReply::finished, this, [this, VttReply, VideoId]()
        {
            const QByteArray Vtt = VttReply->readAll();
            const bool bOk = VttReply->error() == QNetworkReply::NoError;
            VttReply->deleteLater();

            if (!bOk)
            {
                qCWarning(LogYoutube) << "caption track fetch failed for" << VideoId;
                emit captionsUnavailable(VideoId);

                return;
            }

            emit captionVttFetched(VideoId, QString::fromUtf8(Vtt));
        });
    });
}
