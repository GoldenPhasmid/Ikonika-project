#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class AppReaderIntake : public QObject
{
    Q_OBJECT

public:
    static AppReaderIntake& Get();

    explicit AppReaderIntake(QObject* Parent = nullptr);

    Q_INVOKABLE QVariantMap takePending();

signals:
    void bookArrived();
};

struct AppReaderIntakeSingleton
{
    Q_GADGET
    QML_FOREIGN(AppReaderIntake)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppReaderIntake)

public:
    static AppReaderIntake* create(QQmlEngine* Engine, QJSEngine*)
    {
        Q_UNUSED(Engine);

        AppReaderIntake& Instance = AppReaderIntake::Get();

        QJSEngine::setObjectOwnership(&Instance, QJSEngine::CppOwnership);

        return &Instance;
    }
};
