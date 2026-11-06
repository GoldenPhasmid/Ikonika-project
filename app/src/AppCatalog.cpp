#include "AppCatalog.h"

#include "AppDictionary.h"
#include "AppPackageDownloader.h"
#include "AppPaths.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>

#include "AppLocalization.h"

namespace
{
    constexpr const char* CatalogUrlKey = "update/catalogUrl";

    constexpr const char* DefaultCatalogUrl =
        "https://raw.githubusercontent.com/GoldenPhasmid/ikonika-releases/main/catalog.json";

    constexpr int CatalogTimeoutMilliseconds = 15000;

    constexpr int RefreshCooldownMilliseconds = 4 * 60 * 60 * 1000;

    QVariantList NativeLanguageList(const std::vector<std::string>& Codes)
    {
        QVariantList Result;

        for (const std::string& Code : Codes)
        {
            QVariantMap Native;
            Native[QStringLiteral("code")] = QString::fromStdString(Code);
            Native[QStringLiteral("name")] = LanguageDisplayName(Code);

            Result.append(Native);
        }

        return Result;
    }

    QString InstalledVersionKey(const QString& Id)
    {
        return QStringLiteral("packages/%1/version").arg(Id);
    }

    QString DismissedVersionKey(const QString& Id)
    {
        return QStringLiteral("packages/%1/dismissedVersion").arg(Id);
    }

    constexpr const char* DismissedAppVersionKey = "update/dismissedAppVersion";

    QUrl ReadCatalogUrl()
    {
        const QSettings Settings;

        return QUrl(Settings.value(QLatin1String(CatalogUrlKey),
            FTSettings::GetShippedDefault(CatalogUrlKey, QLatin1String(DefaultCatalogUrl)))
                .toString());
    }
}

AppCatalog& AppCatalog::Get()
{
    static AppCatalog Instance;

    return Instance;
}

AppCatalog::AppCatalog(QObject* Parent)
    : QObject(Parent)
    , Network(std::make_unique<QNetworkAccessManager>())
{
    connect(&AppPackageDownloader::Get(), &AppPackageDownloader::packageInstalled, this,
            &AppCatalog::catalogChanged);
    connect(this, &AppCatalog::catalogChanged, this, &AppCatalog::dictionaryUpdateChanged);
    connect(this, &AppCatalog::catalogChanged, this, &AppCatalog::languagesChanged);
    connect(&AppDictionary::Notifier(), &AppDictionaryNotifier::languagesChanged, this,
            &AppCatalog::dictionaryUpdateChanged);
    connect(&AppLocalization::Get(), &AppLocalization::languageChanged, this,
            &AppCatalog::languagesChanged);
    connect(&AppLocalization::Get(), &AppLocalization::languageChanged, this,
            &AppCatalog::dictionaryUpdateChanged);

    RefreshCooldownTimer = new QTimer(this);
    RefreshCooldownTimer->setSingleShot(true);
    RefreshCooldownTimer->setInterval(RefreshCooldownMilliseconds);
    RefreshCooldownTimer->setObjectName(QStringLiteral("CatalogRefreshCooldownTimer"));

    WatchApplicationState();
}

void AppCatalog::WatchApplicationState()
{
    const QGuiApplication* Application{
        qobject_cast<QGuiApplication*>(QCoreApplication::instance())};

    if (Application == nullptr)
    {
        return;
    }

    connect(Application, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState State)
            {
                if (State == Qt::ApplicationActive)
                {
                    RefreshWhenStale();
                }
            });
}

AppCatalog::~AppCatalog() = default;

bool AppCatalog::IsBusy() const
{
    return bBusy;
}

bool AppCatalog::IsLoaded() const
{
    return Catalog.has_value();
}

QString AppCatalog::GetLastError() const
{
    return LastError;
}

const FTCatalog* AppCatalog::GetCatalog() const
{
    return Catalog ? &*Catalog : nullptr;
}

void AppCatalog::RefreshWhenStale()
{
    if (RefreshCooldownTimer->isActive())
    {
        return;
    }

    qCInfo(LogUpdate) << "The catalog was last read over four hours ago, reading it again";

    Refresh();
}

