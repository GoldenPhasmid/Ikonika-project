#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QtQml/qqmlregistration.h>

class AppPaneWidths : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int revision READ GetRevision NOTIFY changed)
    Q_PROPERTY(int minimumWidth READ GetMinimumWidth CONSTANT)
    Q_PROPERTY(int maximumWidth READ GetMaximumWidth CONSTANT)

public:
    static AppPaneWidths& Get();

    Q_INVOKABLE int width(const QString& Key, int DefaultWidth) const;
    Q_INVOKABLE void setWidth(const QString& Key, int Width);

    int GetRevision() const;
    int GetMinimumWidth() const;
    int GetMaximumWidth() const;

signals:
    void changed();

private:
    AppPaneWidths() = default;

    int Revision = 0;
};

struct AppPaneWidthsSingleton
{
    Q_GADGET
    QML_FOREIGN(AppPaneWidths)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppPaneWidths)

public:
    static AppPaneWidths* create(QQmlEngine* Engine, QJSEngine*)
    {
        Q_UNUSED(Engine);

        AppPaneWidths& Instance = AppPaneWidths::Get();

        QJSEngine::setObjectOwnership(&Instance, QJSEngine::CppOwnership);

        return &Instance;
    }
};
