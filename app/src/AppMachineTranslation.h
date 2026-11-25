#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <string>
#include <utility>

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
    AppMachineTranslation(std::string InTargetLanguage, std::string InNativeLanguage, QObject* Parent = nullptr);

    Q_INVOKABLE void download();

    void SetPair(const std::string& NewTargetLanguage, const std::string& NewNativeLanguage);

    std::pair<std::string, std::string> GetPair() const;

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

private:
    void ConnectSources();

    std::string TargetLanguage;
    std::string NativeLanguage;
    bool bFollowsActivePair = true;
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
