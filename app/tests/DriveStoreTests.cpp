#include "SyncTestSupport.h"

#include <QDir>
#include <QTemporaryDir>

#include <cstddef>
#include <cstring>
#include <new>

namespace
{
    struct FDriveHarness
    {
        FTokenSource Tokens;
        FDriveServer Server;
        AppDriveStore Store{Tokens, Server.GetRoot(), 5000};
        QTemporaryDir Folder;
    };

    struct FFindOutcome
    {
        bool bFinished = false;
        std::optional<AppCloudFailure> Failure;
        std::vector<AppCloudCopy> Copies;
    };

    FFindOutcome Find(FDriveHarness& Harness)
    {
        FFindOutcome Outcome;

        Harness.Store.FindCopies([&Outcome](const std::optional<AppCloudFailure>& Failure,
                                            const std::vector<AppCloudCopy>& Copies)
                                 {
                                     Outcome.bFinished = true;
                                     Outcome.Failure = Failure;
                                     Outcome.Copies = Copies;
                                 });

        REQUIRE(WaitUntil([&Outcome] { return Outcome.bFinished; }));

        return Outcome;
    }

    struct FDoneOutcome
    {
        bool bFinished = false;
        std::optional<AppCloudFailure> Failure;
    };

    FDoneOutcome DownloadTo(FDriveHarness& Harness, const QString& Path)
    {
        FDoneOutcome Outcome;

        Harness.Store.Download("copy-1", Path, [&Outcome](const std::optional<AppCloudFailure>& Failure)
                               {
                                   Outcome.bFinished = true;
                                   Outcome.Failure = Failure;
                               });

        REQUIRE(WaitUntil([&Outcome] { return Outcome.bFinished; }));

        return Outcome;
    }

    QByteArray ReadFile(const QString& Path)
    {
        QFile File(Path);

        return File.open(QIODevice::ReadOnly) ? File.readAll() : QByteArray();
    }

    void WriteFile(const QString& Path, const QByteArray& Contents)
    {
        QFile File(Path);
        REQUIRE(File.open(QIODevice::WriteOnly));
        File.write(Contents);
    }
}

TEST_CASE("Finding lists every cloud copy, oldest first", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(200, R"({"files":[
        {"id":"newer","version":"9","createdTime":"2026-10-12T08:00:00.000Z"},
        {"id":"older","version":"4","createdTime":"2026-10-01T08:00:00.000Z"}]})");

    const FFindOutcome Outcome{Find(Harness)};

    CHECK_FALSE(Outcome.Failure.has_value());
    REQUIRE(Outcome.Copies.size() == 2);
    CHECK(Outcome.Copies[0].Id == "older");
    CHECK(Outcome.Copies[0].Version == "4");
    CHECK(Outcome.Copies[1].Id == "newer");
}

TEST_CASE("An account with no cloud copy finds none, which is not a failure", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(200, R"({"files":[]})");

    const FFindOutcome Outcome{Find(Harness)};

    CHECK_FALSE(Outcome.Failure.has_value());
    CHECK(Outcome.Copies.empty());
}

TEST_CASE("Drive requests carry the access token and look only in the app folder", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(200, R"({"files":[]})");

    Find(Harness);

    REQUIRE(Harness.Server.Requests.size() == 1);
    CHECK(Harness.Server.Requests.front().Headers.value("authorization") == "Bearer test-token");
    CHECK(Harness.Server.Requests.front().Target.contains("appDataFolder"));
}

TEST_CASE("A refused credential fails as refused and sends nothing to Drive", "[sync]")
{
    FDriveHarness Harness;
    Harness.Tokens.bRefuse = true;

    const FFindOutcome Outcome{Find(Harness)};

    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Credential);
    CHECK(Harness.Server.Requests.empty());
}

