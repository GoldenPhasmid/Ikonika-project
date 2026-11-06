#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

class AppFileTransfer : public QObject
{
    Q_OBJECT

public:
    explicit AppFileTransfer(QObject* Parent = nullptr);
    ~AppFileTransfer() override;

    void Start(const QUrl& Url, const QString& DestinationPath, const QString& ExpectedSha256,
               qint64 ExpectedBytes);

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

    /** Reads the finished part file back to hash it, since a digest cannot carry across a restart. */
    bool MatchesExpectedDigest() const;

    void DiscardPartFile();

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
};
