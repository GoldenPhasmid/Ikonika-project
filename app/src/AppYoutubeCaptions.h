#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QNetworkAccessManager;

class AppYoutubeCaptions : public QObject
{
    Q_OBJECT

public:
    explicit AppYoutubeCaptions(QObject* Parent = nullptr);
    ~AppYoutubeCaptions() override;

    void FetchTitle(const QString& VideoId);

    /** Fetches a caption track for VideoId in PreferredLanguage and emits the WebVTT text. */
    void FetchCaptionTrack(const QString& VideoId, const QString& PreferredLanguage);

signals:
    void titleFetched(const QString& VideoId, const QString& Title);
    void captionVttFetched(const QString& VideoId, const QString& Vtt);
    void captionsUnavailable(const QString& VideoId);

private:
    std::unique_ptr<QNetworkAccessManager> Network;
};