TEST_CASE("Drive forbidding the request for lack of permission fails as a missing permission", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(403, R"({"error":{"code":403,"errors":[{"reason":"insufficientPermissions"}]}})");

    const FFindOutcome Outcome{Find(Harness)};

    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Permission);
    CHECK(Outcome.Failure->Status == 403);
    CHECK(Outcome.Failure->Message.contains("insufficientPermissions"));
}

TEST_CASE("Drive forbidding the request without a reason fails as a missing permission", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(403, "");

    const FFindOutcome Outcome{Find(Harness)};

    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Permission);
}

TEST_CASE("Drive limiting the rate of requests fails as transient", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(403, R"({"error":{"code":403,"errors":[{"reason":"userRateLimitExceeded"}]}})");

    const FFindOutcome Outcome{Find(Harness)};

    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Transient);
}

TEST_CASE("Drive running out of storage fails as a full quota", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(403, R"({"error":{"code":403,"errors":[{"reason":"storageQuotaExceeded"}]}})");

    const FFindOutcome Outcome{Find(Harness)};

    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Quota);
}

TEST_CASE("Drive answering unauthorised drops the access token without blaming the credential", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(401, R"({"error":{"code":401}})");

    const FFindOutcome Outcome{Find(Harness)};

    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Transient);
    CHECK(Harness.Tokens.InvalidateCount == 1);
}

TEST_CASE("A server error fails without blaming the credential", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(503, R"({"error":{"code":503}})");

    const FFindOutcome Outcome{Find(Harness)};

    REQUIRE(Outcome.Failure.has_value());
    CHECK(Outcome.Failure->Kind == AppCloudFailureKind::Transient);
}

TEST_CASE("A request that hears nothing fails after the timeout", "[sync]")
{
    FTokenSource Tokens;
    FDriveServer Server;
    AppDriveStore Store{Tokens, Server.GetRoot(), 200};

    bool bFinished{false};
    std::optional<AppCloudFailure> Failure;

    Store.FindCopies([&](const std::optional<AppCloudFailure>& Result, const std::vector<AppCloudCopy>&)
                   {
                       bFinished = true;
                       Failure = Result;
                   });

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }, 3000));
    REQUIRE(Failure.has_value());
    CHECK(Failure->Kind == AppCloudFailureKind::Transient);
}

TEST_CASE("A complete download lands at the destination", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(200, "database bytes");

    const QString Path{Harness.Folder.filePath("download.db")};
    const FDoneOutcome Outcome{DownloadTo(Harness, Path)};

    CHECK_FALSE(Outcome.Failure.has_value());
    CHECK(ReadFile(Path) == "database bytes");
    CHECK(Harness.Server.Requests.front().Target.contains("alt=media"));
}

TEST_CASE("A download cut short fails and leaves the destination as it was", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(200, "half", 400);

    const QString Path{Harness.Folder.filePath("download.db")};
    WriteFile(Path, "previous download");

    const FDoneOutcome Outcome{DownloadTo(Harness, Path)};

    REQUIRE(Outcome.Failure.has_value());
    CHECK(ReadFile(Path) == "previous download");
}

TEST_CASE("The version comes from the copy's metadata", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(200, R"({"version":"17"})");

    bool bFinished{false};
    QString Version;

    Harness.Store.FetchVersion("copy-1", [&](const std::optional<AppCloudFailure>&, const QString& Result)
                               {
                                   bFinished = true;
                                   Version = Result;
                               });

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    CHECK(Version == "17");
    CHECK(Harness.Server.Requests.front().Target.contains("copy-1"));
}

TEST_CASE("Uploading without an id creates the copy in the app folder", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(200, R"({"id":"created","version":"1"})");

    const QString Path{Harness.Folder.filePath("snapshot.db")};
    WriteFile(Path, "snapshot bytes");

    bool bFinished{false};
    AppCloudCopy Copy;

    Harness.Store.Upload(QString(), Path, [&](const std::optional<AppCloudFailure>&, const AppCloudCopy& Result)
                         {
                             bFinished = true;
                             Copy = Result;
                         });

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    CHECK(Copy.Id == "created");
    CHECK(Copy.Version == "1");

    REQUIRE(Harness.Server.Requests.size() == 1);
    CHECK(Harness.Server.Requests.front().Method == "POST");
    CHECK(Harness.Server.Requests.front().Body.contains("appDataFolder"));
    CHECK(Harness.Server.Requests.front().Body.contains("snapshot bytes"));
}

