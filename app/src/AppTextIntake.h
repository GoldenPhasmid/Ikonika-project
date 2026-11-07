#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

class AppTextIntake : public QObject
{
    Q_OBJECT

public:
    static AppTextIntake& Get();

    explicit AppTextIntake(QObject* Parent = nullptr);

    void ReceiveText(const QString& Text);
    Q_INVOKABLE QString takePending();

signals:
    void textArrived();

private:
    void ReceiveAndroidText();

    QString PendingText;
};

struct AppTextIntakeSingleton
{
    Q_GADGET
    QML_FOREIGN(AppTextIntake)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppTextIntake)

public:
    static AppTextIntake* create(QQmlEngine*, QJSEngine*)
    {
        AppTextIntake& Instance = AppTextIntake::Get();
        QJSEngine::setObjectOwnership(&Instance, QJSEngine::CppOwnership);
        return &Instance;
    }
};
