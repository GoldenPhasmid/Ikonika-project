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
    Q_PROPERTY(bool bookOpen READ IsBookOpen WRITE SetBookOpen NOTIFY bookOpenChanged)

public:
    static AppReaderIntake& Get();

    explicit AppReaderIntake(QObject* Parent = nullptr);

    bool IsBookOpen() const;
    void SetBookOpen(bool bValue);

    Q_INVOKABLE QVariantMap takePending();
    void OpenPath(const QString& Path);
    void OpenUrl(const QUrl& Url);

protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;

signals:
    void bookArrived();
    void bookOpenChanged();

private:
    QVariantMap PendingBook;
    bool bBookOpen = false;
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