TEST_CASE("Uploading with an id replaces that copy", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(200, R"({"id":"copy-1","version":"8"})");

    const QString Path{Harness.Folder.filePath("snapshot.db")};
    WriteFile(Path, "snapshot bytes");

    bool bFinished{false};
    AppCloudCopy Copy;

    Harness.Store.Upload("copy-1", Path, [&](const std::optional<AppCloudFailure>&, const AppCloudCopy& Result)
                         {
                             bFinished = true;
                             Copy = Result;
                         });

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    CHECK(Copy.Version == "8");
    CHECK(Harness.Server.Requests.front().Method == "PATCH");
    CHECK(Harness.Server.Requests.front().Target.contains("copy-1"));
    CHECK(Harness.Server.Requests.front().Body == "snapshot bytes");
}

TEST_CASE("Cancelled requests never call back", "[sync]")
{
    FTokenSource Tokens;
    FDriveServer Server;
    AppDriveStore Store{Tokens, Server.GetRoot(), 300};

    bool bCalledBack{false};

    Store.FindCopies([&bCalledBack](const std::optional<AppCloudFailure>&, const std::vector<AppCloudCopy>&)
                   {
                       bCalledBack = true;
                   });

    Store.CancelAll();
    SpinFor(600);

    CHECK_FALSE(bCalledBack);
}

TEST_CASE("A token that arrives after cancelling sends nothing", "[sync]")
{
    FTokenSource Tokens;
    Tokens.bDefer = true;
    FDriveServer Server;
    AppDriveStore Store{Tokens, Server.GetRoot(), 5000};

    bool bCalledBack{false};

    Store.FindCopies([&bCalledBack](const std::optional<AppCloudFailure>&, const std::vector<AppCloudCopy>&)
                   {
                       bCalledBack = true;
                   });

    Store.CancelAll();
    Tokens.DeliverPending();
    SpinFor(200);

    CHECK_FALSE(bCalledBack);
    CHECK(Server.Requests.empty());
}

TEST_CASE("A token that arrives after the Drive store is gone touches nothing", "[sync]")
{
    FTokenSource Tokens;
    Tokens.bDefer = true;
    FDriveServer Server;

    bool bCalledBack{false};

    alignas(AppDriveStore) std::byte Storage[sizeof(AppDriveStore)];
    AppDriveStore* Store{new (Storage) AppDriveStore{Tokens, Server.GetRoot(), 5000}};

    Store->FindCopies([&bCalledBack](const std::optional<AppCloudFailure>&, const std::vector<AppCloudCopy>&)
                    {
                        bCalledBack = true;
                    });

    Store->~AppDriveStore();
    std::memset(Storage, 0, sizeof(Storage));

    Tokens.DeliverPending();
    SpinFor(200);

    CHECK_FALSE(bCalledBack);
    CHECK(Server.Requests.empty());
}

TEST_CASE("Deleting a copy sends a delete for it", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(204, QByteArray());

    bool bFinished{false};
    std::optional<AppCloudFailure> Failure;

    Harness.Store.Delete("copy-2", [&](const std::optional<AppCloudFailure>& Result)
                         {
                             bFinished = true;
                             Failure = Result;
                         });

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    CHECK_FALSE(Failure.has_value());
    REQUIRE(Harness.Server.Requests.size() == 1);
    CHECK(Harness.Server.Requests.front().Method == "DELETE");
    CHECK(Harness.Server.Requests.front().Target.contains("copy-2"));
}

