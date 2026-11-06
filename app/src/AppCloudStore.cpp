#include "AppCloudStore.h"

#include "AppFiles.h"

#include "core/FTLog.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSaveFile>
#include <QUrlQuery>

#include <algorithm>
#include <utility>

namespace
{
    const QString ArchiveFileName{QStringLiteral("ikonika-workspace.zip")};
    const QByteArray MultipartBoundary{"ikonika-sync-boundary"};

    constexpr int HttpUnauthorized{401};
    constexpr int HttpFirstError{400};

    AppCloudFailure MakeFailure(const QString& Message, bool bCredentialRefused = false)
    {
        AppCloudFailure Failure;
        Failure.Message = Message;
        Failure.bCredentialRefused = bCredentialRefused;

        return Failure;
    }

    AppCloudFailure MakeCancelled()
    {
        AppCloudFailure Failure{MakeFailure(QStringLiteral("cancelled"))};
        Failure.bCancelled = true;

        return Failure;
    }

    std::optional<AppCloudFailure> ReadFailure(QNetworkReply& Reply)
    {
        const int Status{Reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()};

        if (Status >= HttpFirstError)
        {
            AppCloudFailure Failure{MakeFailure(QStringLiteral("Drive answered %1").arg(Status))};
            Failure.Status = Status;

            return Failure;
        }

        if (Reply.error() != QNetworkReply::NoError)
        {
            return MakeFailure(Reply.errorString());
        }

        return std::nullopt;
    }
}

AppDriveStore::AppDriveStore(AppAccessTokenSource& Tokens, const QUrl& Root, int TimeoutMilliseconds)
    : Tokens{&Tokens}
    , Root{Root}
    , TimeoutMilliseconds{TimeoutMilliseconds}
    , Network{std::make_unique<QNetworkAccessManager>()}
{
}

AppDriveStore::~AppDriveStore()
{
    CancelAll();
}

void AppDriveStore::FindCopies(FindCallback Done)
{
    QUrl Url{Root};
    Url.setPath(QStringLiteral("/drive/v3/files"));

    QUrlQuery Query;
    Query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("appDataFolder"));
    Query.addQueryItem(QStringLiteral("q"), QStringLiteral("not name contains '%1'").arg(CloudBookNamePrefix));
    Query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id,version,createdTime)"));
    Query.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("100"));
    Url.setQuery(Query);

    Send(Url, QByteArrayLiteral("GET"), {}, {},
         [Done](const std::optional<AppCloudFailure>& Failure, const FAnswer& Answer)
         {
             if (Failure.has_value())
             {
                 Done(Failure, {});

                 return;
             }

             const QByteArray& Body{Answer.Body};
             const QJsonArray Files = QJsonDocument::fromJson(Body).object().value(QStringLiteral("files")).toArray();

             std::vector<std::pair<QDateTime, AppCloudCopy>> Found;

             for (const QJsonValue& Value : Files)
             {
                 const QJsonObject File{Value.toObject()};

                 if (File.value(QStringLiteral("id")).toString().isEmpty())
                 {
                     qCWarning(LogSync) << "Drive listed a copy without an id, the answer was:" << Body.left(2000);
                 }

                 Found.emplace_back(
                     QDateTime::fromString(File.value(QStringLiteral("createdTime")).toString(), Qt::ISODateWithMs),
                     AppCloudCopy{File.value(QStringLiteral("id")).toString(),
                                  File.value(QStringLiteral("version")).toString()});
             }

             std::stable_sort(Found.begin(), Found.end(),
                              [](const auto& Left, const auto& Right) { return Left.first < Right.first; });

             std::vector<AppCloudCopy> Copies;

             for (const auto& [Created, Copy] : Found)
             {
                 Copies.push_back(Copy);
             }

             Done(std::nullopt, Copies);
         });
}

void AppDriveStore::Download(const QString& Id, const QString& LocalPath, DoneCallback Done)
{
    QUrl Url{Root};
    Url.setPath(QStringLiteral("/drive/v3/files/") + Id);
    Url.setQuery(QStringLiteral("alt=media"));

    Send(Url, QByteArrayLiteral("GET"), {}, {},
         [Done, LocalPath](const std::optional<AppCloudFailure>& Failure, const FAnswer& Answer)
         {
             const QByteArray& Body{Answer.Body};

             if (Failure.has_value())
             {
                 Done(Failure);

                 return;
             }

             QSaveFile File(LocalPath);

             if (!File.open(QIODevice::WriteOnly) || File.write(Body) != Body.size() || !File.commit())
             {
                 Done(MakeFailure(QStringLiteral("could not write %1").arg(LocalPath)));

                 return;
             }

             Done(std::nullopt);
         });
}