void AppCatalog::Refresh()
{
    if (bBusy)
    {
        return;
    }

    RefreshCooldownTimer->start();

    const QUrl Url = ReadCatalogUrl();

    if (!Url.isValid())
    {
        Fail(tr("The catalog address is not a valid URL."));

        return;
    }

    QNetworkRequest Request(Url);
    Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    Request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);

    Request.setTransferTimeout(CatalogTimeoutMilliseconds);

    qCInfo(LogUpdate) << "Reading the catalog from" << Url.toString();

    SetBusy(true);

    Reply = Network->get(Request);

    connect(Reply, &QNetworkReply::finished, this, [this]()
    {
        const QByteArray Body = Reply->readAll();
        const QNetworkReply::NetworkError Error = Reply->error();
        const QString ErrorText = Reply->errorString();

        Reply->deleteLater();
        Reply = nullptr;

        SetBusy(false);

        if (Error != QNetworkReply::NoError)
        {
            Fail(ErrorText);

            return;
        }

        std::optional<FTCatalog> Parsed = ParseCatalog(Body.toStdString());

        if (!Parsed.has_value())
        {
            Fail(tr("The catalog could not be read."));

            return;
        }

        Catalog = std::move(Parsed);
        LastError.clear();

        emit lastErrorChanged();
        emit catalogChanged();
    });
}

QString AppCatalog::ResolveDestination(const FTCatalogPackage& Package)
{
    const QDir Root(Package.Kind == CatalogKindDictionary
                        ? AppPaths::InstalledDictionariesDirectory()
                        : AppPaths::DownloadedModelsDirectory());

    const QString Relative = QString::fromStdString(Package.UnpackTo);

    if (Relative.isEmpty() || Relative == QStringLiteral("."))
    {
        return Root.absolutePath();
    }

    if (QDir::isAbsolutePath(Relative))
    {
        qCWarning(LogUpdate) << "Package" << QString::fromStdString(Package.Id)
                             << "names an absolute destination. Refusing it.";

        return {};
    }

    const QString Absolute = QDir::cleanPath(Root.filePath(Relative));

    if (!Absolute.startsWith(Root.absolutePath()))
    {
        qCWarning(LogUpdate) << "Package" << QString::fromStdString(Package.Id)
                             << "unpacks outside the download root. Refusing it.";

        return {};
    }

    return Absolute;
}

int AppCatalog::GetInstalledVersion(const QString& Id)
{
    const QSettings Settings;

    return Settings.value(InstalledVersionKey(Id), 0).toInt();
}

void AppCatalog::SetInstalledVersion(const QString& Id, int Version)
{
    QSettings Settings;
    Settings.setValue(InstalledVersionKey(Id), Version);
}

std::optional<FTCatalogPackage> AppCatalog::FindPackage(const QString& Id) const
{
    if (!Catalog)
    {
        return std::nullopt;
    }

    const std::string Wanted = Id.toStdString();

    for (const FTCatalogPackage& Package : Catalog->Packages)
    {
        if (Package.Id == Wanted)
        {
            return Package;
        }
    }

    return std::nullopt;
}

namespace
{
    bool IsLanguageInstalled(const std::string& Code)
    {
        for (const FTDictionaryPackage& Package : AppDictionary::Get().GetPackages())
        {
            if (Package.TargetLanguage == Code)
            {
                return true;
            }
        }

        return false;
    }
}

QVariantList AppCatalog::GetLanguages() const
{
    QVariantList Result;
    QStringList Seen;

    const auto AddEntry = [&Result, &Seen](const std::string& Id, const std::string& TargetLanguage,
                                           std::int64_t Bytes, bool bPublishedCompatible,
                                           const std::vector<std::string>& Natives)
    {
        const QString Code = QString::fromStdString(TargetLanguage);

        if (Seen.contains(Code))
        {
            return;
        }

        Seen.append(Code);

        const bool bInstalled = IsLanguageInstalled(TargetLanguage);

        QVariantMap Entry;
        Entry[QStringLiteral("id")] = QString::fromStdString(Id);
        Entry[QStringLiteral("code")] = Code;
        Entry[QStringLiteral("name")] = LanguageDisplayName(TargetLanguage);
        Entry[QStringLiteral("bytes")] = static_cast<qlonglong>(Bytes);
        Entry[QStringLiteral("compatible")] = bInstalled || bPublishedCompatible;
        Entry[QStringLiteral("installed")] = bInstalled;
        Entry[QStringLiteral("outdated")] = AppDictionary::Get().IsOutdated(TargetLanguage);
        Entry[QStringLiteral("natives")] = NativeLanguageList(Natives);

        Result.append(Entry);
    };

    if (Catalog.has_value())
    {
        for (const FTCatalogPackage& Package : Catalog->Packages)
        {
            if (Package.Kind != CatalogKindDictionary || Package.TargetLanguage.empty())
            {
                continue;
            }

            const FTCatalogPackage* Compatible = Catalog->FindCompatibleDictionary(Package.TargetLanguage);
            const FTCatalogPackage& Listed = Compatible != nullptr ? *Compatible : Package;

            AddEntry(Listed.Id, Listed.TargetLanguage, Listed.Bytes, Listed.IsCompatible(),
                     Listed.NativeLanguages);
        }
    }

    for (const FTDictionaryPackage& Package : AppDictionary::Get().GetPackages())
    {
        AddEntry({}, Package.TargetLanguage, 0, false, Package.NativeLanguages);
    }

    for (const FTDictionaryPackage& Package : AppDictionary::Get().GetOutdatedPackages())
    {
        AddEntry({}, Package.TargetLanguage, 0, false, Package.NativeLanguages);
    }

    return Result;
}