TEST_CASE("Uploading a file that cannot be read fails without a request", "[sync]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(200, R"({"id":"copy-1","version":"8"})");

    bool bFinished{false};
    std::optional<AppCloudFailure> Failure;

    Harness.Store.Upload("copy-1", Harness.Folder.filePath("missing.zip"),
                         [&](const std::optional<AppCloudFailure>& Result, const AppCloudCopy&)
                         {
                             bFinished = true;
                             Failure = Result;
                         });

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    CHECK(Failure.has_value());
    CHECK(Harness.Server.Requests.empty());
}


TEST_CASE("Looking for the workspace copy leaves uploaded books out", "[sync][cloud]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(200, R"({"files":[]})");

    Find(Harness);

    REQUIRE(Harness.Server.Requests.size() == 1);
    CHECK(QUrl::fromPercentEncoding(Harness.Server.Requests.front().Target).contains("not name contains 'ikonika-book-'"));
}

TEST_CASE("A book uploads through a resumable session and comes back named", "[sync][cloud]")
{
    FDriveHarness Harness;
    const QString Path{Harness.Folder.filePath("book.pdf")};
    WriteFile(Path, "book bytes");

    const QByteArray Session{Harness.Server.GetRoot().toString().toUtf8() + "/upload/session-1"};
    Harness.Server.Answer(200, "", -1, "Location: " + Session + "\r\n");
    Harness.Server.Answer(200, R"({"id":"book-1"})");

    bool bFinished{false};
    std::optional<AppCloudFailure> Failure;
    QString Id;
    qint64 LastTotal{0};

    Harness.Store.UploadBook(Path, "ikonika-book-1.pdf", nullptr,
                             [&LastTotal](qint64, qint64 Total) { LastTotal = Total; },
                             [&](const std::optional<AppCloudFailure>& Result, const QString& Uploaded)
                             {
                                 bFinished = true;
                                 Failure = Result;
                                 Id = Uploaded;
                             });

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    CHECK_FALSE(Failure.has_value());
    CHECK(Id == "book-1");
    CHECK(LastTotal == 10);

    REQUIRE(Harness.Server.Requests.size() == 2);
    CHECK(Harness.Server.Requests[0].Target.contains("uploadType=resumable"));
    CHECK(Harness.Server.Requests[0].Headers.value("x-upload-content-length") == "10");
    CHECK(Harness.Server.Requests[0].Body.contains("ikonika-book-1.pdf"));
    CHECK(Harness.Server.Requests[0].Body.contains("appDataFolder"));
    CHECK(Harness.Server.Requests[1].Method == "PUT");
    CHECK(Harness.Server.Requests[1].Target == "/upload/session-1");
    CHECK(Harness.Server.Requests[1].Body == "book bytes");
}

TEST_CASE("A book upload with no session fails", "[sync][cloud]")
{
    FDriveHarness Harness;
    const QString Path{Harness.Folder.filePath("book.pdf")};
    WriteFile(Path, "book bytes");
    Harness.Server.Answer(200, "");

    bool bFinished{false};
    std::optional<AppCloudFailure> Failure;

    Harness.Store.UploadBook(Path, "ikonika-book-1.pdf", nullptr, [](qint64, qint64) {},
                             [&](const std::optional<AppCloudFailure>& Result, const QString&)
                             {
                                 bFinished = true;
                                 Failure = Result;
                             });

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    CHECK(Failure.has_value());
    CHECK(Harness.Server.Requests.size() == 1);
}

TEST_CASE("A downloaded book is written to disk with its progress", "[sync][cloud]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(200, "the whole book");

    const QString Path{Harness.Folder.filePath("book.epub")};

    bool bFinished{false};
    std::optional<AppCloudFailure> Failure;
    qint64 LastReceived{0};

    Harness.Store.DownloadBook("book-1", Path, nullptr, [&LastReceived](qint64 Received, qint64) { LastReceived = Received; },
                               [&](const std::optional<AppCloudFailure>& Result)
                               {
                                   bFinished = true;
                                   Failure = Result;
                               });

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    CHECK_FALSE(Failure.has_value());
    CHECK(ReadFile(Path) == "the whole book");
    CHECK(LastReceived == 14);
    CHECK(Harness.Server.Requests.front().Target.contains("book-1"));
    CHECK(Harness.Server.Requests.front().Target.contains("alt=media"));
}

