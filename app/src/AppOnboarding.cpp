#include "AppOnboarding.h"

#include "AppCatalog.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppLocalization.h"
#include "AppPackageDownloader.h"
#include "AppPaths.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QCoreApplication>

#include <algorithm>

namespace
{
    bool IsForced()
    {
        return QCoreApplication::arguments().contains(QStringLiteral("-onboarding"));
    }

    bool IsActiveLanguageSetUp()
    {
        if (!AppDatabase::Get().IsReady())
        {
            qCWarning(LogApp) << "Setup check has no database, treating the language as set up";

            return true;
        }

        const FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();
        const std::string& TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;

        return CardRepository.FindPlacementCutoff(TargetLanguage).has_value() || CardRepository.CountCards() > 0;
    }

    bool HasDictionaryFor(const std::string& TargetLanguage)
    {
        const std::vector<FTDictionaryPackage>& Packages = AppDictionary::Get().GetPackages();

        return AppDictionary::Get().IsOutdated(TargetLanguage)
            || std::any_of(Packages.begin(), Packages.end(),
                           [&TargetLanguage](const FTDictionaryPackage& Package)
                           {
                               return Package.TargetLanguage == TargetLanguage;
                           });
    }

    bool IsChosenLanguageMissing()
    {
        return FTSettings::Get().HasChosenLanguage()
            && !HasDictionaryFor(FTSettings::Get().GetLanguageRules().TargetLanguage);
    }

    bool IsSetupNeeded()
    {
        if (FTSettings::Get().HasChosenLanguage())
        {
            return IsChosenLanguageMissing();
        }

        if (AppDictionary::Get().GetPackages().empty())
        {
            return true;
        }

        return !AppPaths::IsHeadless() && !IsActiveLanguageSetUp();
    }
}

AppOnboarding& AppOnboarding::Get()
{
    static AppOnboarding Instance;

    return Instance;
}

AppOnboarding::AppOnboarding(QObject* Parent)
    : QObject(Parent)
{
    bRequired = IsForced() || IsSetupNeeded();

    qCInfo(LogApp) << "Setup screens" << (bRequired ? "needed" : "not needed") << ": forced" << IsForced()
                   << ", dictionaries" << AppDictionary::Get().GetPackages().size() << ", language"
                   << QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage);

    connect(&AppPackageDownloader::Get(), &AppPackageDownloader::packageInstalled, this,
            &AppOnboarding::OnPackageInstalled);

    ChooseChosenLanguage();
}

QString AppOnboarding::GetChosenLanguage() const
{
    return ChosenLanguage;
}

bool AppOnboarding::IsDictionaryReady() const
{
    return bDictionaryReady;
}

bool AppOnboarding::IsVoiceReady() const
{
    return bVoiceReady;
}

QString AppOnboarding::GetDictionaryId() const
{
    return ChosenDictionaryId;
}

QString AppOnboarding::GetVoiceId() const
{
    return ChosenVoiceId;
}

void AppOnboarding::choose(const QString& LanguageCode, const QString& NativeCode)
{
    if (LanguageCode.isEmpty())
    {
        return;
    }

    ChosenLanguage = LanguageCode;
    ChosenNative = NativeCode;
    ChosenDictionaryId.clear();
    ChosenVoiceId.clear();
    bDictionaryReady = false;
    bVoiceReady = false;

    for (const FTDictionaryPackage& Installed : AppDictionary::Get().GetPackages())
    {
        if (Installed.TargetLanguage == LanguageCode.toStdString())
        {
            bDictionaryReady = true;

            break;
        }
    }

    qCInfo(LogApp) << "Setup chose" << ChosenLanguage << ", dictionary already here"
                   << bDictionaryReady;

    emit chosenLanguageChanged();

    const FTCatalog* Catalog = AppCatalog::Get().GetCatalog();

    if (Catalog == nullptr)
    {
        return;
    }

    if (const FTCatalogPackage* Dictionary = Catalog->FindCompatibleDictionary(LanguageCode.toStdString()))
    {
        ChosenDictionaryId = QString::fromStdString(Dictionary->Id);

        if (!bDictionaryReady)
        {
            AppPackageDownloader::Get().download(ChosenDictionaryId);
        }
    }

    bVoiceReady = true;

#if defined(IKONIKA_DESKTOP)
    for (const FTCatalogPackage& Package : Catalog->Packages)
    {
        if (Package.Kind != CatalogKindVoice || !Package.IsCompatible())
        {
            continue;
        }

        if (!Package.ServesTarget(LanguageCode.toStdString()))
        {
            continue;
        }

        const QString VoiceId = QString::fromStdString(Package.Id);

        if (AppCatalog::GetInstalledVersion(VoiceId) > 0)
        {
            break;
        }

        ChosenVoiceId = VoiceId;
        bVoiceReady = false;

        AppPackageDownloader::Get().download(VoiceId);

        break;
    }
#endif
}

