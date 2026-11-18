#include "AppAccountRegistry.h"
#include "AppBookCloud.h"
#include "AppBookImageProvider.h"
#include "AppFreeBooks.h"
#include "AppCatalog.h"
#include "AppCloudStore.h"
#include "AppControllers.h"
#include "AppCredentialStore.h"
#include "AppDatabase.h"
#include "AppDatabaseService.h"
#include "AppDictionary.h"
#include "AppGoogleAccount.h"
#include "AppLocalization.h"
#include "AppLog.h"
#include "AppMetrics.h"
#include "AppHostWindow.h"
#include "AppOnboarding.h"
#include "AppPackageDownloader.h"
#include "AppPaths.h"
#include "AppReminder.h"
#include "AppReaderIntake.h"
#include "AppSoundEffects.h"
#include "AppHeartbeat.h"
#include "AppSwitchCover.h"
#include "AppSessionCheckpoints.h"
#include "AppSpeech.h"
#include "AppStartup.h"
#include "AppUpdater.h"
#include "AppWorkspaceHost.h"
#include "controllers/AppSyncController.h"

#include "core/FTCard.h"
#include "core/FTLog.h"

#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QJSValue>
#include <QtGui/private/qhighdpiscaling_p.h>
#include <qpa/qplatformwindow.h>
#include <qpa/qwindowsysteminterface.h>
#include <cmath>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInformation>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QPointer>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QScreen>
#include <QStringList>
#include <QTimer>
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
#include "AppQuickLookup.h"
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

#if defined(IKONIKA_DESKTOP)
    void ApplyForcedWindowSize(QQuickWindow& Window, const QStringList& Arguments)
    {
        const QString Value = FindArgumentValue(Arguments, QStringLiteral("-window="));

        if (Value.isEmpty())
        {
            return;
        }

        const QStringList Parts = Value.split(QLatin1Char('x'));
        bool bWidthValid{false};
        bool bHeightValid{false};
        const int Width = Parts.size() == 2 ? Parts[0].toInt(&bWidthValid) : 0;
        const int Height = Parts.size() == 2 ? Parts[1].toInt(&bHeightValid) : 0;

        if (!bWidthValid || !bHeightValid || Width <= 0 || Height <= 0)
        {
            qCWarning(LogApp) << "Ignoring -window, expected WIDTHxHEIGHT, got" << Value;

            return;
        }

        const QSize Size{Width, Height};
        Window.setMinimumSize(Size);
        Window.setMaximumSize(Size);
        Window.resize(Size);

        qCInfo(LogApp) << "Window forced to" << Width << "x" << Height << "logical pixels";
    }
