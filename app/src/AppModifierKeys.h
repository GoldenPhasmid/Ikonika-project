#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

class AppModifierKeys : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool controlHeld READ IsControlHeld NOTIFY controlHeldChanged)

public:
    static AppModifierKeys& Get();

    explicit AppModifierKeys(QObject* Parent = nullptr);

    bool IsControlHeld() const;

signals:
    void controlHeldChanged();

protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;

private:
    void SetControlHeld(bool bValue);

    bool bControlHeld = false;
};

struct AppModifierKeysSingleton
{
    Q_GADGET
    QML_FOREIGN(AppModifierKeys)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppModifierKeys)

public:
    static AppModifierKeys* create(QQmlEngine* Engine, QJSEngine*)
    {
        Q_UNUSED(Engine);

        AppModifierKeys& Instance = AppModifierKeys::Get();

        QJSEngine::setObjectOwnership(&Instance, QJSEngine::CppOwnership);

        return &Instance;
    }
};
