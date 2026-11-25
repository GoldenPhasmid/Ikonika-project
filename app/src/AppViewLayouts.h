#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class AppViewLayouts : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int revision READ GetRevision NOTIFY changed)

public:
    static AppViewLayouts& Get();

    Q_INVOKABLE bool isList(const QString& Key) const;
    Q_INVOKABLE void setList(const QString& Key, bool bList);

    Q_INVOKABLE QString sortOrder(const QString& Key, const QString& Default) const;
    Q_INVOKABLE bool sortReversed(const QString& Key) const;
    Q_INVOKABLE void setSort(const QString& Key, const QString& Order, bool bReversed);

    Q_INVOKABLE QStringList navigationOrder() const;
    Q_INVOKABLE void setNavigationOrder(const QStringList& Order);

    int GetRevision() const;

signals:
    void changed();

private:
    AppViewLayouts() = default;

    int Revision = 0;
};

struct AppViewLayoutsSingleton
{
    Q_GADGET
    QML_FOREIGN(AppViewLayouts)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppViewLayouts)

public:
    static AppViewLayouts* create(QQmlEngine* Engine, QJSEngine*)
    {
        Q_UNUSED(Engine);

        AppViewLayouts& Instance = AppViewLayouts::Get();

        QJSEngine::setObjectOwnership(&Instance, QJSEngine::CppOwnership);

        return &Instance;
    }
};
