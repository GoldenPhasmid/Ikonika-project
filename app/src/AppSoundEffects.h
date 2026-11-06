#pragma once

#include <QHash>
#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <cstdint>
#include <memory>
#include <vector>

class FTAudioPlayer;

class AppSoundEffects : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ IsEnabled WRITE SetEnabled NOTIFY enabledChanged)

public:
    static AppSoundEffects& Get();

    explicit AppSoundEffects(QObject* Parent = nullptr);
    ~AppSoundEffects() override;

    void Prewarm();

    bool IsEnabled() const;
    void SetEnabled(bool bValue);

    void PlayIncorrect();
    void PlayComplete();

    Q_INVOKABLE void celebrateDailyGoal();

signals:
    void enabledChanged();

private:
    bool ClaimDailyGoal();
    void Play(const QString& Name);

    std::unique_ptr<FTAudioPlayer> Player;
    QHash<QString, std::vector<int16_t>> Sounds;
};

struct AppSoundEffectsSingleton
{
    Q_GADGET
    QML_FOREIGN(AppSoundEffects)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppSoundEffects)

public:
    static AppSoundEffects* create(QQmlEngine* Engine, QJSEngine*)
    {
        Q_UNUSED(Engine);

        AppSoundEffects& Instance = AppSoundEffects::Get();

        QJSEngine::setObjectOwnership(&Instance, QJSEngine::CppOwnership);

        return &Instance;
    }
};
