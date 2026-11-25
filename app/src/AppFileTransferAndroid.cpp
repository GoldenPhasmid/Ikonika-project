#include "AppFileTransfer.h"

#include "core/FTLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>

namespace
{
    constexpr const char* PackageDownloadClientClass = "com/goldenphasmid/ikonika/PackageDownloadClient";
    constexpr int PollIntervalMilliseconds = 500;
    constexpr qint64 ServiceStallMilliseconds = 60000;

    bool PlatformStartDownload(const QString& Payload)
    {
        QJniObject Context = QNativeInterface::QAndroidApplication::context();
        QJniObject JniPayload = QJniObject::fromString(Payload);

        return QJniObject::callStaticMethod<jboolean>(PackageDownloadClientClass, "start",
            "(Landroid/content/Context;Ljava/lang/String;)Z",
            Context.object<jobject>(), JniPayload.object<jstring>()) == JNI_TRUE;
    }

    void PlatformCancelDownload(const QString& DestinationPath)
    {
        QJniObject Context = QNativeInterface::QAndroidApplication::context();
        QJniObject JniDestination = QJniObject::fromString(DestinationPath);

        QJniObject::callStaticMethod<void>(PackageDownloadClientClass, "cancel",
            "(Landroid/content/Context;Ljava/lang/String;)V",
            Context.object<jobject>(), JniDestination.object<jstring>());
    }

    void PlatformAcknowledgeFinished()
    {
        QJniObject Context = QNativeInterface::QAndroidApplication::context();

        QJniObject::callStaticMethod<void>(PackageDownloadClientClass, "acknowledgeFinished",
            "(Landroid/content/Context;)V", Context.object<jobject>());
    }

    QJsonObject ToJson(const FTransferJob& Job)
    {
        QJsonObject Object;

        Object.insert(QStringLiteral("url"), Job.Url.toString());
        Object.insert(QStringLiteral("destination"), Job.DestinationPath);
        Object.insert(QStringLiteral("sha256"), Job.Sha256.toLower());
        Object.insert(QStringLiteral("bytes"), static_cast<double>(Job.Bytes));
        Object.insert(QStringLiteral("title"), Job.Title);

        return Object;
    }
}

void AppFileTransfer::StartInBackground(const FTransferJob& Job, const QList<FTransferJob>& Upcoming)
{
    StartInService(Job, Upcoming);
}

QString AppFileTransfer::BuildServicePayload(const QList<FTransferJob>& Jobs) const
{
    QJsonArray Entries;

    for (int Index = 0; Index < Jobs.size(); ++Index)
    {
        if (Index == 0 || !QFileInfo::exists(Jobs[Index].DestinationPath))
        {
            Entries.append(ToJson(Jobs[Index]));
        }
    }

    QJsonObject Payload;

    Payload.insert(QStringLiteral("channel"), tr("Downloads"));
    Payload.insert(QStringLiteral("finished"), tr("Open Ikonika to finish setting up."));
    Payload.insert(QStringLiteral("jobs"), Entries);

    return QString::fromUtf8(QJsonDocument(Payload).toJson(QJsonDocument::Compact));
}

void AppFileTransfer::QueueInBackground(const FTransferJob& Job)
{
    if (QFileInfo::exists(Job.DestinationPath))
    {
        return;
    }

    if (!PlatformStartDownload(BuildServicePayload({Job})))
    {
        qCWarning(LogUpdate) << "The download service did not take the queued download" << Job.Url.toString();

        return;
    }

    qCInfo(LogUpdate) << "Handed the queued download" << Job.Url.toString() << "to the download service";
}

void AppFileTransfer::CancelBackground(const QString& DestinationPath)
{
    qCInfo(LogUpdate) << "Asking the download service to drop" << DestinationPath;

    PlatformCancelDownload(DestinationPath);
}

