#pragma once

#include "core/FTNarration.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <deque>
#include <optional>
#include <string>

class FTParagraphModel;

class ReaderReadAloud : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Handed to QML through ReaderController.readAloud")

    Q_PROPERTY(bool enabled READ IsEnabled WRITE SetEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool available READ IsAvailable NOTIFY availableChanged)
    Q_PROPERTY(bool playing READ IsPlaying NOTIFY stateChanged)
    Q_PROPERTY(bool started READ IsStarted NOTIFY stateChanged)
    Q_PROPERTY(int paragraph READ GetParagraph NOTIFY stateChanged)
    Q_PROPERTY(int sentenceStart READ GetSentenceStart NOTIFY stateChanged)
    Q_PROPERTY(int sentenceEnd READ GetSentenceEnd NOTIFY stateChanged)

public:
    explicit ReaderReadAloud(const FTParagraphModel& InParagraphs, QObject* Parent = nullptr);

    Q_INVOKABLE void startAt(int Paragraph, int Character);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop();

    void SetTitle(const QString& Value);

    bool IsEnabled() const;
    void SetEnabled(bool bValue);

    bool IsAvailable() const;
    bool IsPlaying() const;
    bool IsStarted() const;
    int GetParagraph() const;
    int GetSentenceStart() const;
    int GetSentenceEnd() const;

signals:
    void enabledChanged();
    void availableChanged();
    void stateChanged();

private:
    struct FQueuedSentence
    {
        int Id = 0;
        FTNarrationSentence Sentence;
        double Seconds = 0.0;
    };

    void PlayFrom(const FTNarrationSentence& Sentence);
    void QueueAhead();
    void Silence();

    void OnStarted(int Id);
    void OnFinished(int Id);
    void OnFailed();
    void PublishSession();

    std::string GetLanguageCode() const;

    const FTParagraphModel& Paragraphs;
    QString Title;
    std::deque<FQueuedSentence> Queued;
    std::optional<FTNarrationSentence> Current;
    std::optional<FTNarrationSentence> LastQueued;
    int NextId = 1;
    int FirstLiveId = 1;
    bool bEnabled = false;
    bool bPlaying = false;
    bool bReachedEnd = false;
    bool bPublishedStarted = false;
    bool bPublishedPlaying = false;
};
