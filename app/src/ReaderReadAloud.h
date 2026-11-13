#pragma once

#include "core/FTNarration.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
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

    Q_PROPERTY(bool available READ IsAvailable NOTIFY availableChanged)
    Q_PROPERTY(bool voiceInstalled READ IsVoiceInstalled NOTIFY availableChanged)
    Q_PROPERTY(QString voicePackage READ GetVoicePackage NOTIFY availableChanged)
    Q_PROPERTY(bool playing READ IsPlaying NOTIFY stateChanged)
    Q_PROPERTY(bool started READ IsStarted NOTIFY stateChanged)
    Q_PROPERTY(int paragraph READ GetParagraph NOTIFY stateChanged)
    Q_PROPERTY(int sentenceStart READ GetSentenceStart NOTIFY stateChanged)
    Q_PROPERTY(int sentenceEnd READ GetSentenceEnd NOTIFY stateChanged)
    Q_PROPERTY(int savedParagraph READ GetSavedParagraph NOTIFY savedChanged)
    Q_PROPERTY(int savedCharacter READ GetSavedCharacter NOTIFY savedChanged)
    Q_PROPERTY(qreal speed READ GetSpeed WRITE SetSpeed NOTIFY speedChanged)
    Q_PROPERTY(qreal minimumSpeed READ GetMinimumSpeed CONSTANT)
    Q_PROPERTY(qreal maximumSpeed READ GetMaximumSpeed CONSTANT)

public:
    explicit ReaderReadAloud(const FTParagraphModel& InParagraphs, QObject* Parent = nullptr);

    Q_INVOKABLE void startAt(int Paragraph, int Character);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void next();
    Q_INVOKABLE void stepSpeed();
    Q_INVOKABLE void enableSpeech();

    void SetTitle(const QString& Value);
    void SetSavedPlace(int Paragraph, int Character);

    bool IsAvailable() const;
    bool IsVoiceInstalled() const;
    QString GetVoicePackage() const;
    bool IsPlaying() const;
    bool IsStarted() const;
    int GetParagraph() const;
    int GetSentenceStart() const;
    int GetSentenceEnd() const;
    int GetSavedParagraph() const;
    int GetSavedCharacter() const;
    qreal GetSpeed() const;
    void SetSpeed(qreal Value);
    qreal GetMinimumSpeed() const;
    qreal GetMaximumSpeed() const;

signals:
    void availableChanged();
    void stateChanged();
    void savedChanged();
    void speedChanged();

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
    void Remember(const FTNarrationSentence& Sentence);

    void OnStarted(int Id);
    void OnFinished(int Id);
    void OnFailed();
    void OnSpeedChanged();
    void PublishSession();

    std::string GetLanguageCode() const;

    const FTParagraphModel& Paragraphs;
    QString Title;
    std::deque<FQueuedSentence> Queued;
    std::optional<FTNarrationSentence> Current;
    std::optional<FTNarrationSentence> LastQueued;
    QElapsedTimer SentenceClock;
    QTimer SpeedRestart;
    int NextId = 1;
    int FirstLiveId = 1;
    int SavedParagraph = -1;
    int SavedCharacter = 0;
    bool bPlaying = false;
    bool bReachedEnd = false;
    bool bPublishedStarted = false;
    bool bPublishedPlaying = false;
};
