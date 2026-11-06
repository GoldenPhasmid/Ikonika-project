#include "AppBookCloud.h"

#include "AppCloudStore.h"
#include "AppDatabase.h"
#include "AppWorkspace.h"
#include "AppWorkspaceHost.h"

#include "core/FTCloudBookRepository.h"
#include "core/FTLog.h"
#include "core/FTReaderRepository.h"
#include "core/FTUuid.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <optional>
#include <unordered_set>

namespace
{
    constexpr int HttpNotFound{404};

    AppBookCloud* Instance = nullptr;

}

AppBookCloud* AppBookCloud::GetInstance()
{
    return Instance;
}

AppBookCloud::AppBookCloud(AppWorkspaceHost& Host, AppBookStore* Store, QObject* Parent)
    : QObject(Parent)
    , Host{&Host}
    , Store{Store}
{
    Instance = this;

    connect(&Host, &AppWorkspaceHost::aboutToSwitch, this, [this] { CancelTransfers(); });
    connect(&Host, &AppWorkspaceHost::workspaceOpened, this, [this] { emit availabilityChanged(); });
    connect(&Host, &AppWorkspaceHost::switched, this, [this] { emit availabilityChanged(); });
}

AppBookCloud::~AppBookCloud()
{
    if (Store != nullptr)
    {
        Store->CancelAll();
    }

    if (Instance == this)
    {
        Instance = nullptr;
    }
}

bool AppBookCloud::IsAvailable() const
{
    if (Store == nullptr || Host->GetActive() == nullptr)
    {
        return false;
    }

    const std::optional<AppAccount> Account{Host->GetRegistry().GetActiveAccount()};

    return Account.has_value() && !Account->bNeedsReconnect;
}

QVariantMap AppBookCloud::GetTransfers() const
{
    QVariantMap Result;

    for (auto Iterator = Transfers.cbegin(); Iterator != Transfers.cend(); ++Iterator)
    {
        Result.insert(Iterator.key(), Iterator.value());
    }

    return Result;
}

void AppBookCloud::Upload(const QString& DocumentId)
{
    if (!IsAvailable() || Transfers.contains(DocumentId))
    {
        return;
    }

    const std::optional<FTReaderDocumentRow> Document{
        AppDatabase::Get().GetReaderLibrary().FindDocument(DocumentId.toStdString())};

    if (!Document.has_value() || !Document->bExists || !Document->CloudId.empty())
    {
        qCWarning(LogSync) << "Cannot upload the book" << DocumentId << ": it is missing or already in the cloud";

        return;
    }

    const QString Path{QString::fromStdString(Document->FilePath)};
    const QString Title{QString::fromStdString(Document->Title)};
    const QString Name{CloudBookNamePrefix + QString::fromStdString(MakeUuid()) + QLatin1Char('.')
                       + QString::fromStdString(Document->Format).toLower()};
    const qint64 Size{QFileInfo(Path).size()};

    qCInfo(LogSync) << "Uploading the book" << Title << Size << "bytes";

    SetProgress(DocumentId, 0.0);

    const auto Transfer{std::make_shared<AppBookTransfer>()};
    Handles.insert(DocumentId, Transfer);

    Store->UploadBook(
        Path, Name, Transfer,
        [this, DocumentId](qint64 Sent, qint64 Total) { SetProgress(DocumentId, double(Sent) / double(Total)); },
        [this, DocumentId, Document, Title, Size](const std::optional<AppCloudFailure>& Failure,
                                                  const QString& DriveFileId)
        {
            FinishTransfer(DocumentId);

            if (Failure.has_value() && Failure->bCancelled)
            {
                qCInfo(LogSync) << "Uploading the book" << Title << "was cancelled";

                return;
            }

            if (Failure.has_value())
            {
                qCWarning(LogSync) << "Uploading the book" << Title << "failed:" << Failure->Message;

                emit failed(tr("Could not upload %1").arg(Title));

                return;
            }

            const FTReadingPlace Place{Document->LastPosition, Document->LastZoom, Document->LastScrollX,
                                       Document->LastScrollY, Document->PositionCount};
            const std::optional<std::string> CloudId{AppDatabase::Get().GetCloudBooks().AddBook(
                Document->Title, Document->Format, Size, DriveFileId.toStdString(), Place)};

            if (!CloudId.has_value())
            {
                qCWarning(LogSync) << "Could not record the uploaded book" << Title << ", removing it from Drive";

                Store->DeleteBook(DriveFileId, [](const std::optional<AppCloudFailure>&) {});

                emit failed(tr("Could not upload %1").arg(Title));

                return;
            }

            AppDatabase::Get().GetReaderLibrary().SetCloudId(Document->Id, *CloudId);

            qCInfo(LogSync) << "Uploaded the book" << Title;

            emit libraryChanged();
        });
}

