#pragma once

#include <QObject>
#include <QString>

#include <string>

class AppTranslatorNotifier : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

signals:
    /** The model finished loading, successfully or not; ask IsAvailable which. */
    void readyChanged();

    /** A translation finished. Empty text means the model could not produce one. */
    void translated(const QString& Source, const QString& Text);

    /** The sentences translated so far of a text still being translated. */
    void translationProgress(const QString& Source, const QString& Text);

    void modelsChanged();

    void downloadingChanged();
};

namespace AppTranslator
{
    inline bool IsEnabled()
    {
#ifdef IKONIKA_ENABLE_MACHINE_TRANSLATION
        return true;
#else
        return false;
#endif
    }

    AppTranslatorNotifier& Notifier();
    
    bool IsSupportedPair();
    
    bool IsAvailable();
    
    bool IsBusy();
    
    void Translate(const QString& Text, quintptr Owner);

    void Reset();

    bool IsPairOffered();

    bool IsDownloading();

    void Download();

    qint64 GetDownloadBytes();

    double GetDownloadProgress();

    bool IsSupportedPair(const std::string& TargetLanguage, const std::string& NativeLanguage);

    void Translate(const QString& Text, quintptr Owner, const std::string& TargetLanguage,
                   const std::string& NativeLanguage);

    bool IsPairOffered(const std::string& TargetLanguage, const std::string& NativeLanguage);

    bool IsDownloading(const std::string& TargetLanguage, const std::string& NativeLanguage);

    void Download(const std::string& TargetLanguage, const std::string& NativeLanguage);

    qint64 GetDownloadBytes(const std::string& TargetLanguage, const std::string& NativeLanguage);

    double GetDownloadProgress(const std::string& TargetLanguage, const std::string& NativeLanguage);

    QString GetAttribution();
}
