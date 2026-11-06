#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class AppOnboarding : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool required READ IsRequired NOTIFY requiredChanged)
    Q_PROPERTY(QString chosenLanguage READ GetChosenLanguage NOTIFY chosenLanguageChanged)
    Q_PROPERTY(QString chosenNative READ GetChosenNative NOTIFY chosenLanguageChanged)
    Q_PROPERTY(bool dictionaryReady READ IsDictionaryReady NOTIFY chosenLanguageChanged)
    Q_PROPERTY(bool voiceReady READ IsVoiceReady NOTIFY chosenLanguageChanged)
    Q_PROPERTY(QString dictionaryId READ GetDictionaryId NOTIFY chosenLanguageChanged)
    Q_PROPERTY(QString voiceId READ GetVoiceId NOTIFY chosenLanguageChanged)
    Q_PROPERTY(bool resumingLanguage READ IsResumingLanguage NOTIFY chosenLanguageChanged)

public:
    static AppOnboarding& Get();

    explicit AppOnboarding(QObject* Parent = nullptr);

    /** Queues whatever a language still needs: its dictionary, then the voice that covers it. */
    Q_INVOKABLE void choose(const QString& LanguageCode, const QString& NativeCode = QString());

    /**
     * Asks again for whatever is missing: the catalog when nothing has been chosen yet, otherwise
     * the packages the choice needs. What a machine with no network on its first launch is waiting
     * for.
     */
    Q_INVOKABLE void retry();

    Q_INVOKABLE void finish();

    Q_INVOKABLE void refresh();

    Q_INVOKABLE void markLanguageSetUp(const QString& LanguageCode);

    bool IsRequired() const;

    /** The code being set up, empty until something is chosen. */
    QString GetChosenLanguage() const;

    /** The language translations are shown in, empty when the current one should stay. */
    QString GetChosenNative() const;

    bool IsDictionaryReady() const;

    bool IsVoiceReady() const;

    QString GetDictionaryId() const;

    QString GetVoiceId() const;

    bool IsResumingLanguage() const;

signals:
    void requiredChanged();
    void chosenLanguageChanged();

private:
    void OnPackageInstalled(const QString& Id);
    void ReevaluateRequired();
    void ChooseChosenLanguage();

    QString ChosenLanguage;
    QString ChosenNative;
    QString ChosenDictionaryId;
    QString ChosenVoiceId;

    bool bRequired = false;
    bool bDictionaryReady = false;
    bool bVoiceReady = false;
    bool bResumingLanguage = false;
};

struct AppOnboardingSingleton
{
    Q_GADGET
    QML_FOREIGN(AppOnboarding)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppOnboarding)

public:
    static AppOnboarding* create(QQmlEngine* Engine, QJSEngine*)
    {
        AppOnboarding& Instance = AppOnboarding::Get();

        QQmlEngine::setObjectOwnership(&Instance, QQmlEngine::CppOwnership);
        Q_UNUSED(Engine);

        return &Instance;
    }
};
