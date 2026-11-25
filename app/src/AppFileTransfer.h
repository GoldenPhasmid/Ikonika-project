#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

struct FTransferJob
{
    QUrl Url;
    QString DestinationPath;
    QString Sha256;
    qint64 Bytes = 0;
    QString Title;
};

class AppFileTransfer : public QObject
{
    Q_OBJECT

public:
    explicit AppFileTransfer(QObject* Parent = nullptr);
    ~AppFileTransfer() override;

    void Start(const QUrl& Url, const QString& DestinationPath, const QString& ExpectedSha256,
               qint64 ExpectedBytes);

    void StartInBackground(const FTransferJob& Job, const QList<FTransferJob>& Upcoming);

    void QueueInBackground(const FTransferJob& Job);

    static void CancelBackground(const QString& DestinationPath);

    void Cancel();

    bool IsBusy() const;

signals:
    void progress(qint64 Received, qint64 Total);
    void finished(const QString& FilePath);
    void failed(const QString& Error);

private:
    void OnReadyRead();
    void OnFinished();
    void Fail(const QString& Message);
    void Reset();

    bool MatchesExpectedDigest(const QString& FilePath) const;

    void DiscardPartFile();

#if defined(Q_OS_ANDROID)
    void StartInService(const FTransferJob& Job, const QList<FTransferJob>& Upcoming);
    QString DescribeServiceError(const QString& Code, const QString& Detail) const;
    QString BuildServicePayload(const QList<FTransferJob>& Jobs) const;
    void CancelInService();
    void PollService();
    void StopPolling();
#endif

    std::unique_ptr<QNetworkAccessManager> Network;
    std::unique_ptr<QFile> PartFile;

    QNetworkReply* Reply = nullptr;

    QString DestinationPath;
    QString PartPath;
    QString ExpectedSha256;

    qint64 ExpectedBytes = 0;
    qint64 ReceivedBytes = 0;

    /** What the part file already held when this attempt started. Zero for a fresh transfer. */
    qint64 ResumeOffset = 0;

#if defined(Q_OS_ANDROID)
    QTimer* PollTimer = nullptr;
    QElapsedTimer ProgressClock;

    qint64 LastProgressBytes = 0;
    bool bServiceActive = false;
#endif
};
