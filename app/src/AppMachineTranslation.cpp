#include "AppMachineTranslation.h"

#include "AppCatalog.h"
#include "AppDictionary.h"
#include "AppLocalization.h"
#include "AppPackageDownloader.h"
#include "AppTranslator.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

AppMachineTranslation& AppMachineTranslation::Get()
{
    static AppMachineTranslation Instance;

    return Instance;
}

AppMachineTranslation::AppMachineTranslation(QObject* Parent)
    : QObject(Parent)
{
    connect(&AppCatalog::Get(), &AppCatalog::catalogChanged, this, &AppMachineTranslation::changed);
    connect(&AppDictionary::Notifier(), &AppDictionaryNotifier::languagesChanged, this,
            &AppMachineTranslation::changed);
    connect(&AppTranslator::Notifier(), &AppTranslatorNotifier::modelsChanged, this,
            &AppMachineTranslation::changed);
    connect(&AppTranslator::Notifier(), &AppTranslatorNotifier::downloadingChanged, this,
            &AppMachineTranslation::changed);
    connect(&AppPackageDownloader::Get(), &AppPackageDownloader::busyChanged, this,
            &AppMachineTranslation::changed);
    connect(&AppPackageDownloader::Get(), &AppPackageDownloader::queueChanged, this,
            &AppMachineTranslation::changed);
    connect(&AppPackageDownloader::Get(), &AppPackageDownloader::progressChanged, this,
            &AppMachineTranslation::changed);
}

void AppMachineTranslation::download()
{
    if (!IsEnabled())
    {
        qCWarning(LogTranslate) << "Machine translation is disabled in this build, ignoring the download request";

        return;
    }

    qCInfo(LogTranslate) << "Learner asked to download the translation model for"
                         << GetLanguageName();

    AppTranslator::Download();
}

bool AppMachineTranslation::IsEnabled() const
{
    return AppTranslator::IsEnabled();
}

bool AppMachineTranslation::IsInstalled() const
{
    return IsEnabled() && AppTranslator::IsSupportedPair();
}

bool AppMachineTranslation::IsOffered() const
{
    return IsEnabled() && !IsInstalled() && AppTranslator::IsPairOffered();
}

bool AppMachineTranslation::IsDownloading() const
{
    return IsEnabled() && AppTranslator::IsDownloading();
}

double AppMachineTranslation::GetProgress() const
{
    return IsEnabled() ? AppTranslator::GetDownloadProgress() : 0.0;
}

qint64 AppMachineTranslation::GetBytes() const
{
    return IsEnabled() ? AppTranslator::GetDownloadBytes() : 0;
}

QString AppMachineTranslation::GetLanguageName() const
{
    return LanguageDisplayName(FTSettings::Get().GetLanguageRules().TargetLanguage);
}

QString AppMachineTranslation::GetPairLabel() const
{
    const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

    return QStringLiteral("%1 → %2")
        .arg(QString::fromStdString(Rules.TargetLanguage).toUpper(),
             QString::fromStdString(Rules.NativeLanguage).toUpper());
}

QString AppMachineTranslation::GetAttribution() const
{
    return AppTranslator::GetAttribution();
}
