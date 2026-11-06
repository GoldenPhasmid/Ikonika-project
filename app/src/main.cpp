#include "AppAccountRegistry.h"
#include "AppBookCloud.h"
#include "AppBookImageProvider.h"
#include "AppFreeBooks.h"
#include "AppCatalog.h"
#include "AppCloudStore.h"
#include "AppCredentialStore.h"
#include "AppDatabase.h"
#include "AppDatabaseService.h"
#include "AppDictionary.h"
#include "AppGoogleAccount.h"
#include "AppLocalization.h"
#include "AppLog.h"
#include "AppMetrics.h"
#include "AppOnboarding.h"
#include "AppPackageDownloader.h"
#include "AppPaths.h"
#include "AppReminder.h"
#include "AppSoundEffects.h"
#include "AppHeartbeat.h"
#include "AppSwitchCover.h"
#include "AppSpeech.h"
#include "AppStartup.h"
#include "AppUpdater.h"
#include "AppWorkspaceHost.h"
#include "controllers/AppSyncController.h"

#include "core/FTCard.h"
#include "core/FTLog.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInformation>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QPointer>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QStringList>
#include <memory>

#ifdef IKONIKA_ENABLE_WEBENGINE
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif

#if defined(Q_OS_ANDROID)
#include "AppPlatformSpeech.h"
#elif defined(IKONIKA_DESKTOP)
#include "AppSingleInstance.h"
#include "AppSpeechService.h"
#endif

#if defined(Q_OS_WIN)
#include "AppStartupTask.h"
#endif

#if defined(Q_OS_IOS)
#include "AppIosSpeech.h"
#include "AppIosStorage.h"
#endif

namespace
{
    void LoadBundledFonts()
    {
        const QStringList Faces{QStringLiteral("Regular"), QStringLiteral("Italic"), QStringLiteral("Medium"),
                                QStringLiteral("SemiBold"), QStringLiteral("Bold")};

        for (const QString& Face : Faces)
        {
            const QString Path = QStringLiteral(":/fonts/Inter-%1.ttf").arg(Face);

            if (QFontDatabase::addApplicationFont(Path) < 0)
            {
                qCWarning(LogApp) << "Could not load the bundled font" << Path;
            }
        }

        QFont Font = QGuiApplication::font();
        Font.setFamily(QStringLiteral("Inter"));
        QGuiApplication::setFont(Font);
    }

    const QUrl DriveRoot{QStringLiteral("https://www.googleapis.com")};
    constexpr int DriveTimeoutMilliseconds{120000};

    QString GetCredentialPrefix()
    {
#if defined(QT_DEBUG)
        return QStringLiteral("Ikonika-debug/google/");
#else
        return QStringLiteral("Ikonika/google/");
#endif
    }

    QString FindArgumentValue(const QStringList& Arguments, const QString& Prefix)
    {
        for (const QString& Argument : Arguments)
        {
            if (Argument.startsWith(Prefix))
            {
                return Argument.mid(Prefix.length());
            }
        }

        return QString();
    }

    bool CopyDatabaseIntoSandbox(const QString& SourceFile)
    {
        const QString TargetFile = AppAccountRegistry::MakeFolder(AppPaths::DataRoot(), QString()).GetDatabaseFilePath();
        const QString WriteAheadSuffix = QStringLiteral("-wal");

        QDir().mkpath(QFileInfo(TargetFile).absolutePath());

        if (!QFile::copy(SourceFile, TargetFile))
        {
            qCCritical(LogApp) << "-database-file could not be copied:" << SourceFile << "to" << TargetFile;

            return false;
        }

        if (QFile::exists(SourceFile + WriteAheadSuffix) && !QFile::copy(SourceFile + WriteAheadSuffix, TargetFile + WriteAheadSuffix))
        {
            qCCritical(LogApp) << "-database-file could not copy its write-ahead log:" << SourceFile + WriteAheadSuffix;

            return false;
        }

        qCInfo(LogApp) << "-database-file copied" << SourceFile << "into the signed-out workspace";

        return true;
    }

