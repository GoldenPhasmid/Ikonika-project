#pragma once

#include "core/FTYoutubeCaptions.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

class AppYoutubeCaptions;

/**
 * The YouTube reader's library and the open video's captions, for QML.
 *
 * Owns nothing about playback or the player itself - the screen embeds the IFrame player and
 * calls reportTime() as it plays, the same division ReaderController has with the PDF view it
 * does not render. Word lookup and translation are the same DictionaryController the book reader
 * and the Dictionary screen use, handed the current caption line's text; this class only tracks
 * the library and which line is current.
 */
class YoutubeController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    /** The library as a list of {id, videoId, title}. */
    Q_PROPERTY(QVariantList documents READ GetDocuments NOTIFY documentsChanged)

    /** The open video's id, or empty when none is open. */
    Q_PROPERTY(QString currentVideoId READ GetCurrentVideoId NOTIFY currentVideoChanged)

    /** The open video's title - the video id itself until the real one arrives. */
    Q_PROPERTY(QString currentTitle READ GetCurrentTitle NOTIFY currentVideoChanged)

    /** True while a video is open. */
    Q_PROPERTY(bool watching READ IsWatching NOTIFY currentVideoChanged)

    /** True while the open video's captions are still being fetched. */
    Q_PROPERTY(bool captionsLoading READ AreCaptionsLoading NOTIFY currentVideoChanged)

    /** True once fetching has finished and the video turned out to have no captions. */
    Q_PROPERTY(bool captionsUnavailable READ AreCaptionsUnavailable NOTIFY currentVideoChanged)

    /** The open video's caption lines, as {startMs, durationMs, text}, in order. */
    Q_PROPERTY(QVariantList cues READ GetCues NOTIFY currentVideoChanged)

    /** Which line in cues is showing right now, -1 when none is. Its own signal: this changes
        several times a second while playing, and nothing else in this class does. */
    Q_PROPERTY(int currentCueIndex READ GetCurrentCueIndex NOTIFY currentCueIndexChanged)

public:
    explicit YoutubeController(QObject* Parent = nullptr);
    ~YoutubeController() override;

    /** Re-reads the library. */
    Q_INVOKABLE void refresh();

    /**
     * Parses a pasted URL or bare id, adds it to the library (or brings an existing entry to the
     * front) and opens it. False when UrlOrId names no video.
     */
    Q_INVOKABLE bool addAndOpen(const QString& UrlOrId);

    /** Opens a video already in the library, by its library row id. */
    Q_INVOKABLE bool openVideo(const QString& Id);

    /** Leaves the watching view back to the library. */
    Q_INVOKABLE void closeVideo();

    /** Removes one video from the library. */
    Q_INVOKABLE void removeVideo(const QString& Id);

    /** The player's current playback position, in seconds. Moves currentCueIndex to whichever
        cue spans it, or -1 between them. Called on a timer from the watching screen. */
    Q_INVOKABLE void reportTime(double Seconds);

    Q_INVOKABLE void receiveCaptionText(const QString& VideoId, const QString& RawText);

    Q_INVOKABLE void reportCaptionsUnavailable(const QString& VideoId);

    QVariantList GetDocuments() const;
    QString GetCurrentVideoId() const;
    QString GetCurrentTitle() const;
    bool IsWatching() const;
    bool AreCaptionsLoading() const;
    bool AreCaptionsUnavailable() const;
    QVariantList GetCues() const;
    int GetCurrentCueIndex() const;

signals:
    void documentsChanged();
    void currentVideoChanged();
    void currentCueIndexChanged();

private:
    void OnTitleFetched(const QString& VideoId, const QString& Title);

    void OnCaptionTextFetched(const QString& VideoId, const QString& RawText);
    void OnCaptionsUnavailable(const QString& VideoId);

    void StartCaptionFetch();

    QVariantList Documents;
    QString CurrentRowId;
    QString CurrentVideoId;
    QString CurrentTitle;
    bool bWatching = false;
    bool bCaptionsLoading = false;
    bool bCaptionsUnavailable = false;

    std::vector<FTCaptionCue> Cues;
    QVariantList CuesVariant;
    int CurrentCueIndex = -1;

    std::unique_ptr<AppYoutubeCaptions> Fetcher;
};
