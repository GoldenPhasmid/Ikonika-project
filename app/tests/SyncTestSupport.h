#pragma once

#include <catch2/catch_test_macros.hpp>

#include "AppCloudStore.h"
#include "AppSyncMerger.h"

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace
{
    template <typename FPredicate>
    bool WaitUntil(FPredicate IsDone, int TimeoutMilliseconds = 5000)
    {
        QElapsedTimer Clock;
        Clock.start();

        while (!IsDone() && Clock.elapsed() < TimeoutMilliseconds)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }

        return IsDone();
    }

    void SpinFor(int Milliseconds)
    {
        QElapsedTimer Clock;
        Clock.start();

        while (Clock.elapsed() < Milliseconds)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
    }

    struct FReceivedRequest
    {
        QByteArray Method;
        QByteArray Target;
        QHash<QByteArray, QByteArray> Headers;
        QByteArray Body;
    };

    class FDriveServer : public QObject
    {
    public:
        FDriveServer()
        {
            REQUIRE(Server.listen(QHostAddress::LocalHost));

            connect(&Server, &QTcpServer::newConnection, this, &FDriveServer::Accept);
        }

        QUrl GetRoot() const
        {
            return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(Server.serverPort()));
        }

        void Answer(int Status, const QByteArray& Body, qint64 DeclaredLength = -1, const QByteArray& Headers = {})
        {
            const qint64 Length{DeclaredLength >= 0 ? DeclaredLength : Body.size()};

            Responses.append("HTTP/1.1 " + QByteArray::number(Status) + " Status\r\n"
                             "Content-Type: application/json\r\n" + Headers +
                             "Content-Length: " + QByteArray::number(Length) + "\r\n"
                             "Connection: close\r\n\r\n" + Body);
            Pieces.append(QList<QByteArray>{});
        }

        void AnswerInPieces(int Status, const QList<QByteArray>& BodyPieces)
        {
            Responses.append("HTTP/1.1 " + QByteArray::number(Status) + " Status\r\n"
                             "Content-Type: application/octet-stream\r\n"
                             "Connection: close\r\n\r\n");
            Pieces.append(BodyPieces);
        }

        std::vector<FReceivedRequest> Requests;

    private:
        void Accept()
        {
            QTcpSocket* Socket{Server.nextPendingConnection()};

            connect(Socket, &QTcpSocket::readyRead, this, [this, Socket]
                    {
                        QByteArray& Received{Pending[Socket]};
                        Received += Socket->readAll();

                        const qsizetype HeadEnd{Received.indexOf("\r\n\r\n")};

                        if (HeadEnd < 0)
                        {
                            return;
                        }

                        FReceivedRequest Request;
                        const QList<QByteArray> Lines{Received.left(HeadEnd).split('\n')};
                        const QList<QByteArray> First{Lines.first().trimmed().split(' ')};

                        Request.Method = First.value(0);
                        Request.Target = First.value(1);

                        for (qsizetype Index = 1; Index < Lines.size(); ++Index)
                        {
                            const qsizetype Colon{Lines[Index].indexOf(':')};

                            if (Colon > 0)
                            {
                                Request.Headers.insert(Lines[Index].left(Colon).trimmed().toLower(),
                                                       Lines[Index].mid(Colon + 1).trimmed());
                            }
                        }

                        const qint64 Expected{Request.Headers.value("content-length", "0").toLongLong()};
                        Request.Body = Received.mid(HeadEnd + 4);

                        if (Request.Body.size() < Expected)
                        {
                            return;
                        }

                        Requests.push_back(Request);

                        if (Responses.isEmpty())
                        {
                            return;
                        }

                        Socket->write(Responses.takeFirst());
                        Socket->flush();
                        SendPieces(Socket, Pieces.takeFirst());
                    });
        }

        void SendPieces(QTcpSocket* Socket, const QList<QByteArray>& Remaining)
        {
            if (Remaining.isEmpty())
            {
                Socket->disconnectFromHost();

                return;
            }

            QTimer::singleShot(PieceInterval, Socket, [this, Socket, Remaining]
                               {
                                   Socket->write(Remaining.first());
                                   Socket->flush();
                                   SendPieces(Socket, Remaining.mid(1));
                               });
        }

        static constexpr int PieceInterval{150};

        QTcpServer Server;
        QList<QByteArray> Responses;
        QList<QList<QByteArray>> Pieces;
        QHash<QTcpSocket*, QByteArray> Pending;
    };

    class FTokenSource : public AppAccessTokenSource
    {
    public:
        void RequestToken(TokenCallback Done) override
        {
            ++RequestCount;

            if (bRefuse)
            {
                AppCloudFailure Failure;
                Failure.Message = QStringLiteral("invalid_grant");
                Failure.bCredentialRefused = true;

                Done(Failure, QString());

                return;
            }

            if (bDefer)
            {
                Pending.push_back(std::move(Done));

                return;
            }

            Done(std::nullopt, QStringLiteral("test-token"));
        }

        void InvalidateToken() override
        {
            ++InvalidateCount;
        }

        void DeliverPending()
        {
            for (const TokenCallback& Done : std::exchange(Pending, {}))
            {
                Done(std::nullopt, QStringLiteral("test-token"));
            }
        }

        bool bRefuse = false;
        bool bDefer = false;
        int RequestCount = 0;
        int InvalidateCount = 0;
        std::vector<TokenCallback> Pending;
    };

    AppCloudFailure MakeOffline()
    {
        AppCloudFailure Failure;
        Failure.Message = QStringLiteral("host unreachable");

        return Failure;
    }

    AppCloudFailure MakeRefused()
    {
        AppCloudFailure Failure;
        Failure.Message = QStringLiteral("invalid_grant");
        Failure.bCredentialRefused = true;

        return Failure;
    }

    class FFakeCloudStore : public AppCloudStore
    {
    public:
        void FindCopies(FindCallback Done) override
        {
            ++FindCount;
            PendingFind = std::move(Done);
        }

        void Download(const QString& Id, const QString& LocalPath, DoneCallback Done) override
        {
            DownloadedIds.push_back(Id);
            DownloadPath = LocalPath;
            PendingDownload = std::move(Done);
        }

        void FetchVersion(const QString& Id, VersionCallback Done) override
        {
            ++VersionCount;
            PendingVersion = std::move(Done);
        }

        void Upload(const QString& Id, const QString& LocalPath, UploadCallback Done) override
        {
            UploadedIds.push_back(Id);

            QFile File(LocalPath);
            REQUIRE(File.open(QIODevice::ReadOnly));
            UploadedBytes = File.readAll();

            PendingUpload = std::move(Done);
        }

        void Delete(const QString& Id, DoneCallback Done) override
        {
            DeletedIds.push_back(Id);
            PendingDelete = std::move(Done);
        }

        void CancelAll() override
        {
            ++CancelCount;
        }

        void FinishFind(const std::optional<AppCloudFailure>& Failure,
                        const std::optional<AppCloudCopy>& Copy)
        {
            FinishFindAll(Failure, Copy.has_value() ? std::vector<AppCloudCopy>{*Copy} : std::vector<AppCloudCopy>{});
        }

        void FinishFindAll(const std::optional<AppCloudFailure>& Failure, const std::vector<AppCloudCopy>& Copies)
        {
            REQUIRE(PendingFind);
            FindCallback Done{std::exchange(PendingFind, {})};
            Done(Failure, Copies);
        }

        void FinishDelete(const std::optional<AppCloudFailure>& Failure)
        {
            REQUIRE(PendingDelete);
            DoneCallback Done{std::exchange(PendingDelete, {})};
            Done(Failure);
        }

        void FinishDownload(const std::optional<AppCloudFailure>& Failure,
                            const QByteArray& Contents = {})
        {
            REQUIRE(PendingDownload);

            if (!Failure.has_value())
            {
                QFile File(DownloadPath);
                REQUIRE(File.open(QIODevice::WriteOnly));
                File.write(Contents);
            }

            DoneCallback Done{std::exchange(PendingDownload, {})};
            Done(Failure);
        }

        void FinishVersion(const std::optional<AppCloudFailure>& Failure, const QString& Version)
        {
            REQUIRE(PendingVersion);
            VersionCallback Done{std::exchange(PendingVersion, {})};
            Done(Failure, Version);
        }

        void FinishUpload(const std::optional<AppCloudFailure>& Failure, const AppCloudCopy& Copy)
        {
            REQUIRE(PendingUpload);
            UploadCallback Done{std::exchange(PendingUpload, {})};
            Done(Failure, Copy);
        }

        int FindCount = 0;
        int VersionCount = 0;
        int CancelCount = 0;
        std::vector<QString> DownloadedIds;
        std::vector<QString> UploadedIds;
        std::vector<QString> DeletedIds;
        QByteArray UploadedBytes;
        QString DownloadPath;

        FindCallback PendingFind;
        DoneCallback PendingDownload;
        VersionCallback PendingVersion;
        UploadCallback PendingUpload;
        DoneCallback PendingDelete;
    };

    class FFakeMerger : public AppSyncMerger
    {
    public:
        AppMergeResult Merge(FTDatabase& Local, FTDatabase& Downloaded) override
        {
            ++MergeCount;

            FTStatement Query{Downloaded.Prepare("SELECT COUNT(*) FROM Cards")};
            DownloadedCardCount = Query.IsValid() && Query.Step() ? Query.GetInt(0) : -1;

            if (WriteDuringMerge)
            {
                WriteDuringMerge(Local);
            }

            AppMergeResult Result;
            Result.bSuccess = bSucceed;
            Result.Error = bSucceed ? QString() : QStringLiteral("rows could not be read");

            return Result;
        }

        bool bSucceed = true;
        int MergeCount = 0;
        int DownloadedCardCount = -1;
        std::function<void(FTDatabase&)> WriteDuringMerge;
    };
}