void AppDriveStore::FetchVersion(const QString& Id, VersionCallback Done)
{
    QUrl Url{Root};
    Url.setPath(QStringLiteral("/drive/v3/files/") + Id);
    Url.setQuery(QStringLiteral("fields=version"));

    Send(Url, QByteArrayLiteral("GET"), {}, {},
         [Done](const std::optional<AppCloudFailure>& Failure, const FAnswer& Answer)
         {
             if (Failure.has_value())
             {
                 Done(Failure, QString());

                 return;
             }

             Done(std::nullopt,
                  QJsonDocument::fromJson(Answer.Body).object().value(QStringLiteral("version")).toString());
         });
}

void AppDriveStore::Upload(const QString& Id, const QString& LocalPath, UploadCallback Done)
{
    const std::optional<QByteArray> Read{ReadWholeFile(LocalPath)};

    if (!Read.has_value())
    {
        qCWarning(LogSync) << "Could not read" << LocalPath << "to upload it";

        Done(MakeFailure(QStringLiteral("could not read %1").arg(LocalPath)), AppCloudCopy{});

        return;
    }

    const QByteArray& Contents{*Read};

    QUrl Url{Root};
    QByteArray Verb;
    QByteArray Body;
    QByteArray ContentType;

    if (Id.isEmpty())
    {
        Url.setPath(QStringLiteral("/upload/drive/v3/files"));
        Url.setQuery(QStringLiteral("uploadType=multipart&fields=id,version"));
        Verb = QByteArrayLiteral("POST");

        QJsonObject Metadata;
        Metadata.insert(QStringLiteral("name"), ArchiveFileName);
        Metadata.insert(QStringLiteral("parents"), QJsonArray{QStringLiteral("appDataFolder")});

        Body = "--" + MultipartBoundary + "\r\n"
               "Content-Type: application/json; charset=UTF-8\r\n\r\n"
               + QJsonDocument(Metadata).toJson(QJsonDocument::Compact) + "\r\n"
               "--" + MultipartBoundary + "\r\n"
               "Content-Type: application/zip\r\n\r\n"
               + Contents + "\r\n"
               "--" + MultipartBoundary + "--\r\n";
        ContentType = "multipart/related; boundary=" + MultipartBoundary;
    }
    else
    {
        Url.setPath(QStringLiteral("/upload/drive/v3/files/") + Id);
        Url.setQuery(QStringLiteral("uploadType=media&fields=id,version"));
        Verb = QByteArrayLiteral("PATCH");
        Body = Contents;
        ContentType = QByteArrayLiteral("application/zip");
    }

    Send(Url, Verb, Body, ContentType,
         [Done](const std::optional<AppCloudFailure>& Failure, const FAnswer& Answer)
         {
             if (Failure.has_value())
             {
                 Done(Failure, AppCloudCopy{});

                 return;
             }

             const QJsonObject Copy{QJsonDocument::fromJson(Answer.Body).object()};

             Done(std::nullopt, AppCloudCopy{Copy.value(QStringLiteral("id")).toString(),
                                             Copy.value(QStringLiteral("version")).toString()});
         });
}

void AppDriveStore::Delete(const QString& Id, DoneCallback Done)
{
    QUrl Url{Root};
    Url.setPath(QStringLiteral("/drive/v3/files/") + Id);

    Send(Url, QByteArrayLiteral("DELETE"), {}, {},
         [Done](const std::optional<AppCloudFailure>& Failure, const FAnswer&)
         {
             Done(Failure);
         });
}