QString AppCatalog::describePackage(const QString& Id) const
{
    const std::optional<FTCatalogPackage> Package = FindPackage(Id);

    if (!Package.has_value())
    {
        return Id;
    }

    const QString Language = Package->TargetLanguage.empty()
                                 ? QString()
                                 : LanguageDisplayName(Package->TargetLanguage);

    if (Package->Kind == CatalogKindDictionary)
    {
        return Language.isEmpty() ? tr("Dictionary") : tr("%1 dictionary").arg(Language);
    }

    if (Package->Kind == CatalogKindVoice)
    {
        return Language.isEmpty() ? tr("Voice") : tr("%1 voice").arg(Language);
    }

    if (Package->Kind == CatalogKindTranslation)
    {
        return Language.isEmpty() ? tr("Translation") : tr("%1 translation").arg(Language);
    }

    return Id;
}

QVariantList AppCatalog::GetPackages() const
{
    QVariantList Result;

    if (!Catalog)
    {
        return Result;
    }

    const QSettings Settings;

    for (const FTCatalogPackage& Package : Catalog->Packages)
    {
        const QString Id = QString::fromStdString(Package.Id);
        const int Installed = Settings.value(InstalledVersionKey(Id), 0).toInt();

        QVariantMap Entry;
        Entry[QStringLiteral("id")] = Id;
        Entry[QStringLiteral("kind")] = QString::fromStdString(Package.Kind);
        Entry[QStringLiteral("target")] = QString::fromStdString(Package.TargetLanguage);
        Entry[QStringLiteral("version")] = Package.Version;
        Entry[QStringLiteral("installedVersion")] = Installed;
        Entry[QStringLiteral("installed")] = Installed > 0;
        Entry[QStringLiteral("outdated")] = Installed > 0 && Installed < Package.Version;
        Entry[QStringLiteral("bytes")] = static_cast<qlonglong>(Package.Bytes);
        Entry[QStringLiteral("unpackedBytes")] = static_cast<qlonglong>(Package.UnpackedBytes);
        Entry[QStringLiteral("compatible")] = Package.IsCompatible();
        Entry[QStringLiteral("desktopOnly")] = Package.bDesktopOnly;

        QVariantList Natives;

        for (const std::string& Native : Package.NativeLanguages)
        {
            Natives.append(QString::fromStdString(Native));
        }

        Entry[QStringLiteral("natives")] = Natives;

        Result.append(Entry);
    }

    return Result;
}

QVariantMap AppCatalog::GetDictionaryUpdate() const
{
    const QVariantMap Update = GetAvailableDictionaryUpdate();

    if (Update.value(QStringLiteral("dismissed")).toBool())
    {
        return {};
    }

    return Update;
}

QVariantMap AppCatalog::GetAvailableDictionaryUpdate() const
{
    if (!Catalog)
    {
        return {};
    }

    const FTDictionaryPackage* Active = AppDictionary::Get().GetActivePackage();

    const std::string TargetLanguage = Active != nullptr
        ? Active->TargetLanguage
        : FTSettings::Get().GetLanguageRules().TargetLanguage;

    const bool bOutdated = Active == nullptr && AppDictionary::Get().IsOutdated(TargetLanguage);

    if (Active == nullptr && !bOutdated)
    {
        return {};
    }

    const FTCatalogPackage* Package = Catalog->FindCompatibleDictionary(TargetLanguage);

    if (Package == nullptr)
    {
        return {};
    }

    const QString Id = QString::fromStdString(Package->Id);
    const QSettings Settings;
    const int Installed = Settings.value(InstalledVersionKey(Id), 0).toInt();

    if ((!bOutdated && Installed <= 0) || Package->Version <= Installed)
    {
        return {};
    }

    QVariantMap Update;
    Update[QStringLiteral("dismissed")] = Settings.value(DismissedVersionKey(Id), 0).toInt() >= Package->Version;
    Update[QStringLiteral("id")] = Id;
    Update[QStringLiteral("name")] = LanguageDisplayName(Package->TargetLanguage);
    Update[QStringLiteral("version")] = Package->Version;
    Update[QStringLiteral("bytes")] = static_cast<qlonglong>(Package->Bytes);

    return Update;
}

