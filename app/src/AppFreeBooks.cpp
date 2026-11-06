#include "AppFreeBooks.h"

#include "AppCatalog.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppFileTransfer.h"
#include "AppWorkspace.h"
#include "AppWorkspaceHost.h"

#include "core/FTLog.h"
#include "core/FTReaderRepository.h"
#include "core/FTSettings.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

#include <algorithm>
#include <utility>

namespace
{
    constexpr const char* BookCatalogUrlKey = "books/catalogUrl";

    constexpr int BookCatalogTimeoutMilliseconds = 15000;

    const QString FreeBookPrefix{QStringLiteral("free-")};

    AppFreeBooks* Instance = nullptr;

    std::string TargetLanguage()
    {
        return FTSettings::Get().GetLanguageRules().TargetLanguage;
    }

    bool MatchesPublishedFile(const FTCatalogBook& Book, const QString& Path)
    {
        QFile File(Path);
        QCryptographicHash Digest(QCryptographicHash::Sha256);

        if (!File.open(QIODevice::ReadOnly) || !Digest.addData(&File))
        {
            return false;
        }

        return QString::fromLatin1(Digest.result().toHex()) == QString::fromStdString(Book.Sha256);
    }
}

AppFreeBooks* AppFreeBooks::GetInstance()
{
    return Instance;
}

AppFreeBooks::AppFreeBooks(AppWorkspaceHost& Host, QObject* Parent)
    : QObject(Parent)
    , Host{&Host}
    , Network(std::make_unique<QNetworkAccessManager>())
{
    Instance = this;

    connect(&Host, &AppWorkspaceHost::aboutToSwitch, this, [this] { CancelTransfers(); });
    connect(&Host, &AppWorkspaceHost::switched, this, &AppFreeBooks::libraryChanged);
    connect(&Host, &AppWorkspaceHost::workspaceOpened, this, &AppFreeBooks::Refresh);
    connect(&AppCatalog::Get(), &AppCatalog::catalogChanged, this, &AppFreeBooks::Refresh);
    connect(&AppDictionary::Notifier(), &AppDictionaryNotifier::languagesChanged, this, &AppFreeBooks::Refresh);
}

AppFreeBooks::~AppFreeBooks()
{
    CancelTransfers();

    if (Instance == this)
    {
        Instance = nullptr;
    }
}

bool AppFreeBooks::IsAvailable() const
{
    return Host->GetActive() != nullptr && Catalog.has_value() && !Catalog->Books.empty();
}

const FTBookCatalog* AppFreeBooks::GetCatalog() const
{
    return Catalog ? &*Catalog : nullptr;
}

QVariantMap AppFreeBooks::GetTransfers() const
{
    QVariantMap Result;

    for (auto Iterator = Progress.cbegin(); Iterator != Progress.cend(); ++Iterator)
    {
        Result.insert(Iterator.key(), Iterator.value());
    }

    return Result;
}

QString AppFreeBooks::GetBooksDirectory() const
{
    if (Host->GetActive() == nullptr)
    {
        return {};
    }

    return QDir(Host->GetActive()->GetFolder().GetBooksDirectory()).absolutePath();
}

QString AppFreeBooks::GetBookPath(const FTCatalogBook& Book) const
{
    const QString Directory{GetBooksDirectory()};

    if (Directory.isEmpty())
    {
        return {};
    }

    const QString Extension{QFileInfo(QString::fromStdString(Book.FileName)).suffix().toLower()};

    return QDir(Directory).filePath(FreeBookPrefix + QString::fromStdString(Book.Id) + QLatin1Char('.') + Extension);
}

bool AppFreeBooks::OwnsFile(const QString& FilePath) const
{
    const QString Directory{GetBooksDirectory()};

    if (Directory.isEmpty())
    {
        return false;
    }

    const QFileInfo File(FilePath);

    return File.absolutePath() == Directory && File.fileName().startsWith(FreeBookPrefix);
}

QUrl AppFreeBooks::ResolveCatalogUrl() const
{
    const QString Target{QString::fromStdString(TargetLanguage())};
    const QSettings Settings;
    const QString Override{Settings.value(QLatin1String(BookCatalogUrlKey),
                                          FTSettings::GetShippedDefault(BookCatalogUrlKey, QString())).toString()};

    if (!Override.isEmpty())
    {
        return QUrl(QString(Override).replace(QStringLiteral("{target}"), Target));
    }

    const FTCatalog* Published{AppCatalog::Get().GetCatalog()};

    if (Published == nullptr)
    {
        return {};
    }

    const auto Found{Published->BookCatalogUrls.find(Target.toStdString())};

    return Found == Published->BookCatalogUrls.end() ? QUrl() : QUrl(QString::fromStdString(Found->second));
}

