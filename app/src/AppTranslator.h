#pragma once

#include <QObject>
#include <QString>

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
    
    void Translate(const QString& Text);

    void Reset();

    bool IsPairOffered();

    bool IsDownloading();

    void Download();

    qint64 GetDownloadBytes();

    double GetDownloadProgress();

    QString GetAttribution();
}
