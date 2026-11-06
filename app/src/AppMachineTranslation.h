#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class AppMachineTranslation : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ IsEnabled CONSTANT)
    Q_PROPERTY(bool installed READ IsInstalled NOTIFY changed)
    Q_PROPERTY(bool offered READ IsOffered NOTIFY changed)
    Q_PROPERTY(bool downloading READ IsDownloading NOTIFY changed)
    Q_PROPERTY(double progress READ GetProgress NOTIFY changed)
    Q_PROPERTY(qint64 bytes READ GetBytes NOTIFY changed)
    Q_PROPERTY(QString languageName READ GetLanguageName NOTIFY changed)
    Q_PROPERTY(QString pairLabel READ GetPairLabel NOTIFY changed)
    Q_PROPERTY(QString attribution READ GetAttribution CONSTANT)

public:
    static AppMachineTranslation& Get();

    explicit AppMachineTranslation(QObject* Parent = nullptr);

    Q_INVOKABLE void download();

    bool IsEnabled() const;
    bool IsInstalled() const;
    bool IsOffered() const;
    bool IsDownloading() const;
    double GetProgress() const;
    qint64 GetBytes() const;
    QString GetLanguageName() const;
    QString GetPairLabel() const;
    QString GetAttribution() const;

signals:
    void changed();
};

struct AppMachineTranslationSingleton
{
    Q_GADGET
    QML_FOREIGN(AppMachineTranslation)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppMachineTranslation)

public:
    static AppMachineTranslation* create(QQmlEngine* Engine, QJSEngine*)
    {
        AppMachineTranslation& Instance = AppMachineTranslation::Get();

        QQmlEngine::setObjectOwnership(&Instance, QQmlEngine::CppOwnership);
        Q_UNUSED(Engine);

        return &Instance;
    }
};
