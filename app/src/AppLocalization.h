#pragma once

#include <QJSEngine>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;

class AppLocalization : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int languageIndex READ GetLanguageIndex WRITE SetLanguageIndex NOTIFY languageChanged)
    Q_PROPERTY(QStringList languageNames READ GetLanguageNames CONSTANT)
    Q_PROPERTY(QString localeName READ GetLocaleName NOTIFY languageChanged)

public:
    explicit AppLocalization(QObject* Parent = nullptr);

    void ApplySavedLanguage();

    /** Engine to retranslate on language change. Set before QML first touches this singleton. */
    void SetEngine(QQmlEngine* Engine);

    static AppLocalization& Get();

    int GetLanguageIndex() const;
    void SetLanguageIndex(int NewLanguage);

    QStringList GetLanguageNames() const;
    QString GetLocaleName() const;

signals:
    void languageChanged();

private:
    int CurrentLanguage = 0;
};

QString LanguageDisplayName(const std::string& Code);

struct AppLocalizationSingleton
{
    Q_GADGET
    QML_FOREIGN(AppLocalization)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppLocalization)

public:
    static AppLocalization* create(QQmlEngine* Engine, QJSEngine*)
    {
        AppLocalization& Instance = AppLocalization::Get();

        Instance.SetEngine(Engine);

        // Owned by Get()'s static, so QML must not delete it.
        QJSEngine::setObjectOwnership(&Instance, QJSEngine::CppOwnership);

        return &Instance;
    }
};