#endif

    constexpr qreal MobileDesignWidth{560.0};
    constexpr qreal MobileDesignHeight{900.0};
    constexpr qreal MobileScaleStep{0.05};
    constexpr int TabletShortestSide{600};

    void ApplyMobileScale(QQuickWindow& Window, qreal& AppliedFactor)
    {
        if (Window.screen() == nullptr || Window.width() <= 0 || Window.height() <= 0)
        {
            return;
        }

        static const qreal ScreenFactor{QHighDpiScaling::factor(Window.screen())};

        const qreal BaseWidth{Window.width() * AppliedFactor};
        const qreal BaseHeight{Window.height() * AppliedFactor};
        const qreal Fit{std::min(BaseWidth / MobileDesignWidth, BaseHeight / MobileDesignHeight)};
        const qreal Wanted{IsMobileLayoutEnabled() ? std::max(1.0, std::floor(Fit / MobileScaleStep) * MobileScaleStep) : 1.0};

        if (qFuzzyCompare(Wanted, AppliedFactor))
        {
            return;
        }

        qCInfo(LogApp) << "Window is" << BaseWidth << "x" << BaseHeight << "unscaled, scaling the interface by" << Wanted;

        AppliedFactor = Wanted;
        QHighDpiScaling::setScreenFactor(Window.screen(), ScreenFactor * Wanted);

        if (QPlatformWindow* Platform = Window.handle())
        {
            const QRect NativeGeometry{Platform->geometry()};

            QWindowSystemInterface::handleWindowDevicePixelRatioChanged(&Window);
            QWindowSystemInterface::handleGeometryChange(&Window, NativeGeometry);
            QWindowSystemInterface::handleExposeEvent(&Window, QRect{QPoint{}, NativeGeometry.size()});
        }
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

        if (Code == QStringLiteral("n"))
        {
            return GenderNeuter;
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

    void RunCheckpointCheck()
    {
        const std::string Kind{"Check"};
        const std::string State{R"({"step":3})"};

        AppSessionCheckpoints::Save(Kind, "walk", State);

        const std::vector<std::pair<std::string, std::string>> Saved{AppSessionCheckpoints::LoadAll(Kind)};

        AppSessionCheckpoints::Clear(Kind, "walk");

        const bool bRoundTrip = Saved.size() == 1 && Saved.front().first == "walk" && Saved.front().second == State;

        if (!bRoundTrip || !AppSessionCheckpoints::LoadAll(Kind).empty())
        {
            qCWarning(LogApp) << "checkpoint check failed:" << Saved.size() << "saves read back";

            return;
        }

        qCInfo(LogApp) << "checkpoint check done";
    }

    void StartNarrationCheck(const QString& Sentences)
    {
        constexpr int CheckTimeoutMilliseconds = 60000;
        const QStringList Queue{Sentences.split(QLatin1Char('|'), Qt::SkipEmptyParts)};
        AppSpeech& Speech{AppSpeech::Get()};

        if (!Speech.CanNarrate())
        {
            qCWarning(LogSpeech) << "narration check failed: the speech service cannot narrate";

            return;
        }

        auto Finished{std::make_shared<int>(0)};

        QObject::connect(&Speech, &AppSpeech::narrationStarted, &Speech, [](int Id)
        {
            qCInfo(LogSpeech) << "narration check: sentence" << Id << "started";
        });

        QObject::connect(&Speech, &AppSpeech::narrationFinished, &Speech, [Finished, Count = Queue.size()](int Id)
        {
            qCInfo(LogSpeech) << "narration check: sentence" << Id << "finished";

            if (++*Finished == Count)
            {
                qCInfo(LogSpeech) << "narration check done";
            }
        });

        QObject::connect(&Speech, &AppSpeech::narrationFailed, &Speech, []()
        {
            qCWarning(LogSpeech) << "narration check failed: the speech service reported a failure";
        });

        QTimer::singleShot(CheckTimeoutMilliseconds, &Speech, [Finished, Count = Queue.size()]()
        {
            if (*Finished < Count)
            {
                qCWarning(LogSpeech) << "narration check failed:" << *Finished << "of" << Count << "sentences finished";
            }
        });

        for (int Index = 0; Index < Queue.size(); ++Index)
        {
            Speech.QueueNarration(Index, Queue.at(Index), QStringLiteral("fr"));
        }
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

    AppReaderIntake::Get();
#endif

    AppMemoryCredentialStore MemoryCredentials;
#if defined(Q_OS_WIN)
    AppWindowsCredentialStore WindowsCredentials(GetCredentialPrefix());
    AppCredentialStore& Credentials{AppPaths::IsHeadless() ? static_cast<AppCredentialStore&>(MemoryCredentials)
                                                           : WindowsCredentials};
#elif defined(Q_OS_IOS)
    AppIosCredentialStore KeychainCredentials(GetCredentialPrefix());
    AppCredentialStore& Credentials{AppPaths::IsHeadless() ? static_cast<AppCredentialStore&>(MemoryCredentials)
                                                           : KeychainCredentials};
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

    if (qEnvironmentVariableIsSet("IKONIKA_CHECKPOINT_CHECK"))
    {
        RunCheckpointCheck();
    }

    const QString NarrationCheck{qEnvironmentVariable("IKONIKA_NARRATION_CHECK")};

    if (!NarrationCheck.isEmpty())
    {
        QTimer::singleShot(0, [NarrationCheck]() { StartNarrationCheck(NarrationCheck); });
    }

    AppStartup::Mark("application object");

    qCInfo(LogApp) << "Ikonika starting";

    if (!DatabaseSeedFile.isEmpty() && !SeedDatabaseFromJson(DatabaseSeedFile))
    {
        return -1;
    }

#if defined(IKONIKA_DESKTOP)
    AppSingleInstance SingleInstance;
    QString BookPath;
    for (const QString& Argument : Application.arguments().mid(1))
    {
        const QFileInfo File{Argument};
        if (File.isFile() && QStringList{QStringLiteral("pdf"), QStringLiteral("epub"), QStringLiteral("fb2")}.contains(File.suffix().toLower()))
        {
            BookPath = File.absoluteFilePath();
            break;
        }
    }

    if (!SingleInstance.TryAcquire(BookPath))
    {
        return 0;
    }

    AppStartup::Mark("single instance");
    QObject::connect(&SingleInstance, &AppSingleInstance::bookRequested,
                     &AppReaderIntake::Get(), &AppReaderIntake::OpenPath);
#endif

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    LoadBundledFonts();

    AppPackageDownloader::RecoverInterruptedInstalls();

    AppDictionary::Get().Initialise();

    AppStartup::Mark("dictionary");

    QObject* QuickLookupService = nullptr;

#if defined(Q_OS_WIN)
    std::unique_ptr<AppQuickLookup> QuickLookup;

    if (!IsMobileLayoutEnabled())
    {
        QuickLookup = std::make_unique<AppQuickLookup>();
        QuickLookupService = QuickLookup.get();
        QObject::connect(&WorkspaceHost, &AppWorkspaceHost::aboutToSwitch, QuickLookup.get(), &AppQuickLookup::Reset);
    }
#endif

    std::unique_ptr<QQmlApplicationEngine> Engine;
    std::unique_ptr<AppControllers> Controllers;
    AppHostWindow HostWindow;
    AppSwitchCover SwitchCover;
    QPointer<QQuickWindow> CurrentWindow;
    QPointer<QObject> CurrentRoot;
    AppReminder& Reminder = AppReminder::Get();

#if defined(IKONIKA_DESKTOP)
    const bool bAdaptiveLayout{Application.arguments().contains(QStringLiteral("-adaptive"))};
    const bool bForcedWindowSize{!FindArgumentValue(Application.arguments(), QStringLiteral("-window=")).isEmpty()};
#else
    const QSize ScreenSize{QGuiApplication::primaryScreen()->size()};
    const bool bAdaptiveLayout{std::min(ScreenSize.width(), ScreenSize.height()) >= TabletShortestSide};
    const bool bForcedWindowSize{false};

    if (bAdaptiveLayout)
    {
        SetMobileLayoutEnabled(ScreenSize.height() > ScreenSize.width());

        qCInfo(LogApp) << "Tablet screen" << ScreenSize << ", starting with the"
                       << (IsMobileLayoutEnabled() ? "mobile" : "desktop") << "layout";
    }
#endif

    QTimer LayoutSettle;
    LayoutSettle.setSingleShot(true);
    LayoutSettle.setInterval(300);
    qreal AppliedScreenFactor{1.0};

    const auto CloseInterface = [&Engine, &HostWindow]()
    {
        HostWindow.Unload();
        AppLocalization::Get().SetEngine(nullptr);
        Engine.reset();
    };

    const auto LoadInterface = [&](bool bStartHidden, const QVariantMap& LayoutState = {}) -> QQuickWindow*
    {
        Engine = std::make_unique<QQmlApplicationEngine>();
        Engine->rootContext()->setContextProperty(QStringLiteral("quickLookupService"), QuickLookupService);
        Engine->addImageProvider(QStringLiteral("book"), new AppBookImageProvider);
        QObject::connect(Engine.get(), &QQmlEngine::quit, &Application, &QCoreApplication::quit, Qt::QueuedConnection);
        QObject::connect(Engine.get(), &QQmlEngine::exit, &Application, &QCoreApplication::exit, Qt::QueuedConnection);
        AppLocalization::Get().SetEngine(Engine.get());

        const bool bFreshWindow{!HostWindow.IsCreated()};
        const bool bMobile{IsMobileLayoutEnabled()};
        QQuickWindow* Loaded{&HostWindow.EnsureWindow()};
        bool bStartupCover{false};

        CurrentWindow = Loaded;

#if defined(IKONIKA_DESKTOP)
        const AppMetrics Metrics;
#endif

        if (bFreshWindow)
        {
#if defined(IKONIKA_DESKTOP)
            Loaded->resize(bMobile ? QSize{412, 887} : QSize{Metrics.GetWindowWidth(), Metrics.GetWindowHeight()});
            ApplyForcedWindowSize(*Loaded, QCoreApplication::arguments());
#endif

            if (!bForcedWindowSize)
            {
                QObject::connect(Loaded, &QWindow::widthChanged, &LayoutSettle, qOverload<>(&QTimer::start));
                QObject::connect(Loaded, &QWindow::heightChanged, &LayoutSettle, qOverload<>(&QTimer::start));
            }

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

            AppStartup::ReportFirstFrame(Loaded);

            if (!bStartHidden)
            {
                Loaded->show();

                bool bCoverDrawn{false};
                QEventLoop CoverDrawn;

                SwitchCover.Show(*Loaded, [&bCoverDrawn, &CoverDrawn]()
                {
                    bCoverDrawn = true;
                    CoverDrawn.quit();
                });

                if (!bCoverDrawn)
                {
                    CoverDrawn.exec();
                }

                bStartupCover = true;
            }
        }

        ApplyMobileScale(*Loaded, AppliedScreenFactor);

#if defined(IKONIKA_DESKTOP)
        if (!bForcedWindowSize)
        {
            Loaded->setMinimumSize(bMobile ? QSize{360, 640} : QSize{Metrics.GetWindowMinimumWidth(), Metrics.GetWindowMinimumHeight()});
        }
#endif

        if (Controllers == nullptr)
        {
            Controllers = std::make_unique<AppControllers>();
            QQmlEngine::setObjectOwnership(Controllers.get(), QQmlEngine::CppOwnership);
        }

        const QVariantMap InitialProperties{{QStringLiteral("controllers"), QVariant::fromValue(Controllers.get())},
                                            {QStringLiteral("layoutState"), LayoutState}};

        if (HostWindow.Load(*Engine, bMobile ? QStringLiteral("MobileMain") : QStringLiteral("Main"), InitialProperties) == nullptr)
        {
            return nullptr;
        }

        CurrentRoot = HostWindow.GetRoot();

        if (!bForcedWindowSize)
        {
            LayoutSettle.start();
        }

        if (bStartupCover)
        {
            SwitchCover.HandOver(*Loaded);
        }

        return Loaded;
    };

    AppStartup::Mark("qml engine");

    AppLocalization::Get().ApplySavedLanguage();

    AppStartup::Mark("localisation");

    bool bStartHidden = Application.arguments().contains(QStringLiteral("--minimised"));

#if defined(Q_OS_WIN)
    bStartHidden = bStartHidden || AppStartupTask::WasLaunchedAtSignIn();
#endif

    bStartHidden = bStartHidden && Reminder.ShouldHideOnClose();

    QQuickWindow* Window = LoadInterface(bStartHidden);

#if defined(IKONIKA_DESKTOP)
    if (!BookPath.isEmpty())
    {
        AppReaderIntake::Get().OpenPath(BookPath);
    }
#endif

    if (Window == nullptr)
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

    QObject::connect(&WorkspaceHost, &AppWorkspaceHost::aboutToSwitch, &WorkspaceHost, [&CloseInterface, &Controllers]()
    {
        qCInfo(LogApp) << "Closing the screens and their controllers before the workspace switch";

        CloseInterface();
        Controllers.reset();
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

    QObject::connect(&LayoutSettle, &QTimer::timeout, &LayoutSettle,
                     [&CurrentWindow, &CurrentRoot, &SwitchCover, &CloseInterface, &LoadInterface, &AppliedScreenFactor, &LayoutSettle, bAdaptiveLayout]()
    {
        if (CurrentWindow == nullptr)
        {
            return;
        }

        if (SwitchCover.IsShown())
        {
            LayoutSettle.start();

            return;
        }

        const bool bWantsMobile{CurrentWindow->height() > CurrentWindow->width()};

        if (!bAdaptiveLayout || bWantsMobile == IsMobileLayoutEnabled())
        {
            ApplyMobileScale(*CurrentWindow, AppliedScreenFactor);

            return;
        }

        qCInfo(LogApp) << "Window is now" << CurrentWindow->width() << "x" << CurrentWindow->height()
                       << ", switching to the" << (bWantsMobile ? "mobile" : "desktop") << "layout";

        SetMobileLayoutEnabled(bWantsMobile);

        SwitchCover.Show(*CurrentWindow, [&SwitchCover, &CloseInterface, &LoadInterface, &CurrentRoot]()
        {
            QVariant LayoutState;

            if (CurrentRoot != nullptr)
            {
                QMetaObject::invokeMethod(CurrentRoot, "captureLayoutState", Q_RETURN_ARG(QVariant, LayoutState));
            }

            if (LayoutState.metaType() == QMetaType::fromType<QJSValue>())
            {
                LayoutState = LayoutState.value<QJSValue>().toVariant();
            }

            CloseInterface();

            QQuickWindow* Reloaded = LoadInterface(true, LayoutState.toMap());

            if (Reloaded == nullptr)
            {
                qCCritical(LogApp) << "QML failed to load after the layout switch, exiting";

                QApplication::exit(-1);

                return;
            }

            AppLocalization::Get().ApplySavedLanguage();

            SwitchCover.HandOver(*Reloaded);

            Reloaded->requestActivate();

            qCInfo(LogApp) << "Screens rebuilt for the" << (IsMobileLayoutEnabled() ? "mobile" : "desktop") << "layout";
        });
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

        if (bStartHidden)
        {
            qCInfo(LogApp) << "Started minimised to the tray";
        }

        if (Window->isVisible())
        {
            AppSpeech::Get().Prewarm();
        }

#if defined(Q_OS_WIN) && defined(QT_DEBUG)
        const QString QuickLookupText = FindArgumentValue(Application.arguments(), QStringLiteral("-quick-lookup="));
        if (QuickLookup && AppPaths::IsHeadless() && !QuickLookupText.isEmpty())
        {
            QTimer::singleShot(0, QuickLookup.get(), [&QuickLookup, QuickLookupText]()
            {
                emit QuickLookup->lookupRequested(QuickLookupText);
            });
        }

        if (QuickLookup && AppPaths::IsHeadless() && Application.arguments().contains(QStringLiteral("-quick-type")))
        {
            QTimer::singleShot(0, QuickLookup.get(), [&QuickLookup]()
            {
                emit QuickLookup->typedLookupRequested();
            });
        }
#endif

        AppSoundEffects::Get().Prewarm();
    }

    AppStartup::Mark("window");
    AppStartup::ReportReady();

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

    QObject::connect(&Catalog, &AppCatalog::catalogChanged, &Catalog, []()
    {
        AppPackageDownloader::Get().RestoreQueue();
    }, Qt::SingleShotConnection);

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
        }, Qt::SingleShotConnection);

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

    QObject::connect(&Application, &QCoreApplication::aboutToQuit, &Application, [&CloseInterface, &Controllers]()
    {
        // Destroy the QML tree - with it the WebEngine views - while the event loop can
        // still service the teardown. Left until main returns, the tree is destroyed after
        // exec() and after the loop has stopped, and QtWebEngine's profile/context teardown
        // then runs as a post-routine with no loop to deliver its deletions: a debug DCHECK
        // crash on exit. aboutToQuit is emitted before the loop stops.
        CloseInterface();
        Controllers.reset();
    });

    const int ExitCode = Application.exec();

    qCInfo(LogApp) << "Ikonika exiting with code" << ExitCode;

    return ExitCode;
}