void AppFreeBooks::Refresh()
{
    if (Reply != nullptr)
    {
        return;
    }

    const QUrl Url{ResolveCatalogUrl()};

    if (!Url.isValid() || Url.isEmpty())
    {
        if (Catalog.has_value())
        {
            Catalog.reset();

            emit catalogChanged();
        }

        return;
    }

    QNetworkRequest Request(Url);
    Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    Request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    Request.setTransferTimeout(BookCatalogTimeoutMilliseconds);

    qCInfo(LogUpdate) << "Reading the book catalog from" << Url.toString();

    Reply = Network->get(Request);

    connect(Reply, &QNetworkReply::finished, this, [this, Target = TargetLanguage()]()
    {
        const QByteArray Body = Reply->readAll();
        const QNetworkReply::NetworkError Error = Reply->error();
        const QString ErrorText = Reply->errorString();

        Reply->deleteLater();
        Reply = nullptr;

        if (Error != QNetworkReply::NoError)
        {
            qCWarning(LogUpdate) << "Book catalog:" << ErrorText;

            return;
        }

        std::optional<FTBookCatalog> Parsed{ParseBookCatalog(Body.toStdString())};

        if (!Parsed.has_value())
        {
            return;
        }

        if (Parsed->TargetLanguage != Target)
        {
            qCWarning(LogUpdate) << "Ignoring a book catalog for" << QString::fromStdString(Parsed->TargetLanguage)
                                 << "fetched for" << QString::fromStdString(Target);

            return;
        }

        Catalog = std::move(Parsed);

        emit catalogChanged();
    });
}

void AppFreeBooks::AddToLibrary(const FTCatalogBook& Book, const QString& Path)
{
    const QString Title{QString::fromStdString(Book.Title)};
    const std::optional<std::string> DocumentId{AppDatabase::Get().GetReaderLibrary().AddDocument(
        Path.toStdString(), Book.Title, DocumentFormatName(Book.GetFormat()))};

    if (!DocumentId.has_value())
    {
        qCWarning(LogUpdate) << "Could not add the free book" << Title << "to the library";

        QFile::remove(Path);

        emit failed(tr("Could not add %1 to your books").arg(Title));

        return;
    }

    qCInfo(LogUpdate) << "Added the free book" << Title << "to the library";

    emit libraryChanged();
}

void AppFreeBooks::Download(const QString& Id)
{
    if (!Catalog.has_value() || Transfers.contains(Id) || !AppDatabase::Get().IsReady())
    {
        return;
    }

    const FTCatalogBook* Found{Catalog->FindBook(Id.toStdString())};

    if (Found == nullptr)
    {
        qCWarning(LogUpdate) << "Cannot download the free book" << Id << ": it is not in the book catalog";

        return;
    }

    const FTCatalogBook Book{*Found};
    const QString Path{GetBookPath(Book)};

    if (Path.isEmpty())
    {
        qCWarning(LogUpdate) << "Cannot download the free book" << Id << ": no workspace is open";

        return;
    }

    if (QFileInfo::exists(Path))
    {
        if (MatchesPublishedFile(Book, Path))
        {
            qCInfo(LogUpdate) << "The free book" << Id << "is already on disk, adding it without a download";

            AddToLibrary(Book, Path);

            return;
        }

        qCInfo(LogUpdate) << "The free book" << Id << "on disk is not the published one, downloading it again";

        QFile::remove(Path);
    }

    AppFileTransfer* Transfer = new AppFileTransfer(this);
    Transfers.insert(Id, Transfer);
    Progress.insert(Id, 0.0);

    emit transfersChanged();

    connect(Transfer, &AppFileTransfer::progress, this, [this, Id, Expected = Book.Bytes](qint64 Received, qint64 Total)
    {
        const qint64 Size{Total > 0 ? Total : Expected};

        if (Size <= 0)
        {
            return;
        }

        Progress.insert(Id, std::clamp(double(Received) / double(Size), 0.0, 1.0));

        emit transfersChanged();
    });

    connect(Transfer, &AppFileTransfer::finished, this, [this, Id, Book](const QString& FilePath)
    {
        FinishTransfer(Id);
        AddToLibrary(Book, FilePath);
    });

    connect(Transfer, &AppFileTransfer::failed, this, [this, Id, Title = QString::fromStdString(Book.Title)](const QString& Message)
    {
        qCWarning(LogUpdate) << "Downloading the free book" << Title << "failed:" << Message;

        FinishTransfer(Id);

        emit failed(tr("Could not download %1").arg(Title));
    });

    qCInfo(LogUpdate) << "Downloading the free book" << QString::fromStdString(Book.Title);

    Transfer->Start(QUrl(QString::fromStdString(Book.Url)), Path, QString::fromStdString(Book.Sha256), Book.Bytes);
}

void AppFreeBooks::Cancel(const QString& Id)
{
    AppFileTransfer* Transfer{Transfers.value(Id, nullptr)};

    if (Transfer == nullptr)
    {
        return;
    }

    Transfer->Cancel();
    FinishTransfer(Id);

    qCInfo(LogUpdate) << "Cancelled downloading the free book" << Id;
}

void AppFreeBooks::FinishTransfer(const QString& Id)
{
    if (AppFileTransfer* Transfer = Transfers.take(Id))
    {
        Transfer->deleteLater();
    }

    Progress.remove(Id);

    emit transfersChanged();
}

void AppFreeBooks::CancelTransfers()
{
    if (Transfers.isEmpty())
    {
        return;
    }

    qCInfo(LogUpdate) << "Cancelled" << Transfers.size() << "free book downloads, the workspace is closing";

    for (AppFileTransfer* Transfer : std::as_const(Transfers))
    {
        Transfer->Cancel();
        Transfer->deleteLater();
    }

    Transfers.clear();
    Progress.clear();

    emit transfersChanged();
}
