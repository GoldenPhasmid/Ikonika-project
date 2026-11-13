#include "AppOnboarding.h"

#include "AppCatalog.h"
#include "AppCoreCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppLocalization.h"
#include "AppPackageDownloader.h"
#include "AppPaths.h"
#include "controllers/AppSyncController.h"
#include "controllers/PlacementTestController.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QCoreApplication>
#include <QLocale>

#include <algorithm>
#include <utility>

namespace
{
    const QString StepSpeak{QStringLiteral("speak")};
    const QString StepSignIn{QStringLiteral("signin")};
    const QString StepLearn{QStringLiteral("learn")};
    const QString StepDaily{QStringLiteral("daily")};
    const QString StepPlacement{QStringLiteral("placement")};
    const QString StepDownload{QStringLiteral("download")};

    bool IsSignInOffered()
    {
        const AppSyncController* SyncController = AppSyncController::GetInstance();

        return SyncController != nullptr && SyncController->IsConfigured();
    }

    QString DefaultSpokenLanguage()
    {
        const AppLocalization& Localization = AppLocalization::Get();

        if (Localization.GetLanguageIndex() != 0)
        {
            return Localization.GetLanguageCode();
        }

        const QString SystemLanguage = QLocale::system().name().section(QLatin1Char('_'), 0, 0);

        return Localization.FindLanguageIndex(SystemLanguage) >= 0 ? SystemLanguage : Localization.GetLanguageCode();
    }

    const QString TodayTourSeenKey = QStringLiteral("onboarding/todayTourSeen");

    bool IsForced()
    {
        return QCoreApplication::arguments().contains(QStringLiteral("-onboarding"));
    }

