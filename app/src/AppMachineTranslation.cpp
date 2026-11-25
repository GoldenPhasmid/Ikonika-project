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
    ConnectSources();
}

AppMachineTranslation::AppMachineTranslation(std::string InTargetLanguage, std::string InNativeLanguage,
                                             QObject* Parent)
    : QObject(Parent)
    , TargetLanguage(std::move(InTargetLanguage))
    , NativeLanguage(std::move(InNativeLanguage))
    , bFollowsActivePair(false)
{
    ConnectSources();
}

void AppMachineTranslation::ConnectSources()
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

void AppMachineTranslation::SetPair(const std::string& NewTargetLanguage, const std::string& NewNativeLanguage)
{
    if (!bFollowsActivePair && TargetLanguage == NewTargetLanguage && NativeLanguage == NewNativeLanguage)
    {
        return;
    }

    bFollowsActivePair = false;
    TargetLanguage = NewTargetLanguage;
    NativeLanguage = NewNativeLanguage;

    qCInfo(LogTranslate) << "Machine translation pair is now"
                         << QString::fromStdString(TargetLanguage) << "to" << QString::fromStdString(NativeLanguage);

    emit changed();
}

std::pair<std::string, std::string> AppMachineTranslation::GetPair() const
{
    if (bFollowsActivePair)
    {
        const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

        return {Rules.TargetLanguage, Rules.NativeLanguage};
    }

    return {TargetLanguage, NativeLanguage};
}

void AppMachineTranslation::download()
{
    if (!IsEnabled())
    {
        qCWarning(LogTranslate) << "Machine translation is disabled in this build, ignoring the download request";

        return;
    }

    const auto [Target, Native] = GetPair();

    qCInfo(LogTranslate) << "Learner asked to download the translation model for"
                         << QString::fromStdString(Target) << "to" << QString::fromStdString(Native);

    AppTranslator::Download(Target, Native);
}

bool AppMachineTranslation::IsEnabled() const
{
    return AppTranslator::IsEnabled();
}

bool AppMachineTranslation::IsInstalled() const
{
    const auto [Target, Native] = GetPair();

    return IsEnabled() && AppTranslator::IsSupportedPair(Target, Native);
}

bool AppMachineTranslation::IsOffered() const
{
    const auto [Target, Native] = GetPair();

    return IsEnabled() && !IsInstalled() && AppTranslator::IsPairOffered(Target, Native);
}

bool AppMachineTranslation::IsDownloading() const
{
    const auto [Target, Native] = GetPair();

    return IsEnabled() && AppTranslator::IsDownloading(Target, Native);
}

double AppMachineTranslation::GetProgress() const
{
    const auto [Target, Native] = GetPair();

    return IsEnabled() ? AppTranslator::GetDownloadProgress(Target, Native) : 0.0;
}

qint64 AppMachineTranslation::GetBytes() const
{
    const auto [Target, Native] = GetPair();

    return IsEnabled() ? AppTranslator::GetDownloadBytes(Target, Native) : 0;
}

QString AppMachineTranslation::GetLanguageName() const
{
    return LanguageDisplayName(GetPair().first);
}

QString AppMachineTranslation::GetPairLabel() const
{
    const auto [Target, Native] = GetPair();

    return QStringLiteral("%1 → %2")
        .arg(QString::fromStdString(Target).toUpper(),
             QString::fromStdString(Native).toUpper());
}

QString AppMachineTranslation::GetAttribution() const
{
    return AppTranslator::GetAttribution();
}
