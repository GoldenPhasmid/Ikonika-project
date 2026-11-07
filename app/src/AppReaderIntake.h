#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class AppReaderIntake : public QObject
{
    Q_OBJECT

public:
    static AppReaderIntake& Get();

    explicit AppReaderIntake(QObject* Parent = nullptr);
    ~AppReaderIntake() override;

    Q_INVOKABLE QVariantMap takePending();
    void OpenPath(const QString& Path);

public slots:
    void OpenUrl(const QUrl& Url);

signals:
    void bookArrived();

private:
    QVariantMap PendingBook;
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