    bool IsTodayTourForced()
    {
        return IsForced() || QCoreApplication::arguments().contains(QStringLiteral("-tour"));
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

    bool IsChosenLanguageMissing()
    {
        const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

        return FTSettings::Get().HasChosenLanguage()
            && !AppDictionary::Get().IsOutdated(Rules.TargetLanguage, Rules.NativeLanguage)
            && !AppDictionary::Get().IsInstalled(Rules.TargetLanguage, Rules.NativeLanguage);
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
    bTodayTourPending = IsTodayTourForced()
        || (!AppPaths::IsHeadless() && !FTSettings::Get().OpenStore().value(TodayTourSeenKey, false).toBool());

    qCInfo(LogApp) << "Setup screens" << (bRequired ? "needed" : "not needed") << ": forced" << IsForced()
                   << ", dictionaries" << AppDictionary::Get().GetPackages().size() << ", language"
                   << QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage);

    connect(&AppPackageDownloader::Get(), &AppPackageDownloader::packageInstalled, this,
            &AppOnboarding::OnPackageInstalled);

    connect(&AppLocalization::Get(), &AppLocalization::languageChanged, this,
            &AppOnboarding::spokenLanguageChanged);

    connect(this, &AppOnboarding::spokenLanguageChanged, this, &AppOnboarding::learnableLanguagesChanged);

    connect(&AppCatalog::Get(), &AppCatalog::languagesChanged, this, &AppOnboarding::learnableLanguagesChanged);

    CurrentStep = StepSpeak;

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
    ChosenNative = NativeCode.isEmpty()
        ? QString::fromStdString(FTSettings::Get().GetLanguageRules().NativeLanguage)
        : NativeCode;

    RequestPackages();
}

void AppOnboarding::RequestPackages()
{
    ChosenDictionaryId.clear();
    ChosenVoiceId.clear();
    bDictionaryReady = false;
    bVoiceReady = false;

    bDictionaryReady = AppDictionary::Get().IsInstalled(ChosenLanguage.toStdString(), ChosenNative.toStdString());

    qCInfo(LogApp) << "Setup chose" << ChosenLanguage << "for" << ChosenNative << ", dictionary already here"
                   << bDictionaryReady;

    emit chosenLanguageChanged();

    const FTCatalog* Catalog = AppCatalog::Get().GetCatalog();

    if (Catalog == nullptr)
    {
        return;
    }

    if (const FTCatalogPackage* Dictionary =
            Catalog->FindCompatibleDictionary(ChosenLanguage.toStdString(), ChosenNative.toStdString()))
    {
        ChosenDictionaryId = QString::fromStdString(Dictionary->Id);

        if (!bDictionaryReady)
        {
            AppPackageDownloader::Get().download(ChosenDictionaryId);
        }
    }

    bVoiceReady = true;

    const QString VoiceId{AppCatalog::Get().FindVoicePackage(ChosenLanguage.toStdString())};

    if (VoiceId.isEmpty() || AppCatalog::GetInstalledVersion(VoiceId) > 0)
    {
        return;
    }

    ChosenVoiceId = VoiceId;
    bVoiceReady = false;

    AppPackageDownloader::Get().download(VoiceId);
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

    RequestPackages();
}

void AppOnboarding::chooseLearning(const QString& LanguageCode)
{
    const QString NativeCode = GetSpokenLanguage();

    if (LanguageCode == ChosenLanguage && NativeCode == ChosenNative)
    {
        qCInfo(LogApp) << "Setup already chose" << ChosenLanguage << "for" << ChosenNative;

        return;
    }

    ForgetPendingPackages();
    choose(LanguageCode, NativeCode);
}

void AppOnboarding::ForgetPendingPackages()
{
    if (!ChosenDictionaryId.isEmpty() && !bDictionaryReady)
    {
        qCInfo(LogApp) << "Setup drops the dictionary it no longer needs" << ChosenDictionaryId;

        AppPackageDownloader::Get().forget(ChosenDictionaryId);
    }

    if (!ChosenVoiceId.isEmpty() && !bVoiceReady)
    {
        qCInfo(LogApp) << "Setup drops the voice it no longer needs" << ChosenVoiceId;

        AppPackageDownloader::Get().forget(ChosenVoiceId);
    }
}

void AppOnboarding::selectSpokenLanguage(const QString& LanguageCode)
{
    if (AppLocalization::Get().FindLanguageIndex(LanguageCode) < 0)
    {
        qCWarning(LogApp) << "Setup cannot speak" << LanguageCode << ", the app has no interface in it";

        return;
    }

    if (LanguageCode == SpokenLanguage)
    {
        return;
    }

    SpokenLanguage = LanguageCode;

    qCInfo(LogApp) << "Setup speaks" << SpokenLanguage;

    ApplySpokenLanguage();

    emit spokenLanguageChanged();
}

void AppOnboarding::ApplySpokenLanguage()
{
    const int Index = AppLocalization::Get().FindLanguageIndex(GetSpokenLanguage());

    if (Index < 0)
    {
        return;
    }

    AppLocalization::Get().SetLanguageIndex(Index);
}

void AppOnboarding::next()
{
    if (CurrentStep == StepSpeak)
    {
        SpokenLanguage = GetSpokenLanguage();
        ApplySpokenLanguage();
    }

    if (CurrentStep == StepLearn)
    {
        if (ChosenLanguage.isEmpty())
        {
            qCWarning(LogApp) << "Setup cannot leave the language choice before a language is chosen";

            return;
        }

        bPlacementOffered = IsForced() ? PlacementTestController::HasWordListFor(ChosenLanguage, ChosenNative)
                                       : PlacementTestController::IsOfferedFor(ChosenLanguage, ChosenNative);
    }

    const QStringList Steps = GetSteps();
    const qsizetype Index = Steps.indexOf(CurrentStep);

    if (Index < 0 || Index + 1 >= Steps.size())
    {
        qCWarning(LogApp) << "Setup has no step after" << CurrentStep;

        return;
    }

    CurrentStep = Steps[Index + 1];

    qCInfo(LogApp) << "Setup moved on to" << CurrentStep;

    emit stepChanged();
}

void AppOnboarding::back()
{
    if (!CanGoBack())
    {
        qCWarning(LogApp) << "Setup cannot go back from" << GetStep();

        return;
    }

    const QStringList Steps = GetSteps();

    CurrentStep = Steps[Steps.indexOf(CurrentStep) - 1];

    qCInfo(LogApp) << "Setup went back to" << CurrentStep;

    emit stepChanged();
}

void AppOnboarding::RestartSteps()
{
    CurrentStep = StepSpeak;
    bPlacementOffered = true;

    emit stepChanged();
}

QString AppOnboarding::GetSpokenLanguage() const
{
    return SpokenLanguage.isEmpty() ? DefaultSpokenLanguage() : SpokenLanguage;
}

QVariantList AppOnboarding::GetLearnableLanguages() const
{
    const QString Spoken = GetSpokenLanguage();
    QVariantList Result;

    for (const QVariant& Item : AppCatalog::Get().GetLanguages())
    {
        QVariantMap Language = Item.toMap();

        if (Language.value(QStringLiteral("code")).toString() == Spoken)
        {
            continue;
        }

        const QVariantList Natives = Language.value(QStringLiteral("natives")).toList();

        const auto SpokenNative = std::find_if(Natives.begin(), Natives.end(),
            [&Spoken](const QVariant& Native)
            {
                return Native.toMap().value(QStringLiteral("code")).toString() == Spoken;
            });

        if (!Natives.isEmpty() && SpokenNative == Natives.end())
        {
            continue;
        }

        if (SpokenNative != Natives.end())
        {
            const QVariantMap Pair = SpokenNative->toMap();

            for (const QString& Key : {QStringLiteral("id"), QStringLiteral("bytes"), QStringLiteral("compatible"),
                                       QStringLiteral("installed"), QStringLiteral("outdated")})
            {
                Language[Key] = Pair.value(Key);
            }
        }

        QString Detail;

        if (!Language.value(QStringLiteral("compatible")).toBool())
        {
            Detail = tr("Needs a newer app version");
        }
        else if (Language.value(QStringLiteral("installed")).toBool())
        {
            Detail = tr("Downloaded");
        }
        else
        {
            Detail = tr("%1 MB").arg(qRound(Language.value(QStringLiteral("bytes")).toDouble() / (1024 * 1024)));
        }

        Language[QStringLiteral("detail")] = Detail;
        Result.append(Language);
    }

    return Result;
}

QString AppOnboarding::GetStep() const
{
    return bResumingLanguage ? StepDownload : CurrentStep;
}

QStringList AppOnboarding::GetSteps() const
{
    if (bResumingLanguage)
    {
        return {StepDownload};
    }

    QStringList Steps{StepSpeak};

    if (IsSignInOffered())
    {
        Steps.append(StepSignIn);
    }

    Steps.append(StepLearn);
    Steps.append(StepDaily);

    if (bPlacementOffered)
    {
        Steps.append(StepPlacement);
    }

    Steps.append(StepDownload);

    return Steps;
}

bool AppOnboarding::CanGoBack() const
{
    return !bResumingLanguage && CurrentStep != StepDownload && GetSteps().indexOf(CurrentStep) > 0;
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

void AppOnboarding::RequestCoreCards(const QString& TargetLanguage)
{
    if (bRequired)
    {
        PendingCoreCardsLanguage = TargetLanguage;

        qCInfo(LogApp) << "Core cards for" << TargetLanguage << "wait for the end of setup";

        return;
    }

    AppCoreCards::Seed(TargetLanguage);
}

void AppOnboarding::finish()
{
    if (!PendingCoreCardsLanguage.isEmpty())
    {
        AppCoreCards::Seed(std::exchange(PendingCoreCardsLanguage, QString()));
    }

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

bool AppOnboarding::IsTodayTourPending() const
{
    return bTodayTourPending;
}

void AppOnboarding::finishTodayTour()
{
    if (!bTodayTourPending)
    {
        return;
    }

    bTodayTourPending = false;
    FTSettings::Get().OpenStore().setValue(TodayTourSeenKey, true);

    qCInfo(LogApp) << "Today tour finished";

    emit todayTourChanged();
}

void AppOnboarding::replayTodayTour()
{
    if (bTodayTourPending)
    {
        return;
    }

    bTodayTourPending = true;
    FTSettings::Get().OpenStore().setValue(TodayTourSeenKey, false);

    qCInfo(LogApp) << "Today tour requested again";

    emit todayTourChanged();
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

    emit stepChanged();

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

        RestartSteps();
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