    std::optional<int> ParseCardGender(const QString& Code)
    {
        if (Code == QStringLiteral("m"))
        {
            return GenderMasculine;
        }

        if (Code == QStringLiteral("f"))
        {
            return GenderFeminine;
        }

        return std::nullopt;
    }

    void SeedCardsFromJson(FTCardRepository& CardRepository, const QJsonArray& CardArray)
    {
        int CardCount = 0;

        for (const QJsonValue& CardValue : CardArray)
        {
            const QJsonObject CardObject = CardValue.toObject();

            FTCard Card;
            Card.TargetWord = CardObject.value(QStringLiteral("word")).toString().toStdString();
            Card.Translation =
                CardObject.value(QStringLiteral("translation")).toString().toStdString();
            Card.Definition = CardObject.value(QStringLiteral("definition")).toString().toStdString();
            Card.PartOfSpeech =
                CardObject.value(QStringLiteral("partOfSpeech")).toString().toStdString();
            Card.Gender = ParseCardGender(CardObject.value(QStringLiteral("gender")).toString());

            if (Card.TargetWord.empty() || Card.Translation.empty())
            {
                qCWarning(LogApp) << "-database: skipping a card with no word or translation";

                continue;
            }

            if (!CardRepository.AddCard(Card).has_value())
            {
                qCWarning(LogApp) << "-database: could not add card"
                                  << QString::fromStdString(Card.TargetWord) << "-"
                                  << QString::fromStdString(CardRepository.GetLastError());

                continue;
            }

            ++CardCount;
        }

        qCInfo(LogApp) << "-database: seeded" << CardCount << "card(s) into My Vocabulary";
    }

    bool SeedDatabaseFromJson(const QString& RelativePath)
    {
        const QString FilePath = QDir(QCoreApplication::applicationDirPath()).filePath(RelativePath);

        QFile File(FilePath);

        if (!File.open(QIODevice::ReadOnly))
        {
            qCCritical(LogApp) << "-database file could not be opened:" << FilePath;

            return false;
        }

        QJsonParseError ParseError;
        const QJsonDocument Document = QJsonDocument::fromJson(File.readAll(), &ParseError);

        if (ParseError.error != QJsonParseError::NoError || !Document.isObject())
        {
            qCCritical(LogApp) << "-database file is not valid JSON:" << FilePath
                               << ParseError.errorString();

            return false;
        }

        if (!AppDatabase::Get().Initialise())
        {
            qCCritical(LogApp) << "-database could not open the headless database:"
                               << AppDatabase::Get().GetLastError();

            return false;
        }

        const QJsonObject Root = Document.object();
        const std::string TargetLanguage = Root.value(QStringLiteral("targetLanguage"))
            .toString(QStringLiteral("fr")).toStdString();
        const std::string NativeLanguage = Root.value(QStringLiteral("nativeLanguage"))
            .toString(QStringLiteral("en")).toStdString();

        FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();
        CardRepository.SetActiveLanguages(TargetLanguage, NativeLanguage);

        qCInfo(LogApp) << "-database: seeding from" << FilePath << "as"
                       << QString::fromStdString(TargetLanguage) << "->"
                       << QString::fromStdString(NativeLanguage);

        SeedCardsFromJson(CardRepository, Root.value(QStringLiteral("cards")).toArray());

        return true;
    }
}

