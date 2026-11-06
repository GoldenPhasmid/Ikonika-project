#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <memory>

class AppBookStore;
struct AppBookTransfer;
class AppWorkspaceHost;
struct FTReaderDocumentRow;

class AppBookCloud : public QObject
{
    Q_OBJECT

public:
    static AppBookCloud* GetInstance();

    AppBookCloud(AppWorkspaceHost& Host, AppBookStore* Store, QObject* Parent = nullptr);
    ~AppBookCloud() override;

    bool IsAvailable() const;
    QVariantMap GetTransfers() const;

    void Upload(const QString& DocumentId);
    void Download(const QString& CloudId);
    void RemoveFromCloud(const QString& CloudId);
    void Cancel(const QString& Key);

    void ForgetLocalCopy(const FTReaderDocumentRow& Row) const;
    void ForgetDeletedBooks() const;

signals:
    void transfersChanged();
    void libraryChanged();
    void availabilityChanged();
    void failed(const QString& Message);

private:
    void SetProgress(const QString& Key, double Progress);
    void FinishTransfer(const QString& Key);
    void CancelTransfers();

    AppWorkspaceHost* Host = nullptr;
    AppBookStore* Store = nullptr;
    QHash<QString, double> Transfers;
    QHash<QString, std::shared_ptr<AppBookTransfer>> Handles;
};