TEST_CASE("A downloaded book Drive sends without a length still reports what arrived", "[sync][cloud]")
{
    FDriveHarness Harness;
    const QByteArray Piece(64 * 1024, 'b');
    Harness.Server.AnswerInPieces(200, {Piece, Piece, Piece});

    const QString Path{Harness.Folder.filePath("book.epub")};

    bool bFinished{false};
    std::optional<AppCloudFailure> Failure;
    qint64 ReceivedWithoutTotal{0};

    Harness.Store.DownloadBook("book-1", Path, nullptr,
                               [&ReceivedWithoutTotal](qint64 Received, qint64 Total)
                               {
                                   if (Total <= 0)
                                   {
                                       ReceivedWithoutTotal = Received;
                                   }
                               },
                               [&](const std::optional<AppCloudFailure>& Result)
                               {
                                   bFinished = true;
                                   Failure = Result;
                               });

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    CHECK_FALSE(Failure.has_value());
    CHECK(ReadFile(Path).size() == Piece.size() * 3);
    CHECK(ReceivedWithoutTotal > 0);
}

TEST_CASE("A book Drive no longer has fails with its status and writes nothing", "[sync][cloud]")
{
    FDriveHarness Harness;
    Harness.Server.Answer(404, R"({"error":{"code":404}})");

    const QString Path{Harness.Folder.filePath("book.epub")};

    bool bFinished{false};
    std::optional<AppCloudFailure> Failure;

    Harness.Store.DownloadBook("book-1", Path, nullptr, [](qint64, qint64) {},
                               [&](const std::optional<AppCloudFailure>& Result)
                               {
                                   bFinished = true;
                                   Failure = Result;
                               });

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    REQUIRE(Failure.has_value());
    CHECK(Failure->Status == 404);
    CHECK_FALSE(QFile::exists(Path));
}

TEST_CASE("A cancelled book download stops and reports it was cancelled", "[sync][cloud]")
{
    FDriveHarness Harness;
    const QString Path{Harness.Folder.filePath("book.epub")};
    const auto Transfer{std::make_shared<AppBookTransfer>()};

    bool bFinished{false};
    std::optional<AppCloudFailure> Failure;

    Harness.Store.DownloadBook("book-1", Path, Transfer, [](qint64, qint64) {},
                               [&](const std::optional<AppCloudFailure>& Result)
                               {
                                   bFinished = true;
                                   Failure = Result;
                               });

    REQUIRE(WaitUntil([&Harness] { return !Harness.Server.Requests.empty(); }));

    Harness.Store.CancelBook(Transfer);

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    REQUIRE(Failure.has_value());
    CHECK(Failure->bCancelled);
    CHECK_FALSE(QFile::exists(Path));
}

TEST_CASE("A book cancelled before its request starts never reaches Drive", "[sync][cloud]")
{
    FDriveHarness Harness;
    Harness.Tokens.bDefer = true;

    const QString Path{Harness.Folder.filePath("book.epub")};
    const auto Transfer{std::make_shared<AppBookTransfer>()};

    bool bFinished{false};
    std::optional<AppCloudFailure> Failure;

    Harness.Store.DownloadBook("book-1", Path, Transfer, [](qint64, qint64) {},
                               [&](const std::optional<AppCloudFailure>& Result)
                               {
                                   bFinished = true;
                                   Failure = Result;
                               });

    Harness.Store.CancelBook(Transfer);
    Harness.Tokens.DeliverPending();

    REQUIRE(WaitUntil([&bFinished] { return bFinished; }));
    REQUIRE(Failure.has_value());
    CHECK(Failure->bCancelled);
    CHECK(Harness.Server.Requests.empty());
}