int main(int ArgumentCount, char* Arguments[])
{
    AppStartup::Begin();

#ifdef IKONIKA_ENABLE_WEBENGINE
    QtWebEngineQuick::initialize();
#endif

    QApplication Application(ArgumentCount, Arguments);
    const auto CloseDictionary{qScopeGuard([] { AppDictionary::Get().Close(); })};

    if (Application.arguments().contains(QStringLiteral("-headless")))
    {
        AppPaths::EnableHeadlessSandbox(Application.arguments().contains(QStringLiteral("-keep-dictionaries")));
    }

    const QString DatabaseSeedFile = FindArgumentValue(Application.arguments(), QStringLiteral("-database="));
    const QString DatabaseCopyFile = FindArgumentValue(Application.arguments(), QStringLiteral("-database-file="));

    if ((!DatabaseSeedFile.isEmpty() || !DatabaseCopyFile.isEmpty()) && !AppPaths::IsHeadless())
    {
        qCCritical(LogApp) << "-database and -database-file require -headless";

        return -1;
    }

    if (!DatabaseCopyFile.isEmpty() && !CopyDatabaseIntoSandbox(DatabaseCopyFile))
    {
        return -1;
    }

    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/AppIcon.png")));

    QApplication::setQuitOnLastWindowClosed(false);

    QApplication::setOrganizationName(QStringLiteral("Ikonika"));
    QApplication::setApplicationName(QStringLiteral("Ikonika"));

    QApplication::setApplicationVersion(QStringLiteral(IKONIKA_VERSION));

    AppPaths::Install();
    AppLog::Install();

#if defined(Q_OS_IOS)
    for (const QString& Directory : {AppPaths::InstalledDictionariesDirectory(), AppPaths::DownloadedModelsDirectory(),
                                     AppPaths::DownloadScratchDirectory()})
    {
        AppIosStorage::ExcludeFromBackup(Directory);
    }
#endif

    AppMemoryCredentialStore MemoryCredentials;
#if defined(Q_OS_WIN)
    AppWindowsCredentialStore WindowsCredentials(GetCredentialPrefix());
    AppCredentialStore& Credentials{AppPaths::IsHeadless() ? static_cast<AppCredentialStore&>(MemoryCredentials)
                                                           : WindowsCredentials};
#else
    AppCredentialStore& Credentials{MemoryCredentials};
#endif

    AppWorkspaceHost WorkspaceHost(AppPaths::DataRoot(), Credentials);
    AppGoogleAccount GoogleAccount(WorkspaceHost.GetRegistry(), Credentials);
    AppDriveStore Drive(GoogleAccount, DriveRoot, DriveTimeoutMilliseconds);
    AppDriveStore BookDrive(GoogleAccount, DriveRoot, DriveTimeoutMilliseconds);
    const auto CloseWorkspace{qScopeGuard([&WorkspaceHost] { WorkspaceHost.Close(); })};

    if (GoogleAccount.IsConfigured())
    {
        WorkspaceHost.SetCloudStore(&Drive, AppSyncTimings{});
    }

    AppSyncController SyncController(WorkspaceHost, GoogleAccount);
    AppBookCloud BookCloud(WorkspaceHost, GoogleAccount.IsConfigured() ? &BookDrive : nullptr);
    AppFreeBooks FreeBooks(WorkspaceHost);

    AppDatabaseService Database(WorkspaceHost);
    AppDatabase::SetInstance(&Database);

    if (!Database.Initialise())
    {
        qCCritical(LogApp) << "Could not open the workspace:" << Database.GetLastError();
    }

#if defined(Q_OS_ANDROID)
    AppPlatformSpeech RealSpeechService;
    AppSpeech::SetInstance(&RealSpeechService);
#elif defined(IKONIKA_DESKTOP)
    AppSpeechService RealSpeechService;
    AppSpeech::SetInstance(&RealSpeechService);
#elif defined(Q_OS_IOS)
    AppIosSpeech RealSpeechService;
    AppSpeech::SetInstance(&RealSpeechService);
#endif

    AppStartup::Mark("application object");

    qCInfo(LogApp) << "Ikonika starting";

    if (!DatabaseSeedFile.isEmpty() && !SeedDatabaseFromJson(DatabaseSeedFile))
    {
        return -1;
    }

#if defined(IKONIKA_DESKTOP)
    AppSingleInstance SingleInstance;

    if (!SingleInstance.TryAcquire())
    {
        return 0;
    }

    AppStartup::Mark("single instance");
#endif

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    LoadBundledFonts();

    AppDictionary::Get().Initialise();

    AppStartup::Mark("dictionary");

    std::unique_ptr<QQmlApplicationEngine> Engine;
    AppSwitchCover SwitchCover;
    QPointer<QQuickWindow> CurrentWindow;
    AppReminder& Reminder = AppReminder::Get();

    const auto LoadInterface = [&](bool bStartHidden) -> QQuickWindow*
    {
        Engine = std::make_unique<QQmlApplicationEngine>();
        Engine->addImageProvider(QStringLiteral("book"), new AppBookImageProvider);
        AppLocalization::Get().SetEngine(Engine.get());

        if (bStartHidden)
        {
            Engine->setInitialProperties({{QStringLiteral("visible"), false}});
        }

        Engine->loadFromModule("Ikonika", IsMobileLayoutEnabled() ? "MobileMain" : "Main");

        if (Engine->rootObjects().isEmpty())
        {
            return nullptr;
        }

        QQuickWindow* Loaded = qobject_cast<QQuickWindow*>(Engine->rootObjects().constFirst());

        if (Loaded == nullptr)
        {
            return nullptr;
        }

        Loaded->setIcon(QApplication::windowIcon());
        CurrentWindow = Loaded;

        QObject::connect(&Reminder, &AppReminder::showWindowRequested, Loaded, [Loaded]()
        {
            Loaded->show();
            Loaded->raise();
            Loaded->requestActivate();
        });

#if defined(IKONIKA_DESKTOP)
        QObject::connect(&SingleInstance, &AppSingleInstance::activationRequested, Loaded, [Loaded]()
        {
            Loaded->show();
            Loaded->setWindowState(Qt::WindowMaximized);
            Loaded->raise();
            Loaded->requestActivate();
        });
#endif

        QObject::connect(Loaded, &QWindow::visibleChanged, Loaded, [&SyncController](bool bVisible)
        {
            if (bVisible)
            {
                AppSpeech::Get().Prewarm();

                return;
            }

            AppSpeech::Get().Unload();
            SyncController.OnApplicationStateChanged(Qt::ApplicationHidden);
        });

        return Loaded;
    };

    AppStartup::Mark("qml engine");

    AppLocalization::Get().ApplySavedLanguage();

    AppStartup::Mark("localisation");

    QQuickWindow* Window = LoadInterface(false);

    if (Engine->rootObjects().isEmpty())
    {
        qCCritical(LogApp) << "QML failed to load, exiting";

        return -1;
    }

    AppStartup::Mark("qml");

    WorkspaceHost.SetSwitchGate([&SwitchCover, &CurrentWindow](std::function<void()> Switch)
    {
        if (CurrentWindow == nullptr || !CurrentWindow->isVisible())
        {
            Switch();

            return;
        }

        SwitchCover.Show(*CurrentWindow, std::move(Switch));
    });

    QObject::connect(&WorkspaceHost, &AppWorkspaceHost::aboutToSwitch, &WorkspaceHost, [&Engine]()
    {
        qCInfo(LogApp) << "Closing the screens before the workspace switch";

        AppLocalization::Get().SetEngine(nullptr);
        Engine.reset();
    });

    QObject::connect(&WorkspaceHost, &AppWorkspaceHost::switched, &WorkspaceHost,
                     [&LoadInterface, &SwitchCover]()
    {
        AppHeartbeat::SetActive(SwitchCover.IsShown());
        AppHeartbeat::Beat();

        AppDictionary::Get().OpenSettingsPair();
        AppOnboarding::Get().refresh();
        AppSpeech::Get().ReloadFromSettings();

        QQuickWindow* Reloaded = LoadInterface(SwitchCover.IsShown());

        AppHeartbeat::SetActive(false);

        if (Reloaded == nullptr)
        {
            qCCritical(LogApp) << "QML failed to load after the workspace switch, exiting";

            QApplication::exit(-1);

            return;
        }

        qCInfo(LogApp) << "Screens rebuilt for the new workspace";

        AppLocalization::Get().ApplySavedLanguage();

        SwitchCover.HandOver(*Reloaded);

        Reloaded->requestActivate();
    });

    QObject::connect(&Application, &QGuiApplication::applicationStateChanged, &SyncController,
                     &AppSyncController::OnApplicationStateChanged);

    if (QNetworkInformation::loadDefaultBackend())
    {
        QObject::connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged, &SyncController,
                         [&SyncController](QNetworkInformation::Reachability Reachability)
                         {
                             if (Reachability == QNetworkInformation::Reachability::Online)
                             {
                                 SyncController.OnNetworkBack();
                             }
                         });
    }
    else
    {
        qCInfo(LogSync) << "No network information backend, sync will not hear when the network returns";
    }

    if (Window != nullptr)
    {
        // Started here rather than from QML, so the tray exists before anything can be closed.
        Reminder.Start();

        QObject::connect(&Reminder, &AppReminder::quitRequested, &Application, &QApplication::quit);

        // Launched by Windows at sign-in, so it starts in the tray rather than putting a window in
        // front of someone who was doing something else.
        bool bStartHidden = Application.arguments().contains(QStringLiteral("--minimised"));

#if defined(Q_OS_WIN)
        bStartHidden = bStartHidden || AppStartupTask::WasLaunchedAtSignIn();
#endif

        if (bStartHidden && Reminder.ShouldHideOnClose())
        {
            Window->hide();

            qCInfo(LogApp) << "Started minimised to the tray";
        }

        if (Window->isVisible())
        {
            AppSpeech::Get().Prewarm();
        }

        AppSoundEffects::Get().Prewarm();
    }

    AppStartup::Mark("window");
    AppStartup::ReportReady();
    AppStartup::ReportFirstFrame(Window);

    // Read after the window is up: an offline machine would otherwise pay a connection timeout
    // before its first frame.
    AppCatalog& Catalog = AppCatalog::Get();

    if (AppPaths::IsStorePackage())
    {
        qCInfo(LogApp) << "Running as a Store package, so the Store delivers application updates";
    }

    QObject::connect(&Catalog, &AppCatalog::catalogChanged, &Catalog, []()
    {
        AppUpdater& Updater = AppUpdater::Get();

        Updater.AdoptOrDiscardPendingUpdate();

        if (!AppCatalog::Get().IsUpdateAvailable())
        {
            return;
        }

        qCInfo(LogApp) << "Version" << AppCatalog::Get().GetPublishedVersion()
                       << "is published; this is" << QCoreApplication::applicationVersion();
    });

    const QString RequestedPackage = qEnvironmentVariable("IKONIKA_INSTALL_PACKAGE");

    if (!RequestedPackage.isEmpty())
    {
        QObject::connect(&Catalog, &AppCatalog::catalogChanged, &Catalog, [RequestedPackage]()
        {
            const QStringList RequestedIds =
                RequestedPackage.split(QLatin1Char(','), Qt::SkipEmptyParts);

            for (const QString& Id : RequestedIds)
            {
                AppPackageDownloader::Get().download(Id.trimmed());
            }
        });

        AppPackageDownloader& Downloader = AppPackageDownloader::Get();

        QObject::connect(&Downloader, &AppPackageDownloader::packageInstalled,
                         &Downloader, [](const QString& Id)
        {
            qCInfo(LogApp) << "Package install finished:" << Id;
        });

        QObject::connect(&Downloader, &AppPackageDownloader::packageFailed,
                         &Downloader, [](const QString& Id, const QString& Error)
        {
            qCWarning(LogApp) << "Package install failed:" << Id << Error;
        });
    }

    Catalog.Refresh();

    QObject::connect(&Application, &QCoreApplication::aboutToQuit, &Application, [&Engine]()
    {
        // Destroy the QML tree - with it the WebEngine views - while the event loop can
        // still service the teardown. Left until main returns, the tree is destroyed after
        // exec() and after the loop has stopped, and QtWebEngine's profile/context teardown
        // then runs as a post-routine with no loop to deliver its deletions: a debug DCHECK
        // crash on exit. aboutToQuit is emitted before the loop stops.
        Engine.reset();
    });

    const int ExitCode = Application.exec();

    qCInfo(LogApp) << "Ikonika exiting with code" << ExitCode;

    return ExitCode;
}