void AppFileTransfer::StartInService(const FTransferJob& Job, const QList<FTransferJob>& Upcoming)
{
    if (IsBusy())
    {
        qCWarning(LogUpdate) << "A transfer is already running; ignoring the request for" << Job.Url;

        return;
    }

    DestinationPath = Job.DestinationPath;
    PartPath = Job.DestinationPath + QStringLiteral(".part");
    ExpectedSha256 = Job.Sha256.toLower();
    ExpectedBytes = Job.Bytes;

    if (!QDir().mkpath(QFileInfo(DestinationPath).absolutePath()))
    {
        Fail(tr("Cannot create the download directory."));

        return;
    }

    QFile::remove(DestinationPath + QStringLiteral(".error"));

    if (QFileInfo::exists(DestinationPath))
    {
        if (MatchesExpectedDigest(DestinationPath))
        {
            qCInfo(LogUpdate) << "The download service already fetched" << DestinationPath << "while the app was closed";

            PlatformAcknowledgeFinished();

            emit finished(DestinationPath);

            return;
        }

        QFile::remove(DestinationPath);
    }

    QList<FTransferJob> Jobs{Job};

    Jobs.append(Upcoming);

    if (!PlatformStartDownload(BuildServicePayload(Jobs)))
    {
        Fail(tr("The download service could not start."));

        return;
    }

    qCInfo(LogUpdate) << "Handed" << Job.Url.toString() << "and" << Upcoming.size() << "queued downloads to the download service";

    bServiceActive = true;
    LastProgressBytes = QFileInfo(PartPath).size();
    ProgressClock.restart();

    if (PollTimer == nullptr)
    {
        PollTimer = new QTimer(this);
        PollTimer->setInterval(PollIntervalMilliseconds);

        connect(PollTimer, &QTimer::timeout, this, &AppFileTransfer::PollService);
    }

    PollTimer->start();
}

void AppFileTransfer::CancelInService()
{
    qCInfo(LogUpdate) << "Download cancelled:" << DestinationPath;

    StopPolling();

    QFile::remove(DestinationPath + QStringLiteral(".error"));

    PlatformCancelDownload(DestinationPath);
}

void AppFileTransfer::StopPolling()
{
    bServiceActive = false;

    if (PollTimer != nullptr)
    {
        PollTimer->stop();
    }
}

QString AppFileTransfer::DescribeServiceError(const QString& Code, const QString& Detail) const
{
    if (Code == QLatin1String("ended_early"))
    {
        return tr("Download ended early: %1 of %2 bytes.").arg(Detail).arg(ExpectedBytes);
    }

    if (Code == QLatin1String("checksum"))
    {
        return tr("The download does not match its published checksum.");
    }

    if (Code == QLatin1String("move"))
    {
        return tr("Cannot move the download into place.");
    }

    if (Code == QLatin1String("whole_file"))
    {
        return tr("The server sent the whole file instead of the part that was asked for.");
    }

    if (Code == QLatin1String("http"))
    {
        return tr("The server answered with status %1.").arg(Detail);
    }

    if (Code == QLatin1String("paused"))
    {
        return tr("The system paused the download. Try again.");
    }

    return Detail.isEmpty() ? tr("The download failed.") : Detail;
}

void AppFileTransfer::PollService()
{
    const QString ErrorPath = DestinationPath + QStringLiteral(".error");

    if (QFileInfo::exists(ErrorPath))
    {
        QFile ErrorFile(ErrorPath);
        QStringList Lines;

        if (ErrorFile.open(QIODevice::ReadOnly))
        {
            Lines = QString::fromUtf8(ErrorFile.readAll()).split(QLatin1Char('\n'));
        }

        ErrorFile.close();
        QFile::remove(ErrorPath);

        StopPolling();
        Fail(DescribeServiceError(Lines.value(0), Lines.value(1)));

        return;
    }

    if (QFileInfo::exists(DestinationPath))
    {
        StopPolling();

        PlatformAcknowledgeFinished();

        qCInfo(LogUpdate) << "Downloaded" << DestinationPath << "by the download service";

        emit finished(DestinationPath);

        return;
    }

    const qint64 Size = QFileInfo(PartPath).size();

    if (Size != LastProgressBytes)
    {
        LastProgressBytes = Size;
        ProgressClock.restart();

        emit progress(Size, ExpectedBytes);

        return;
    }

    if (ProgressClock.hasExpired(ServiceStallMilliseconds))
    {
        StopPolling();
        Fail(tr("The download service stopped responding."));
    }
}
