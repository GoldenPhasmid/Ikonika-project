#include "AppTranslator.h"

#include "core/FTLog.h"

namespace AppTranslator
{
    AppTranslatorNotifier& Notifier()
    {
        static AppTranslatorNotifier Instance;

        return Instance;
    }

    bool IsSupportedPair()
    {
        return false;
    }

    bool IsAvailable()
    {
        return false;
    }

    bool IsBusy()
    {
        return false;
    }

    void Translate(const QString& Text)
    {
        qCWarning(LogApp) << "Machine translation is not part of this build, dropping a request of"
                          << Text.size() << "characters";

        emit Notifier().translated(Text, QString());
    }

    void Reset()
    {
    }

    bool IsPairOffered()
    {
        return false;
    }

    bool IsDownloading()
    {
        return false;
    }

    void Download()
    {
        qCWarning(LogApp) << "Machine translation is not part of this build, ignoring a download request";
    }

    qint64 GetDownloadBytes()
    {
        return 0;
    }

    double GetDownloadProgress()
    {
        return 0.0;
    }

    QString GetAttribution()
    {
        return QString();
    }
}
