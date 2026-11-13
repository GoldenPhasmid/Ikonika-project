#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

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
    Q_PROPERTY(QString spokenLanguage READ GetSpokenLanguage NOTIFY spokenLanguageChanged)
    Q_PROPERTY(QVariantList learnableLanguages READ GetLearnableLanguages NOTIFY learnableLanguagesChanged)
    Q_PROPERTY(QString step READ GetStep NOTIFY stepChanged)
    Q_PROPERTY(QStringList steps READ GetSteps NOTIFY stepChanged)
    Q_PROPERTY(bool canGoBack READ CanGoBack NOTIFY stepChanged)
    Q_PROPERTY(bool todayTourPending READ IsTodayTourPending NOTIFY todayTourChanged)

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

    Q_INVOKABLE void selectSpokenLanguage(const QString& LanguageCode);
    Q_INVOKABLE void chooseLearning(const QString& LanguageCode);

    Q_INVOKABLE void next();
    Q_INVOKABLE void back();

    Q_INVOKABLE void finishTodayTour();
    Q_INVOKABLE void replayTodayTour();

    bool IsRequired() const;

    void RequestCoreCards(const QString& TargetLanguage);

    /** The code being set up, empty until something is chosen. */
    QString GetChosenLanguage() const;

    /** The language translations are shown in, empty when the current one should stay. */
    QString GetChosenNative() const;

    bool IsDictionaryReady() const;

    bool IsVoiceReady() const;

    QString GetDictionaryId() const;

    QString GetVoiceId() const;

    bool IsResumingLanguage() const;

    QString GetSpokenLanguage() const;
    QVariantList GetLearnableLanguages() const;
    QString GetStep() const;
    QStringList GetSteps() const;
    bool CanGoBack() const;
    bool IsTodayTourPending() const;

signals:
    void requiredChanged();
    void chosenLanguageChanged();
    void spokenLanguageChanged();
    void learnableLanguagesChanged();
    void stepChanged();
    void todayTourChanged();

private:
    void OnPackageInstalled(const QString& Id);
    void ReevaluateRequired();
    void ChooseChosenLanguage();
    void RequestPackages();
    void ForgetPendingPackages();
    void ApplySpokenLanguage();
    void RestartSteps();

    QString ChosenLanguage;
    QString ChosenNative;
    QString ChosenDictionaryId;
    QString ChosenVoiceId;
    QString SpokenLanguage;
    QString CurrentStep;
    QString PendingCoreCardsLanguage;

    bool bPlacementOffered = true;

    bool bRequired = false;
    bool bDictionaryReady = false;
    bool bVoiceReady = false;
    bool bResumingLanguage = false;
    bool bTodayTourPending = false;
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
