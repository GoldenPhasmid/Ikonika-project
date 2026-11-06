#pragma once

#include <QHash>
#include <QString>

class AppCredentialStore
{
public:
    virtual ~AppCredentialStore() = default;

    virtual bool Write(const QString& AccountId, const QString& Secret) = 0;
    virtual QString Read(const QString& AccountId) const = 0;
    virtual void Remove(const QString& AccountId) = 0;
};

#if defined(Q_OS_WIN)
class AppWindowsCredentialStore : public AppCredentialStore
{
public:
    explicit AppWindowsCredentialStore(QString Prefix);

    bool Write(const QString& AccountId, const QString& Secret) override;
    QString Read(const QString& AccountId) const override;
    void Remove(const QString& AccountId) override;

private:
    QString Prefix;
};
#endif

class AppMemoryCredentialStore : public AppCredentialStore
{
public:
    bool Write(const QString& AccountId, const QString& Secret) override;
    QString Read(const QString& AccountId) const override;
    void Remove(const QString& AccountId) override;

private:
    QHash<QString, QString> Secrets;
};
