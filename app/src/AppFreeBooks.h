#pragma once

#include "core/FTBookCatalog.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <memory>
#include <optional>

class AppFileTransfer;
class AppWorkspaceHost;
class QNetworkAccessManager;
class QNetworkReply;

class AppFreeBooks : public QObject
{
    Q_OBJECT

public:
    static AppFreeBooks* GetInstance();

    explicit AppFreeBooks(AppWorkspaceHost& Host, QObject* Parent = nullptr);
    ~AppFreeBooks() override;

    bool IsAvailable() const;
    const FTBookCatalog* GetCatalog() const;
    QVariantMap GetTransfers() const;

    QString GetBookPath(const FTCatalogBook& Book) const;
    bool OwnsFile(const QString& FilePath) const;
    QString GetBooksDirectory() const;

    void Refresh();
    void Download(const QString& Id);
    void Cancel(const QString& Id);

signals:
    void catalogChanged();
    void transfersChanged();
    void libraryChanged();
    void failed(const QString& Message);

private:
    QUrl ResolveCatalogUrl() const;
    void AddToLibrary(const FTCatalogBook& Book, const QString& Path);
    void FinishTransfer(const QString& Id);
    void CancelTransfers();

    AppWorkspaceHost* Host = nullptr;
    std::unique_ptr<QNetworkAccessManager> Network;
    QNetworkReply* Reply = nullptr;
    std::optional<FTBookCatalog> Catalog;
    QHash<QString, AppFileTransfer*> Transfers;
    QHash<QString, double> Progress;
};
