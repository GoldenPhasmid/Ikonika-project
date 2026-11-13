#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class DictionaryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList results READ GetResults NOTIFY resultsChanged)
    Q_PROPERTY(bool nativeSearch READ IsNativeSearch WRITE SetNativeSearch NOTIFY nativeSearchChanged)
    Q_PROPERTY(QString targetLanguageCode READ GetTargetLanguageCode NOTIFY languagesChanged)
    Q_PROPERTY(QString nativeLanguageCode READ GetNativeLanguageCode NOTIFY languagesChanged)

    Q_PROPERTY(QVariantMap entry READ GetEntry NOTIFY entryChanged)
    Q_PROPERTY(QVariantList allTranslations READ GetAllTranslations NOTIFY entryChanged)
    Q_PROPERTY(QVariantList readings READ GetReadings NOTIFY entryChanged)
    Q_PROPERTY(QVariantList cards READ GetCards NOTIFY entryChanged)
    Q_PROPERTY(QVariantList partsOfSpeech READ GetPartsOfSpeech NOTIFY entryChanged)

    Q_PROPERTY(QString machineTranslation READ GetMachineTranslation NOTIFY translationChanged)
    Q_PROPERTY(bool translating READ IsTranslating NOTIFY translationChanged)
    Q_PROPERTY(QString formTerm READ GetFormTerm NOTIFY translationChanged)
    Q_PROPERTY(QString formReading READ GetFormReading NOTIFY translationChanged)

public:
    explicit DictionaryController(QObject* Parent = nullptr);

    static constexpr int ResultLimit = 50;

    Q_INVOKABLE void search(const QString& Text);
    Q_INVOKABLE void recordSearch();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void translate(const QString& Text);

    Q_INVOKABLE void openEntry(const QString& Headword, const QString& PartOfSpeech, const QString& SearchText = {});

    Q_INVOKABLE void speak(const QString& Word) const;
    Q_INVOKABLE void speakAutomatically(const QString& Word) const;
    Q_INVOKABLE void openWebSearch(const QString& Headword) const;
    Q_INVOKABLE void copyToClipboard(const QString& Text) const;
    Q_INVOKABLE QString cardMeaningGist(const QString& CardId) const;
    Q_INVOKABLE QString cardHint(const QString& CardId) const;
    Q_INVOKABLE QString sentenceAt(const QString& Text, int Position) const;

    QVariantList GetResults() const;
    bool IsNativeSearch() const;
    void SetNativeSearch(bool bInNativeSearch);
    QString GetTargetLanguageCode() const;
    QString GetNativeLanguageCode() const;
    QVariantMap GetEntry() const;
    QVariantList GetAllTranslations() const;
    QVariantList GetReadings() const;
    QVariantList GetCards() const;
    QVariantList GetPartsOfSpeech() const;

    QString GetMachineTranslation() const;
    bool IsTranslating() const;
    QString GetFormTerm() const;
    QString GetFormReading() const;

signals:
    void resultsChanged();
    void nativeSearchChanged();
    void entryChanged();
    void languagesChanged();
    void translationChanged();

private:
    void ResetTranslation(const QString& Text);
    void RequestTranslation();

    QString FormTerm;
    QString FormReading;

    QString TranslationQuery;
    QString MachineTranslation;
    QString LastSearchText;
    bool bTranslating = false;
    bool bNativeSearch = false;

    QVariantList Results;
    QVariantMap Entry;
    QVariantList AllTranslations;
    QVariantList Readings;
    QVariantList Cards;
    QVariantList PartsOfSpeech;

    quint64 PendingGeneration = 0;
};
