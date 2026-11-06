#include "AppFileTransfer.h"

#include "core/FTLog.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace
{
    constexpr int StallTimeoutMilliseconds = 30000;
}

AppFileTransfer::AppFileTransfer(QObject* Parent)
    : QObject(Parent)
    , Network(std::make_unique<QNetworkAccessManager>())
{
}

AppFileTransfer::~AppFileTransfer()
{
    Reset();
}

bool AppFileTransfer::IsBusy() const
{
    return Reply != nullptr;
}

void AppFileTransfer::Start(const QUrl& Url, const QString& InDestinationPath,
                            const QString& InExpectedSha256, qint64 InExpectedBytes)
{
    if (IsBusy())
    {
        qCWarning(LogUpdate) << "A transfer is already running; ignoring the request for" << Url;

        return;
    }

    DestinationPath = InDestinationPath;
    PartPath = InDestinationPath + QStringLiteral(".part");
    ExpectedSha256 = InExpectedSha256.toLower();
    ExpectedBytes = InExpectedBytes;

    if (!QDir().mkpath(QFileInfo(DestinationPath).absolutePath()))
    {
        Fail(tr("Cannot create the download directory."));

        return;
    }

    // What an interrupted attempt already fetched. Only worth continuing when the published length
    // is known and the file is short of it; anything else starts over.
    const qint64 Existing = QFileInfo(PartPath).size();

    ResumeOffset = ExpectedBytes > 0 && Existing > 0 && Existing < ExpectedBytes ? Existing : 0;
    ReceivedBytes = ResumeOffset;

    PartFile = std::make_unique<QFile>(PartPath);

    const QIODevice::OpenMode Mode = ResumeOffset > 0
        ? QIODevice::WriteOnly | QIODevice::Append
        : QIODevice::WriteOnly | QIODevice::Truncate;

    if (!PartFile->open(Mode))
    {
        const QString Reason = PartFile->errorString();
        PartFile.reset();

        Fail(tr("Cannot write to %1: %2").arg(PartPath, Reason));

        return;
    }

    QNetworkRequest Request(Url);

    Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    Request.setTransferTimeout(StallTimeoutMilliseconds);

    if (ResumeOffset > 0)
    {
        Request.setRawHeader("Range",
                             QByteArrayLiteral("bytes=") + QByteArray::number(ResumeOffset) + "-");

        qCInfo(LogUpdate) << "Resuming" << Url.toString() << "at" << ResumeOffset << "of"
                          << ExpectedBytes << "bytes";
    }
    else
    {
        qCInfo(LogUpdate) << "Downloading" << Url.toString() << "to" << DestinationPath;
    }

    Reply = Network->get(Request);

    connect(Reply, &QNetworkReply::readyRead, this, &AppFileTransfer::OnReadyRead);
    connect(Reply, &QNetworkReply::finished, this, &AppFileTransfer::OnFinished);
    connect(Reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 Received, qint64 Total)
            {
                // A resumed reply counts only its own body, so both halves carry the offset.
                emit progress(ResumeOffset + Received,
                              Total > 0 ? ResumeOffset + Total : ExpectedBytes);
            });
}

void AppFileTransfer::Cancel()
{
    if (!IsBusy())
    {
        return;
    }

    qCInfo(LogUpdate) << "Download cancelled:" << DestinationPath;

    Reply->abort();
    Reset();

    DiscardPartFile();
}

void AppFileTransfer::OnReadyRead()
{
    if (Reply == nullptr || !PartFile)
    {
        return;
    }

    const QByteArray Chunk = Reply->readAll();

    if (Chunk.isEmpty())
    {
        return;
    }

    if (PartFile->write(Chunk) != Chunk.size())
    {
        const QString Reason = PartFile->errorString();

        Reply->abort();
        Reset();
        DiscardPartFile();

        Fail(tr("Cannot write to disk: %1").arg(Reason));

        return;
    }

    ReceivedBytes += Chunk.size();
}

void AppFileTransfer::OnFinished()
{
    if (Reply == nullptr)
    {
        return;
    }

    const QNetworkReply::NetworkError Error = Reply->error();
    const QString ErrorText = Reply->errorString();
    const int StatusCode =
        Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    OnReadyRead();

    Reset();

    // A part file is kept through a network failure, which is the whole point of asking for a
    // range next time. Every other failure below means what is on disk cannot be trusted.
    if (Error != QNetworkReply::NoError)
    {
        Fail(ErrorText);

        return;
    }

    // A server that ignores the range header answers 200 with the whole body, which would have
    // been appended after the bytes already held. Nothing of that is usable.
    if (ResumeOffset > 0 && StatusCode != 206)
    {
        DiscardPartFile();
        Fail(tr("The server sent the whole file instead of the part that was asked for."));

        return;
    }

    if (ExpectedBytes > 0 && ReceivedBytes != ExpectedBytes)
    {
        DiscardPartFile();
        Fail(tr("Download ended early: %1 of %2 bytes.")
                 .arg(ReceivedBytes)
                 .arg(ExpectedBytes));

        return;
    }

    if (!MatchesExpectedDigest())
    {
        DiscardPartFile();
        Fail(tr("The download does not match its published checksum."));

        return;
    }

    QFile::remove(DestinationPath);

    if (!QFile::rename(PartPath, DestinationPath))
    {
        QFile::remove(PartPath);
        Fail(tr("Cannot move the download into place."));

        return;
    }

    qCInfo(LogUpdate) << "Downloaded" << DestinationPath << ReceivedBytes << "bytes, checksum ok";

    emit finished(DestinationPath);
}

bool AppFileTransfer::MatchesExpectedDigest() const
{
    if (ExpectedSha256.isEmpty())
    {
        return true;
    }

    QFile File(PartPath);

    if (!File.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QCryptographicHash Digest(QCryptographicHash::Sha256);

    if (!Digest.addData(&File))
    {
        return false;
    }

    return QString::fromLatin1(Digest.result().toHex()) == ExpectedSha256;
}

void AppFileTransfer::DiscardPartFile()
{
    QFile::remove(PartPath);
}

void AppFileTransfer::Fail(const QString& Message)
{
    qCWarning(LogUpdate) << "Download failed:" << Message;

    emit failed(Message);
}

void AppFileTransfer::Reset()
{
    if (PartFile)
    {
        PartFile->close();
        PartFile.reset();
    }

    if (Reply != nullptr)
    {
        Reply->disconnect(this);
        Reply->deleteLater();
        Reply = nullptr;
    }
}