void AppOnboarding::retry()
{
    if (ChosenLanguage.isEmpty())
    {
        qCInfo(LogApp) << "Setup is reading the catalog again";

        AppCatalog::Get().Refresh();

        return;
    }

    if (AppPackageDownloader::Get().IsBusy())
    {
        return;
    }

    qCInfo(LogApp) << "Setup is asking for" << ChosenLanguage << "again";

    choose(ChosenLanguage, ChosenNative);
}

void AppOnboarding::OnPackageInstalled(const QString& Id)
{
    if (Id == ChosenDictionaryId && !bDictionaryReady)
    {
        bDictionaryReady = true;

        qCInfo(LogApp) << "Setup has a dictionary for" << ChosenLanguage;

        emit chosenLanguageChanged();

        return;
    }

    if (Id == ChosenVoiceId && !bVoiceReady)
    {
        bVoiceReady = true;

        qCInfo(LogApp) << "Setup has a voice for" << ChosenLanguage;

        emit chosenLanguageChanged();
    }
}

bool AppOnboarding::IsRequired() const
{
    return bRequired;
}

void AppOnboarding::finish()
{
    markLanguageSetUp(ChosenLanguage.isEmpty()
        ? QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage)
        : ChosenLanguage);

    if (!bRequired)
    {
        return;
    }

    bRequired = false;
    bResumingLanguage = false;

    qCInfo(LogApp) << "Setup finished";

    emit requiredChanged();
}

void AppOnboarding::refresh()
{
    ReevaluateRequired();
    ChooseChosenLanguage();
}

bool AppOnboarding::IsResumingLanguage() const
{
    return bResumingLanguage;
}

void AppOnboarding::ChooseChosenLanguage()
{
    if (!bRequired || !IsChosenLanguageMissing())
    {
        return;
    }

    const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();
    const QString Target = QString::fromStdString(Rules.TargetLanguage);

    if (bResumingLanguage && ChosenLanguage == Target)
    {
        return;
    }

    qCInfo(LogApp) << "Setup downloads the chosen language" << Target << ", skipping the language choice";

    bResumingLanguage = true;

    choose(Target, QString::fromStdString(Rules.NativeLanguage));
}

void AppOnboarding::ReevaluateRequired()
{
    const bool bNeeded = IsSetupNeeded();

    if (bNeeded == bRequired)
    {
        return;
    }

    if (!bNeeded && (IsForced() || !ChosenLanguage.isEmpty()))
    {
        return;
    }

    bRequired = bNeeded;

    if (bRequired)
    {
        ChosenLanguage.clear();
        ChosenNative.clear();
        ChosenDictionaryId.clear();
        ChosenVoiceId.clear();
        bDictionaryReady = false;
        bVoiceReady = false;
        bResumingLanguage = false;

        emit chosenLanguageChanged();
    }

    qCInfo(LogApp) << "Setup screens" << (bRequired ? "needed" : "not needed") << "after the user data changed";

    emit requiredChanged();
}

void AppOnboarding::markLanguageSetUp(const QString& LanguageCode)
{
    if (LanguageCode.isEmpty() || !AppDatabase::Get().IsReady())
    {
        return;
    }

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    if (CardRepository.FindPlacementCutoff(LanguageCode.toStdString()).has_value())
    {
        return;
    }

    if (!CardRepository.SetPlacementCutoff(LanguageCode.toStdString(), 0))
    {
        qCWarning(LogApp) << "Could not record that" << LanguageCode << "was set up";

        return;
    }

    qCInfo(LogApp) << "Recorded" << LanguageCode << "as set up without a placement result";
}

QString AppOnboarding::GetChosenNative() const
{
    return ChosenNative;
}