void AppBookCloud::Download(const QString& CloudId)
{
    if (!IsAvailable() || Transfers.contains(CloudId))
    {
        return;
    }

    const std::optional<FTCloudBookRow> Book{AppDatabase::Get().GetCloudBooks().FindBook(CloudId.toStdString())};

    if (!Book.has_value())
    {
        qCWarning(LogSync) << "Cannot download the book" << CloudId << ": it is not in the cloud library";

        return;
    }

    const QString Directory{Host->GetActive()->GetFolder().GetBooksDirectory()};

    if (!QDir().mkpath(Directory))
    {
        qCWarning(LogSync) << "Could not create the books folder" << Directory;

        return;
    }

    const QString Path{QDir(Directory).filePath(CloudId + QLatin1Char('.') + QString::fromStdString(Book->Format).toLower())};
    const QString Title{QString::fromStdString(Book->Title)};

    qCInfo(LogSync) << "Downloading the book" << Title;

    SetProgress(CloudId, 0.0);

    const auto Transfer{std::make_shared<AppBookTransfer>()};
    Handles.insert(CloudId, Transfer);

    Store->DownloadBook(
        QString::fromStdString(Book->DriveFileId), Path, Transfer,
        [this, CloudId, FileSize = Book->FileSize](qint64 Received, qint64 Total)
        {
            const qint64 Expected{Total > 0 ? Total : FileSize};

            if (Expected <= 0)
            {
                return;
            }

            SetProgress(CloudId, double(Received) / double(Expected));
        },
        [this, CloudId, Book, Path, Title](const std::optional<AppCloudFailure>& Failure)
        {
            FinishTransfer(CloudId);

            if (Failure.has_value() && Failure->bCancelled)
            {
                qCInfo(LogSync) << "Downloading the book" << Title << "was cancelled";

                return;
            }

            if (Failure.has_value())
            {
                qCWarning(LogSync) << "Downloading the book" << Title << "failed:" << Failure->Message;

                emit failed(tr("Could not download %1").arg(Title));

                return;
            }

            FTReaderRepository& Library{AppDatabase::Get().GetReaderLibrary()};
            const std::optional<std::string> DocumentId{
                Library.AddDocument(Path.toStdString(), Book->Title, Book->Format)};

            if (!DocumentId.has_value())
            {
                qCWarning(LogSync) << "Could not add the downloaded book" << Title << "to the library";

                QFile::remove(Path);

                return;
            }

            Library.SetCloudId(*DocumentId, Book->Id);
            Library.SetPosition(*DocumentId, Book->LastPosition);
            Library.SetZoom(*DocumentId, Book->LastZoom);
            Library.SetScroll(*DocumentId, Book->LastScrollX, Book->LastScrollY);
            Library.SetPositionCount(*DocumentId, Book->PositionCount);

            qCInfo(LogSync) << "Downloaded the book" << Title;

            emit libraryChanged();
        });
}