void AppDriveStore::UploadBook(const QString& LocalPath, const QString& Name,
                               const std::shared_ptr<AppBookTransfer>& Transfer, BookProgress Progress,
                               BookUploaded Done)
{
    auto File{std::make_shared<QFile>(LocalPath)};

    if (!File->open(QIODevice::ReadOnly))
    {
        qCWarning(LogSync) << "Could not read" << LocalPath << "to upload it";

        Done(MakeFailure(QStringLiteral("could not read %1").arg(LocalPath)), QString());

        return;
    }

    QJsonObject Metadata;
    Metadata.insert(QStringLiteral("name"), Name);
    Metadata.insert(QStringLiteral("parents"), QJsonArray{QStringLiteral("appDataFolder")});

    FRequest Request;
    Request.Url = Root;
    Request.Url.setPath(QStringLiteral("/upload/drive/v3/files"));
    Request.Url.setQuery(QStringLiteral("uploadType=resumable"));
    Request.Verb = QByteArrayLiteral("POST");
    Request.Body = QJsonDocument(Metadata).toJson(QJsonDocument::Compact);
    Request.ContentType = QByteArrayLiteral("application/json; charset=UTF-8");
    Request.Headers = {{QByteArrayLiteral("X-Upload-Content-Type"), QByteArrayLiteral("application/octet-stream")},
                       {QByteArrayLiteral("X-Upload-Content-Length"), QByteArray::number(File->size())}};
    Request.Transfer = Transfer;

    Send(std::move(Request),
         [this, File, Transfer, Progress, Done](const std::optional<AppCloudFailure>& Failure, const FAnswer& Answer)
         {
             if (Failure.has_value())
             {
                 Done(Failure, QString());

                 return;
             }

             if (Answer.Location.isEmpty())
             {
                 Done(MakeFailure(QStringLiteral("Drive gave no upload session")), QString());

                 return;
             }

             SendBook(File, Answer.Location, Transfer, Progress, Done);
         });
}

void AppDriveStore::SendBook(const std::shared_ptr<QIODevice>& File, const QString& SessionUrl,
                             const std::shared_ptr<AppBookTransfer>& Transfer, BookProgress Progress,
                             BookUploaded Done)
{
    FRequest Request;
    Request.Transfer = Transfer;
    Request.Url = QUrl(SessionUrl);
    Request.Verb = QByteArrayLiteral("PUT");
    Request.ContentType = QByteArrayLiteral("application/octet-stream");
    Request.Upload = File;
    Request.Progress = std::move(Progress);

    Send(std::move(Request),
         [Done](const std::optional<AppCloudFailure>& Failure, const FAnswer& Answer)
         {
             if (Failure.has_value())
             {
                 Done(Failure, QString());

                 return;
             }

             const QString Id{QJsonDocument::fromJson(Answer.Body).object().value(QStringLiteral("id")).toString()};

             if (Id.isEmpty())
             {
                 Done(MakeFailure(QStringLiteral("Drive did not name the uploaded book")), QString());

                 return;
             }

             Done(std::nullopt, Id);
         });
}

void AppDriveStore::DownloadBook(const QString& Id, const QString& LocalPath,
                                 const std::shared_ptr<AppBookTransfer>& Transfer, BookProgress Progress,
                                 BookDone Done)
{
    auto File{std::make_shared<QSaveFile>(LocalPath)};

    if (!File->open(QIODevice::WriteOnly))
    {
        Done(MakeFailure(QStringLiteral("could not write %1").arg(LocalPath)));

        return;
    }

    FRequest Request;
    Request.Url = Root;
    Request.Url.setPath(QStringLiteral("/drive/v3/files/") + Id);
    Request.Url.setQuery(QStringLiteral("alt=media"));
    Request.Verb = QByteArrayLiteral("GET");
    Request.Sink = File;
    Request.Transfer = Transfer;
    Request.Progress = std::move(Progress);

    Send(std::move(Request),
         [File, LocalPath, Done](const std::optional<AppCloudFailure>& Failure, const FAnswer&)
         {
             if (Failure.has_value())
             {
                 File->cancelWriting();
                 File->commit();

                 Done(Failure);

                 return;
             }

             if (!File->commit())
             {
                 Done(MakeFailure(QStringLiteral("could not write %1").arg(LocalPath)));

                 return;
             }

             Done(std::nullopt);
         });
}

void AppDriveStore::DeleteBook(const QString& Id, BookDone Done)
{
    Delete(Id, std::move(Done));
}

void AppDriveStore::CancelBook(const std::shared_ptr<AppBookTransfer>& Transfer)
{
    if (Transfer == nullptr)
    {
        return;
    }

    Transfer->bCancelled = true;

    if (Transfer->Reply != nullptr)
    {
        Transfer->Reply->abort();
    }
}

void AppDriveStore::CancelAll()
{
    ++Generation;

    for (const QPointer<QNetworkReply>& Reply : std::exchange(InFlight, {}))
    {
        if (Reply != nullptr)
        {
            Reply->abort();
        }
    }
}

void AppDriveStore::Send(const QUrl& Url, const QByteArray& Verb, const QByteArray& Body,
                         const QByteArray& ContentType, ReplyCallback Done)
{
    FRequest Request;
    Request.Url = Url;
    Request.Verb = Verb;
    Request.Body = Body;
    Request.ContentType = ContentType;

    Send(std::move(Request), std::move(Done));
}