std::optional<FTCatalogPackage> AppCatalog::FindTranslationPackage(const std::string& TargetLanguage,
                                                                   const std::string& NativeLanguage) const
{
    if (!Catalog)
    {
        return std::nullopt;
    }

    for (const FTCatalogPackage& Package : Catalog->Packages)
    {
        if (Package.Kind == CatalogKindTranslation && Package.IsCompatible()
            && Package.TargetLanguage == TargetLanguage && Package.SupportsNative(NativeLanguage))
        {
            return Package;
        }
    }

    return std::nullopt;
}

QVariantList AppCatalog::GetPackageUpdates() const
{
    QVariantList Result;

    if (!Catalog)
    {
        return Result;
    }

    const QSettings Settings;

    for (const FTCatalogPackage& Package : Catalog->Packages)
    {
        if (!Package.IsCompatible())
        {
            continue;
        }

        if (Package.Kind == CatalogKindDictionary
            && Catalog->FindCompatibleDictionary(Package.TargetLanguage) != &Package)
        {
            continue;
        }

        const QString Id{QString::fromStdString(Package.Id)};
        const int Installed{GetInstalledVersion(Id)};
        const bool bOutdatedDictionary{Package.Kind == CatalogKindDictionary
            && AppDictionary::Get().IsOutdated(Package.TargetLanguage)};

        if ((!bOutdatedDictionary && Installed <= 0) || Package.Version <= Installed)
        {
            continue;
        }

        QVariantMap Update;
        Update[QStringLiteral("id")] = Id;
        Update[QStringLiteral("name")] = describePackage(Id);
        Update[QStringLiteral("version")] = Package.Version;
        Update[QStringLiteral("bytes")] = static_cast<qlonglong>(Package.Bytes);
        Update[QStringLiteral("dismissed")] = Settings.value(DismissedVersionKey(Id), 0).toInt() >= Package.Version;
        Result.append(Update);
    }

    return Result;
}

void AppCatalog::dismissPackageUpdate(const QString& Id)
{
    const std::optional<FTCatalogPackage> Package{FindPackage(Id)};

    if (!Package.has_value())
    {
        return;
    }

    QSettings().setValue(DismissedVersionKey(Id), Package->Version);
    qCInfo(LogUpdate) << "Learner passed on" << Id << "version" << Package->Version;
    emit catalogChanged();
}

void AppCatalog::dismissDictionaryUpdate()
{
    const QVariantMap Update = GetDictionaryUpdate();

    if (Update.isEmpty())
    {
        return;
    }

    const QString Id = Update.value(QStringLiteral("id")).toString();
    dismissPackageUpdate(Id);
}

void AppCatalog::dismissAppUpdate()
{
    const QString Version = GetPublishedVersion();

    if (Version.isEmpty())
    {
        return;
    }

    QSettings Settings;
    Settings.setValue(QLatin1String(DismissedAppVersionKey), Version);

    qCInfo(LogUpdate) << "Learner passed on application version" << Version;

    emit catalogChanged();
}

bool AppCatalog::IsAppUpdateDismissed() const
{
    const QString Version = GetPublishedVersion();

    if (Version.isEmpty())
    {
        return false;
    }

    const QSettings Settings;

    return Settings.value(QLatin1String(DismissedAppVersionKey)).toString() == Version;
}

bool AppCatalog::IsUpdateAvailable() const
{
    if (AppPaths::IsStorePackage())
    {
        return false;
    }

    if (!Catalog || !Catalog->App.has_value())
    {
        return false;
    }

    return IsNewerVersion(QCoreApplication::applicationVersion().toStdString(),
                          Catalog->App->Version);
}

QString AppCatalog::GetPublishedVersion() const
{
    if (!Catalog || !Catalog->App.has_value())
    {
        return {};
    }

    return QString::fromStdString(Catalog->App->Version);
}

void AppCatalog::Fail(const QString& Message)
{
    qCWarning(LogUpdate) << "Catalog:" << Message;

    LastError = Message;

    emit lastErrorChanged();
}

void AppCatalog::SetBusy(bool bValue)
{
    if (bBusy == bValue)
    {
        return;
    }

    bBusy = bValue;

    emit busyChanged();
}
