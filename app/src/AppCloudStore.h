#pragma once

#include <QByteArray>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QIODevice;
class QNetworkAccessManager;
class QNetworkReply;

struct AppCloudCopy
{
    QString Id;
    QString Version;
};

enum class AppCloudFailureKind
{
    Transient,
    Credential,
    Permission,
    Quota
};

struct AppCloudFailure
{
    QString Message;
    int Status = 0;
    AppCloudFailureKind Kind = AppCloudFailureKind::Transient;
    bool bCancelled = false;
};

struct AppBookTransfer
{
    QPointer<QNetworkReply> Reply;
    bool bCancelled = false;
};

class AppAccessTokenSource
{
public:
    using TokenCallback =
        std::function<void(const std::optional<AppCloudFailure>&, const QString&)>;

    virtual ~AppAccessTokenSource() = default;

    virtual void RequestToken(TokenCallback Done) = 0;
    virtual void InvalidateToken() = 0;
};

class AppCloudStore
{
public:
    using FindCallback = std::function<void(const std::optional<AppCloudFailure>&,
                                            const std::vector<AppCloudCopy>&)>;
    using DoneCallback = std::function<void(const std::optional<AppCloudFailure>&)>;
    using VersionCallback =
        std::function<void(const std::optional<AppCloudFailure>&, const QString&)>;
    using UploadCallback =
        std::function<void(const std::optional<AppCloudFailure>&, const AppCloudCopy&)>;

    virtual ~AppCloudStore() = default;

    virtual void FindCopies(FindCallback Done) = 0;
    virtual void Download(const QString& Id, const QString& LocalPath, DoneCallback Done) = 0;
    virtual void FetchVersion(const QString& Id, VersionCallback Done) = 0;
    virtual void Upload(const QString& Id, const QString& LocalPath, UploadCallback Done) = 0;
    virtual void Delete(const QString& Id, DoneCallback Done) = 0;
    virtual void CancelAll() = 0;
};

class AppBookStore
{
public:
    using BookProgress = std::function<void(qint64, qint64)>;
    using BookDone = std::function<void(const std::optional<AppCloudFailure>&)>;
    using BookUploaded = std::function<void(const std::optional<AppCloudFailure>&, const QString&)>;

    virtual ~AppBookStore() = default;

    virtual void UploadBook(const QString& LocalPath, const QString& Name,
                            const std::shared_ptr<AppBookTransfer>& Transfer, BookProgress Progress,
                            BookUploaded Done) = 0;
    virtual void DownloadBook(const QString& Id, const QString& LocalPath,
                              const std::shared_ptr<AppBookTransfer>& Transfer, BookProgress Progress,
                              BookDone Done) = 0;
    virtual void DeleteBook(const QString& Id, BookDone Done) = 0;
    virtual void CancelBook(const std::shared_ptr<AppBookTransfer>& Transfer) = 0;
    virtual void CancelAll() = 0;
};

inline const QString CloudBookNamePrefix{QStringLiteral("ikonika-book-")};

class AppDriveStore : public AppCloudStore, public AppBookStore
{
public:
    AppDriveStore(AppAccessTokenSource& Tokens, const QUrl& Root, int TimeoutMilliseconds);
    ~AppDriveStore() override;

    void FindCopies(FindCallback Done) override;
    void Download(const QString& Id, const QString& LocalPath, DoneCallback Done) override;
    void FetchVersion(const QString& Id, VersionCallback Done) override;
    void Upload(const QString& Id, const QString& LocalPath, UploadCallback Done) override;
    void Delete(const QString& Id, DoneCallback Done) override;
    void CancelAll() override;

    void UploadBook(const QString& LocalPath, const QString& Name, const std::shared_ptr<AppBookTransfer>& Transfer,
                    BookProgress Progress, BookUploaded Done) override;
    void DownloadBook(const QString& Id, const QString& LocalPath, const std::shared_ptr<AppBookTransfer>& Transfer,
                      BookProgress Progress, BookDone Done) override;
    void DeleteBook(const QString& Id, BookDone Done) override;
    void CancelBook(const std::shared_ptr<AppBookTransfer>& Transfer) override;

private:
    struct FAnswer
    {
        QByteArray Body;
        QString Location;
    };

    struct FRequest
    {
        QUrl Url;
        QByteArray Verb;
        QByteArray Body;
        QByteArray ContentType;
        std::vector<std::pair<QByteArray, QByteArray>> Headers;
        std::shared_ptr<QIODevice> Upload;
        std::shared_ptr<QIODevice> Sink;
        std::shared_ptr<AppBookTransfer> Transfer;
        BookProgress Progress;
    };

    using ReplyCallback = std::function<void(const std::optional<AppCloudFailure>&, const FAnswer&)>;

    void Send(const QUrl& Url, const QByteArray& Verb, const QByteArray& Body,
              const QByteArray& ContentType, ReplyCallback Done);
    void Send(FRequest Request, ReplyCallback Done);
    void SendBook(const std::shared_ptr<QIODevice>& File, const QString& SessionUrl,
                  const std::shared_ptr<AppBookTransfer>& Transfer, BookProgress Progress, BookUploaded Done);

    AppAccessTokenSource* Tokens = nullptr;
    QUrl Root;
    int TimeoutMilliseconds = 0;
    int Generation = 0;
    std::unique_ptr<QNetworkAccessManager> Network;
    std::vector<QPointer<QNetworkReply>> InFlight;
    std::shared_ptr<char> Lifetime{std::make_shared<char>()};
};