void AppDriveStore::Send(FRequest Request, ReplyCallback Done)
{
    const int RequestGeneration{Generation};

    Tokens->RequestToken(
        [this, Alive = std::weak_ptr<char>(Lifetime), Request = std::move(Request), Done, RequestGeneration](
            const std::optional<AppCloudFailure>& Failure, const QString& Token)
        {
            if (Alive.expired() || RequestGeneration != Generation)
            {
                return;
            }

            if (Failure.has_value())
            {
                qCWarning(LogSync) << "No access token for Drive:" << Failure->Message;

                Done(Failure, {});

                return;
            }

            if (Request.Transfer != nullptr && Request.Transfer->bCancelled)
            {
                Done(MakeCancelled(), {});

                return;
            }

            QNetworkRequest NetworkRequest(Request.Url);
            NetworkRequest.setRawHeader("Authorization", "Bearer " + Token.toUtf8());
            NetworkRequest.setTransferTimeout(TimeoutMilliseconds);

            if (!Request.ContentType.isEmpty())
            {
                NetworkRequest.setHeader(QNetworkRequest::ContentTypeHeader, Request.ContentType);
            }

            for (const auto& [Name, Value] : Request.Headers)
            {
                NetworkRequest.setRawHeader(Name, Value);
            }

            QNetworkReply* Reply{Request.Upload != nullptr
                                     ? Network->sendCustomRequest(NetworkRequest, Request.Verb, Request.Upload.get())
                                     : Network->sendCustomRequest(NetworkRequest, Request.Verb, Request.Body)};
            InFlight.push_back(Reply);

            if (Request.Transfer != nullptr)
            {
                Request.Transfer->Reply = Reply;
            }

            auto Received{std::make_shared<qint64>(0)};

            if (Request.Sink != nullptr)
            {
                QObject::connect(Reply, &QNetworkReply::readyRead, Reply,
                                 [Reply, Sink = Request.Sink, Received]
                                 {
                                     *Received += Sink->write(Reply->readAll());
                                 });
            }

            if (Request.Progress && Request.Upload != nullptr)
            {
                QObject::connect(Reply, &QNetworkReply::uploadProgress, Reply,
                                 [Progress = Request.Progress](qint64 Sent, qint64 Total)
                                 {
                                     if (Total > 0)
                                     {
                                         Progress(Sent, Total);
                                     }
                                 });
            }

            if (Request.Progress && Request.Sink != nullptr)
            {
                QObject::connect(Reply, &QNetworkReply::downloadProgress, Reply, Request.Progress);
            }

            QObject::connect(Reply, &QNetworkReply::finished, Reply,
                             [this, Reply, Done, RequestGeneration, Upload = Request.Upload, Sink = Request.Sink,
                              Transfer = Request.Transfer, Received]
                             {
                                 Reply->deleteLater();

                                 if (RequestGeneration != Generation)
                                 {
                                     return;
                                 }

                                 std::erase(InFlight, QPointer<QNetworkReply>(Reply));

                                 if (Transfer != nullptr && Transfer->bCancelled)
                                 {
                                     qCInfo(LogSync) << "A book transfer was cancelled";

                                     Done(MakeCancelled(), {});

                                     return;
                                 }

                                 if (Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                                     == HttpUnauthorized)
                                 {
                                     qCWarning(LogSync) << "Drive refused the access token, it is dropped";

                                     Tokens->InvalidateToken();
                                     Done(MakeFailure(QStringLiteral("Drive refused the access token")), {});

                                     return;
                                 }

                                 if (std::optional<AppCloudFailure> Failure{ReadFailure(*Reply)})
                                 {
                                     qCWarning(LogSync) << "Drive request failed:" << Failure->Message;

                                     Done(Failure, {});

                                     return;
                                 }

                                 FAnswer Answer;
                                 Answer.Location = QString::fromUtf8(Reply->rawHeader("Location"));

                                 if (Sink != nullptr)
                                 {
                                     *Received += Sink->write(Reply->readAll());
                                 }
                                 else
                                 {
                                     Answer.Body = Reply->readAll();
                                     *Received = Answer.Body.size();
                                 }

                                 const QVariant Declared{Reply->header(QNetworkRequest::ContentLengthHeader)};

                                 if (Declared.isValid() && Declared.toLongLong() != *Received)
                                 {
                                     Done(MakeFailure(QStringLiteral("Drive cut the answer short")), {});

                                     return;
                                 }

                                 Done(std::nullopt, Answer);
                             });
        });
}