void AppBookCloud::RemoveFromCloud(const QString& CloudId)
{
    if (!IsAvailable() || Transfers.contains(CloudId))
    {
        return;
    }

    const std::optional<FTCloudBookRow> Book{AppDatabase::Get().GetCloudBooks().FindBook(CloudId.toStdString())};

    if (!Book.has_value())
    {
        return;
    }

    const QString Title{QString::fromStdString(Book->Title)};

    qCInfo(LogSync) << "Deleting the book" << Title << "from the cloud";

    Store->DeleteBook(
        QString::fromStdString(Book->DriveFileId),
        [this, CloudId, Title](const std::optional<AppCloudFailure>& Failure)
        {
            if (Failure.has_value() && Failure->Status != HttpNotFound)
            {
                qCWarning(LogSync) << "Deleting the book" << Title << "from the cloud failed:" << Failure->Message;

                emit failed(tr("Could not delete %1 from the cloud").arg(Title));

                return;
            }

            AppDatabase::Get().GetCloudBooks().RemoveBook(CloudId.toStdString());
            ForgetDeletedBooks();

            qCInfo(LogSync) << "Deleted the book" << Title << "from the cloud";

            emit libraryChanged();
        });
}

void AppBookCloud::Cancel(const QString& Key)
{
    const auto Found{Handles.constFind(Key)};

    if (Store == nullptr || Found == Handles.cend())
    {
        return;
    }

    qCInfo(LogSync) << "Cancelling the book transfer" << Key;

    Store->CancelBook(Found.value());
}

void AppBookCloud::ForgetLocalCopy(const FTReaderDocumentRow& Row) const
{
    if (Row.CloudId.empty() || Host->GetActive() == nullptr)
    {
        return;
    }

    const QString Directory{QDir(Host->GetActive()->GetFolder().GetBooksDirectory()).absolutePath()};
    const QString Path{QFileInfo(QString::fromStdString(Row.FilePath)).absoluteFilePath()};

    if (!Path.startsWith(Directory + QLatin1Char('/')))
    {
        return;
    }

    if (!QFile::remove(Path))
    {
        qCWarning(LogSync) << "Could not delete the downloaded book" << Path;

        return;
    }

    qCInfo(LogSync) << "Deleted the downloaded book" << Path;
}

void AppBookCloud::ForgetDeletedBooks() const
{
    if (!AppDatabase::Get().IsReady())
    {
        return;
    }

    const std::vector<std::string> DeletedIds{AppDatabase::Get().GetCloudBooks().ListDeletedIds()};

    if (DeletedIds.empty())
    {
        return;
    }

    const std::unordered_set<std::string> Deleted(DeletedIds.begin(), DeletedIds.end());
    FTReaderRepository& Library{AppDatabase::Get().GetReaderLibrary()};

    for (const FTReaderDocumentRow& Row : Library.ListDocuments())
    {
        if (Row.CloudId.empty() || !Deleted.contains(Row.CloudId))
        {
            continue;
        }

        qCInfo(LogSync) << "The book" << QString::fromStdString(Row.Title)
                        << "was deleted from the cloud, removing it from this device";

        ForgetLocalCopy(Row);
        Library.RemoveDocument(Row.Id);
    }
}

void AppBookCloud::SetProgress(const QString& Key, double Progress)
{
    Transfers.insert(Key, std::clamp(Progress, 0.0, 1.0));

    emit transfersChanged();
}

void AppBookCloud::FinishTransfer(const QString& Key)
{
    Transfers.remove(Key);
    Handles.remove(Key);

    emit transfersChanged();
}

void AppBookCloud::CancelTransfers()
{
    if (Store != nullptr)
    {
        Store->CancelAll();
    }

    if (Transfers.isEmpty())
    {
        return;
    }

    qCInfo(LogSync) << "Cancelled" << Transfers.size() << "book transfers, the workspace is closing";

    Transfers.clear();
    Handles.clear();

    emit transfersChanged();
}
