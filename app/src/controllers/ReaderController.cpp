#include "ReaderController.h"

#include "AppBookCloud.h"
#include "AppDatabase.h"
#include "AppFreeBooks.h"
#include "AppReaderSettings.h"
#include "AppSyncController.h"
#include "FTPdfDocumentExtractor.h"

#include "core/FTCloudBookRepository.h"
#include "core/FTDocumentExtractor.h"
#include "core/FTLog.h"
#include "core/FTReaderRepository.h"

#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QMetaObject>
#include <QPointer>
#include <QProcess>
#include <QThreadPool>
#include <QUrl>
#include <QWindow>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>
#elif defined(Q_OS_IOS)
#include "AppIosReader.h"
#include "AppIosShare.h"
#endif

namespace
{
    QVariantMap ToVariant(const FTReaderDocumentRow& Row)
    {
        QVariantMap Entry;
        Entry[QStringLiteral("id")] = QString::fromStdString(Row.Id);
        Entry[QStringLiteral("title")] = QString::fromStdString(Row.Title);
        Entry[QStringLiteral("format")] = QString::fromStdString(Row.Format);
        Entry[QStringLiteral("path")] = QString::fromStdString(Row.FilePath);
        Entry[QStringLiteral("exists")] = Row.bExists;
        Entry[QStringLiteral("added")] = static_cast<qint64>(Row.AddedDate);
        Entry[QStringLiteral("lastOpened")] = static_cast<qint64>(Row.LastOpenedDate);
        Entry[QStringLiteral("cloudId")] = QString();
        Entry[QStringLiteral("local")] = true;
        Entry[QStringLiteral("progress")] = -1.0;

        return Entry;
    }

    double ProgressOf(const FTReadingPlace& Place)
    {
        if (Place.PositionCount <= 0)
        {
            return -1.0;
        }

        return std::clamp(double(Place.Position + 1) / double(Place.PositionCount), 0.0, 1.0);
    }

    QVariantMap ToVariant(const FTCloudBookRow& Book)
    {
        QVariantMap Entry;
        Entry[QStringLiteral("id")] = QString();
        Entry[QStringLiteral("title")] = QString::fromStdString(Book.Title);
        Entry[QStringLiteral("format")] = QString::fromStdString(Book.Format);
        Entry[QStringLiteral("path")] = QString();
        Entry[QStringLiteral("exists")] = false;
        Entry[QStringLiteral("added")] = static_cast<qint64>(Book.AddedDate);
        Entry[QStringLiteral("lastOpened")] = static_cast<qint64>(Book.LastOpenedDate);
        Entry[QStringLiteral("cloudId")] = QString::fromStdString(Book.Id);
        Entry[QStringLiteral("size")] = static_cast<qint64>(Book.FileSize);
        Entry[QStringLiteral("local")] = false;
        Entry[QStringLiteral("progress")] = ProgressOf(
            FTReadingPlace{Book.LastPosition, Book.LastZoom, Book.LastScrollX, Book.LastScrollY, Book.PositionCount});

        return Entry;
    }

    std::optional<std::string> FileNameTitle(const FTReaderDocumentRow& Row)
    {
#if defined(Q_OS_ANDROID)
        Q_UNUSED(Row);

        return std::nullopt;
#else
        if (ParseDocumentFormat(Row.Format) != EFTDocumentFormat::Pdf)
        {
            return std::nullopt;
        }

        const std::string Title{QFileInfo(QString::fromStdString(Row.FilePath)).completeBaseName().toStdString()};

        if (Title.empty() || Title == Row.Title || Title == Row.CloudId)
        {
            return std::nullopt;
        }

        return Title;
#endif
    }

    FTReadingPlace ChoosePlace(const FTReaderDocumentRow& Row)
    {
        const FTReadingPlace Local{Row.LastPosition, Row.LastZoom, Row.LastScrollX, Row.LastScrollY,
                                   Row.PositionCount};

        if (Row.CloudId.empty())
        {
            return Local;
        }

        const std::optional<FTCloudBookRow> Book{AppDatabase::Get().GetCloudBooks().FindBook(Row.CloudId)};

        if (!Book.has_value() || Book->LastUpdatedDate <= Row.LastOpenedDate)
        {
            return Local;
        }

        return FTReadingPlace{Book->LastPosition, Book->LastZoom, Book->LastScrollX, Book->LastScrollY,
                              Book->PositionCount > 0 ? Book->PositionCount : Row.PositionCount};
    }

    const std::optional<FTReaderDocument> ExtractDocument(const QString& FilePath,
                                                           EFTDocumentFormat Format)
    {
        const std::string Path = FilePath.toStdString();

        switch (Format)
        {
        case EFTDocumentFormat::Txt:
            return FTTxtDocumentExtractor().Extract(Path);
        case EFTDocumentFormat::Fb2:
            return FTFb2DocumentExtractor().Extract(Path);
        case EFTDocumentFormat::Epub:
            return FTEpubDocumentExtractor().Extract(Path);
        case EFTDocumentFormat::Pdf:
        case EFTDocumentFormat::Unknown:
            return std::nullopt;
        }

        return std::nullopt;
    }

#if defined(Q_OS_ANDROID)
    const char* AndroidBridgeClass = "com/goldenphasmid/ikonika/ReaderFilePicker";
    const char* AndroidWindowClass = "com/goldenphasmid/ikonika/ReaderWindow";
    const char* AndroidKeysClass = "com/goldenphasmid/ikonika/ReaderKeys";

    void AndroidSetWindowState(const char* Method, bool bEnabled)
    {
        QJniObject Activity = QNativeInterface::QAndroidApplication::context();

        QJniObject::callStaticMethod<void>(AndroidWindowClass, Method,
            "(Landroid/content/Context;Z)V", Activity.object<jobject>(),
            static_cast<jboolean>(bEnabled));
    }

    ReaderController* AndroidInstance = nullptr;

    void AndroidNativeOnVolumeKey(JNIEnv*, jclass, jboolean bForward)
    {
        if (AndroidInstance == nullptr)
        {
            return;
        }

        ReaderController* Instance = AndroidInstance;
        const bool bForwardValue = bForward;

        QMetaObject::invokeMethod(Instance, [Instance, bForwardValue]()
        {
            emit Instance->volumePageTurn(bForwardValue);
        }, Qt::QueuedConnection);
    }

    void AndroidNativeOnPicked(JNIEnv*, jclass, jstring Uri, jstring DisplayName)
    {
        if (AndroidInstance == nullptr)
        {
            return;
        }

        const QString UriString = QJniObject(Uri).toString();
        const QString NameString = QJniObject(DisplayName).toString();
        ReaderController* Instance = AndroidInstance;

        QMetaObject::invokeMethod(Instance, [Instance, UriString, NameString]()
        {
            Instance->HandleAndroidPicked(UriString, NameString);
        }, Qt::QueuedConnection);
    }

    void AndroidNativeOnPickFailed(JNIEnv*, jclass, jstring Message)
    {
        if (AndroidInstance == nullptr)
        {
            return;
        }

        const QString Text = QJniObject(Message).toString();
        ReaderController* Instance = AndroidInstance;

        QMetaObject::invokeMethod(Instance, [Instance, Text]()
        {
            Instance->HandleAndroidPickFailed(Text);
        }, Qt::QueuedConnection);
    }
#endif
}

ReaderController::ReaderController(QObject* Parent)
    : QObject(Parent)
    , ReadAloud(ParagraphModel)
{
    AppDatabase::Get().Initialise();

    connect(this, &ReaderController::currentDocumentChanged, this, [this]()
    {
        ReadAloud.SetTitle(CurrentTitle);
        ApplyKeepScreenOn(bReading && !bUnreadable);
        UpdateVolumeKeys();

        if (!bReading)
        {
            setFullScreen(false);
        }
    });

    connect(&AppReaderSettings::Get(), &AppReaderSettings::changed, this, [this]()
    {
        UpdateVolumeKeys();

        emit readingSettingsChanged();
    });

    if (AppBookCloud* Cloud = AppBookCloud::GetInstance())
    {
        connect(Cloud, &AppBookCloud::transfersChanged, this, &ReaderController::transfersChanged);
        connect(Cloud, &AppBookCloud::failed, this, &ReaderController::cloudFailed);
        connect(Cloud, &AppBookCloud::libraryChanged, this, &ReaderController::refresh);
        connect(Cloud, &AppBookCloud::availabilityChanged, this, [this]()
        {
            emit cloudAvailabilityChanged();

            refresh();
        });
    }

    if (AppFreeBooks* Free = AppFreeBooks::GetInstance())
    {
        connect(Free, &AppFreeBooks::catalogChanged, this, &ReaderController::refresh);
        connect(Free, &AppFreeBooks::libraryChanged, this, &ReaderController::refresh);
        connect(Free, &AppFreeBooks::transfersChanged, this, &ReaderController::transfersChanged);
        connect(Free, &AppFreeBooks::failed, this, &ReaderController::freeBookFailed);
    }

    if (AppSyncController* Sync = AppSyncController::GetInstance())
    {
        connect(Sync, &AppSyncController::dataMerged, this, &ReaderController::refresh);
        connect(Sync, &AppSyncController::accountChanged, this, &ReaderController::cloudAvailabilityChanged);
    }

    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState State)
    {
        if (State != Qt::ApplicationActive)
        {
            SaveCloudPlace();
        }
    });

    connect(qGuiApp, &QCoreApplication::aboutToQuit, this, &ReaderController::SaveCloudPlace);

#if defined(Q_OS_ANDROID)
    AndroidInstance = this;

    static const JNINativeMethod Methods[] = {
        { "nativeOnPicked", "(Ljava/lang/String;Ljava/lang/String;)V",
          reinterpret_cast<void*>(&AndroidNativeOnPicked) },
        { "nativeOnPickFailed", "(Ljava/lang/String;)V",
          reinterpret_cast<void*>(&AndroidNativeOnPickFailed) },
    };

    QJniEnvironment Env;

    if (!Env.registerNativeMethods(AndroidBridgeClass, Methods, 2))
    {
        qCWarning(LogApp) << "Reader: could not register the Android file-picker bridge";
    }

    static const JNINativeMethod KeyMethods[] = {
        { "nativeOnVolumeKey", "(Z)V", reinterpret_cast<void*>(&AndroidNativeOnVolumeKey) },
    };

    if (!Env.registerNativeMethods(AndroidKeysClass, KeyMethods, 1))
    {
        qCWarning(LogApp) << "Reader: could not register the Android volume-key bridge";
    }
#endif
}

ReaderController::~ReaderController()
{
    SaveCloudPlace();
    ApplyKeepScreenOn(false);
    setFullScreen(false);

#if defined(Q_OS_ANDROID)
    QJniObject::callStaticMethod<void>(AndroidKeysClass, "setTurnPages", "(Z)V", jboolean{false});
#endif
}

void ReaderController::UpdateVolumeKeys()
{
#if defined(Q_OS_ANDROID)
    const bool bEnabled = bReading && !bUnreadable && AreVolumeKeysTurningPages();

    QJniObject::callStaticMethod<void>(AndroidKeysClass, "setTurnPages", "(Z)V",
                                       static_cast<jboolean>(bEnabled));
#endif
}

void ReaderController::openExternal(const QString& FilePath, const QString& DisplayName)
{
    qCInfo(LogApp) << "Reader: opening a book handed over by another app:" << DisplayName;

    OpenFile(FilePath, DisplayName);
}

void ReaderController::ApplyKeepScreenOn(bool bEnabled)
{
    if (bKeepScreenOn == bEnabled)
    {
        return;
    }

    bKeepScreenOn = bEnabled;

    qCInfo(LogApp) << "Reader: keep screen on" << bEnabled;

#if defined(Q_OS_ANDROID)
    AndroidSetWindowState("setKeepScreenOn", bEnabled);
#elif defined(Q_OS_IOS)
    AppIosReader::SetKeepScreenOn(bEnabled);
#endif
}

void ReaderController::setFullScreen(bool bEnabled)
{
    if (bFullScreen == bEnabled)
    {
        return;
    }

    bFullScreen = bEnabled;

#if defined(Q_OS_ANDROID)
    AndroidSetWindowState("setImmersive", bEnabled);
#endif

    emit fullScreenChanged();
}

bool ReaderController::IsCloudAvailable() const
{
    AppBookCloud* Cloud = AppBookCloud::GetInstance();

    return Cloud != nullptr && Cloud->IsAvailable();
}

QVariantMap ReaderController::GetTransfers() const
{
    AppBookCloud* Cloud = AppBookCloud::GetInstance();

    return Cloud != nullptr ? Cloud->GetTransfers() : QVariantMap();
}

bool ReaderController::IsFullScreen() const
{
    return bFullScreen;
}

int ReaderController::GetTextSize() const
{
    return AppReaderSettings::Get().GetTextSize();
}

void ReaderController::SetTextSize(int Value)
{
    AppReaderSettings::Get().SetTextSize(Value);
}

int ReaderController::GetLineSpacing() const
{
    return AppReaderSettings::Get().GetLineSpacing();
}

void ReaderController::SetLineSpacing(int Value)
{
    AppReaderSettings::Get().SetLineSpacing(Value);
}

int ReaderController::GetTextMargin() const
{
    return AppReaderSettings::Get().GetTextMargin();
}

void ReaderController::SetTextMargin(int Value)
{
    AppReaderSettings::Get().SetTextMargin(Value);
}

QString ReaderController::GetPageTone() const
{
    return AppReaderSettings::Get().GetPageTone();
}

void ReaderController::SetPageTone(const QString& Value)
{
    AppReaderSettings::Get().SetPageTone(Value);
}

bool ReaderController::IsCropMargins() const
{
    return AppReaderSettings::Get().IsCropMargins();
}

void ReaderController::SetCropMargins(bool bValue)
{
    AppReaderSettings::Get().SetCropMargins(bValue);
}

bool ReaderController::AreVolumeKeysTurningPages() const
{
    return AppReaderSettings::Get().AreVolumeKeysTurningPages();
}

void ReaderController::SetVolumeKeysTurnPages(bool bValue)
{
    AppReaderSettings::Get().SetVolumeKeysTurnPages(bValue);
}

void ReaderController::refresh()
{
    if (!AppDatabase::Get().IsReady())
    {
        return;
    }

    if (AppBookCloud* Cloud = AppBookCloud::GetInstance())
    {
        Cloud->ForgetDeletedBooks();
    }

    std::unordered_map<std::string, FTCloudBookRow> CloudBooks;

    for (FTCloudBookRow& Book : AppDatabase::Get().GetCloudBooks().ListBooks())
    {
        CloudBooks.emplace(Book.Id, std::move(Book));
    }

    const AppFreeBooks* Free{AppFreeBooks::GetInstance()};

    QHash<QString, QString> LevelsByPath;

    if (Free != nullptr && Free->IsAvailable())
    {
        for (const FTCatalogBook& Book : Free->GetCatalog()->Books)
        {
            LevelsByPath.insert(Free->GetBookPath(Book), QString::fromStdString(Book.Level));
        }
    }

    std::vector<QVariantMap> Entries;
    std::unordered_set<std::string> Linked;

    for (FTReaderDocumentRow Row : AppDatabase::Get().GetReaderLibrary().ListDocuments())
    {
        if (const std::optional<std::string> Renamed{FileNameTitle(Row)})
        {
            AppDatabase::Get().GetReaderLibrary().SetTitle(Row.Id, *Renamed);
            Row.Title = *Renamed;
        }

        QVariantMap Entry{ToVariant(Row)};
        const auto Book{CloudBooks.find(Row.CloudId)};

        Entry[QStringLiteral("progress")] = ProgressOf(ChoosePlace(Row));
        Entry[QStringLiteral("free")] = Free != nullptr && Free->OwnsFile(QString::fromStdString(Row.FilePath));
        Entry[QStringLiteral("level")] = LevelsByPath.value(QFileInfo(QString::fromStdString(Row.FilePath)).absoluteFilePath());

        if (!Row.CloudId.empty() && Book != CloudBooks.end())
        {
            Entry[QStringLiteral("cloudId")] = QString::fromStdString(Row.CloudId);
            Entry[QStringLiteral("added")] =
                static_cast<qint64>(std::min(Row.AddedDate, Book->second.AddedDate));
            Entry[QStringLiteral("lastOpened")] =
                static_cast<qint64>(std::max(Row.LastOpenedDate, Book->second.LastOpenedDate));
            Linked.insert(Row.CloudId);
        }

        Entries.push_back(std::move(Entry));
    }

    for (const auto& [Id, Book] : CloudBooks)
    {
        if (!Linked.contains(Id))
        {
            Entries.push_back(ToVariant(Book));
        }
    }

    std::stable_sort(Entries.begin(), Entries.end(), [](const QVariantMap& Left, const QVariantMap& Right)
    {
        return Left.value(QStringLiteral("lastOpened")).toLongLong()
             > Right.value(QStringLiteral("lastOpened")).toLongLong();
    });

    Documents.clear();

    for (const QVariantMap& Entry : Entries)
    {
        Documents.append(Entry);
    }

    FreeBooks.clear();

    if (Free != nullptr && Free->IsAvailable())
    {
        QHash<QString, QVariantMap> ByPath;

        for (const QVariantMap& Entry : Entries)
        {
            ByPath.insert(QFileInfo(Entry.value(QStringLiteral("path")).toString()).absoluteFilePath(), Entry);
        }

        for (const FTCatalogBook& Book : Free->GetCatalog()->Books)
        {
            const auto Downloaded{ByPath.constFind(Free->GetBookPath(Book))};

            QVariantMap Entry;

            if (Downloaded != ByPath.cend() && Downloaded.value().value(QStringLiteral("exists")).toBool())
            {
                Entry = Downloaded.value();
            }
            else
            {
                Entry[QStringLiteral("id")] = QString();
                Entry[QStringLiteral("title")] = QString::fromStdString(Book.Title);
                Entry[QStringLiteral("format")] = QString::fromLatin1(DocumentFormatName(Book.GetFormat()));
                Entry[QStringLiteral("exists")] = false;
                Entry[QStringLiteral("cloudId")] = QString();
                Entry[QStringLiteral("local")] = false;
                Entry[QStringLiteral("progress")] = -1.0;
            }

            Entry[QStringLiteral("freeId")] = QString::fromStdString(Book.Id);
            Entry[QStringLiteral("author")] = QString::fromStdString(Book.Author);
            Entry[QStringLiteral("level")] = QString::fromStdString(Book.Level);
            Entry[QStringLiteral("size")] = static_cast<qint64>(Book.Bytes);

            FreeBooks.append(Entry);
        }
    }

    emit documentsChanged();
}

bool ReaderController::pickAndOpen()
{
#if defined(Q_OS_ANDROID)
    AndroidPickAndOpen();

    return false;
#elif defined(Q_OS_IOS)
    AppIosReader::PickDocument([Controller = QPointer<ReaderController>(this)](const QString& PickedPath)
    {
        if (Controller)
        {
            Controller->HandleIosPicked(PickedPath);
        }
    });

    return false;
#else
    const QString Path = QFileDialog::getOpenFileName(
        nullptr, tr("Open a book"),
        QString(), tr("Books (*.pdf *.epub *.fb2 *.txt);;All files (*.*)"));

    if (Path.isEmpty())
    {
        return false;
    }

    OpenFile(Path);

    return bReading;
#endif
}

#if defined(Q_OS_ANDROID)
void ReaderController::AndroidPickAndOpen()
{
    QJniObject Activity = QNativeInterface::QAndroidApplication::context();

    QJniObject::callStaticMethod<void>(AndroidBridgeClass, "pickDocument",
        "(Landroid/app/Activity;)V", Activity.object<jobject>());
}

void ReaderController::AndroidShowOnDevice(const QString& Uri)
{
    QJniObject Activity = QNativeInterface::QAndroidApplication::context();
    QJniObject UriJni = QJniObject::fromString(Uri);

    QJniObject::callStaticMethod<void>(AndroidBridgeClass, "showDocument",
        "(Landroid/app/Activity;Ljava/lang/String;)V", Activity.object<jobject>(),
        UriJni.object<jstring>());
}

void ReaderController::HandleAndroidPicked(const QString& Uri, const QString& DisplayName)
{
    OpenFile(Uri, DisplayName);
}

void ReaderController::HandleAndroidPickFailed(const QString& Message)
{
    if (!Message.isEmpty())
    {
        qCWarning(LogApp) << "Reader: Android file pick failed:" << Message;
    }
}
#endif

#if defined(Q_OS_IOS)
void ReaderController::HandleIosPicked(const QString& PickedPath)
{
    const QString Kept{AppIosReader::KeepBook(PickedPath, true)};

    if (Kept.isEmpty())
    {
        return;
    }

    OpenFile(Kept);
}
#endif

bool ReaderController::openPath(const QString& FilePath)
{
    const QString Local = QUrl(FilePath).isLocalFile()
                        ? QUrl(FilePath).toLocalFile()
                        : FilePath;

    if (!QFileInfo::exists(Local))
    {
        qCWarning(LogApp) << "Reader: no such file" << Local;

        return false;
    }

    OpenFile(Local);

    return bReading;
}

bool ReaderController::openDocument(const QString& Id)
{
    if (!AppDatabase::Get().IsReady())
    {
        return false;
    }

    const std::string IdStd = Id.toStdString();

    if (const std::optional<FTReaderDocumentRow> Found{AppDatabase::Get().GetReaderLibrary().FindDocument(IdStd)})
    {
        const FTReaderDocumentRow& Row{*Found};
        const FTReadingPlace Place{ChoosePlace(Row)};

        if (Place.Position != Row.LastPosition)
        {
            qCInfo(LogApp) << "Reader: opening" << QString::fromStdString(Row.Title)
                           << "where another device left it, at" << Place.Position;
        }

        CurrentPositionCount = static_cast<int>(Place.PositionCount);

        AppDatabase::Get().GetReaderLibrary().TouchDocument(IdStd);

        if (!Row.bExists)
        {
            ++PendingGeneration;
            CurrentId = QString::fromStdString(Row.Id);
            CurrentCloudId.clear();
            CurrentTitle = QString::fromStdString(Row.Title);
            CurrentPath.clear();
            CurrentPosition = 0;
            CurrentZoom = 0.0;
            CurrentScroll = QPointF();
            ParagraphModel.Clear();
            Chapters.clear();
            bIsPdf = false;
            bReading = false;
            bLoading = false;
            bUnreadable = false;

            emit currentDocumentChanged();

            qCWarning(LogApp) << "Reader: document missing from disk"
                              << QString::fromStdString(Row.FilePath);

            return false;
        }

        if (ParseDocumentFormat(Row.Format) == EFTDocumentFormat::Pdf)
        {
#if defined(Q_OS_ANDROID)
            const std::optional<FTReaderDocument> Named = FTPdfDocumentExtractor().Extract(Row.FilePath);
#else
            const std::optional<FTReaderDocument> Named{std::nullopt};
#endif
            const bool bRenamed = Named.has_value() && !Named->Title.empty() && Named->Title != Row.Title;

            if (bRenamed)
            {
                AppDatabase::Get().GetReaderLibrary().AddDocument(Row.FilePath, Named->Title, Row.Format);

                qCInfo(LogApp) << "Reader: book renamed from its own title:"
                               << QString::fromStdString(Named->Title);
            }

            ++PendingGeneration;
            CurrentId = QString::fromStdString(Row.Id);
            CurrentCloudId = QString::fromStdString(Row.CloudId);
            CurrentTitle = QString::fromStdString(bRenamed ? Named->Title : Row.Title);
            CurrentPath = QString::fromStdString(Row.FilePath);
            CurrentPosition = static_cast<int>(Place.Position);
            CurrentZoom = Place.Zoom;
            CurrentScroll = QPointF(Place.ScrollX, Place.ScrollY);
            ParagraphModel.Clear();
            Chapters.clear();
            bIsPdf = true;
            bReading = true;
            bLoading = false;
            bUnreadable = false;

            emit currentDocumentChanged();

            return true;
        }

        ++PendingGeneration;
        const quint64 Generation = PendingGeneration;

        CurrentId = QString::fromStdString(Row.Id);
        CurrentCloudId = QString::fromStdString(Row.CloudId);
        CurrentTitle = QString::fromStdString(Row.Title);
        CurrentPath = QString::fromStdString(Row.FilePath);
        CurrentPosition = static_cast<int>(Place.Position);
        CurrentZoom = Place.Zoom;
        CurrentScroll = QPointF(Place.ScrollX, Place.ScrollY);
        ParagraphModel.Clear();
        Chapters.clear();
        bIsPdf = false;
        bReading = true;
        bLoading = true;
        bUnreadable = false;

        emit currentDocumentChanged();

        StartReflowLoad(Generation, QString::fromStdString(Row.FilePath),
                        ParseDocumentFormat(Row.Format), false);

        return true;
    }

    qCWarning(LogApp) << "Reader: document not in the library" << Id;

    return false;
}

void ReaderController::closeDocument()
{
    SaveCloudPlace();

    ++PendingGeneration;
    CurrentId.clear();
    CurrentCloudId.clear();
    CurrentPositionCount = 0;
    CurrentTitle.clear();
    CurrentPath.clear();
    CurrentPosition = 0;
    CurrentZoom = 0.0;
    CurrentScroll = QPointF();
    ParagraphModel.Clear();
    Chapters.clear();
    bIsPdf = false;
    bReading = false;
    bLoading = false;
    bUnreadable = false;

    emit currentDocumentChanged();
}

void ReaderController::savePosition(int Position)
{
    CurrentPosition = Position;

    if (CurrentId.isEmpty() || !AppDatabase::Get().IsReady())
    {
        return;
    }

    AppDatabase::Get().GetReaderLibrary().SetPosition(CurrentId.toStdString(), Position);
}

void ReaderController::savePositionCount(int Count)
{
    if (Count <= 0 || Count == CurrentPositionCount || CurrentId.isEmpty() || !AppDatabase::Get().IsReady())
    {
        return;
    }

    CurrentPositionCount = Count;

    AppDatabase::Get().GetReaderLibrary().SetPositionCount(CurrentId.toStdString(), Count);
}

void ReaderController::saveZoom(qreal Zoom)
{
    CurrentZoom = Zoom;

    if (CurrentId.isEmpty() || !AppDatabase::Get().IsReady())
    {
        return;
    }

    AppDatabase::Get().GetReaderLibrary().SetZoom(CurrentId.toStdString(), Zoom);
}

void ReaderController::saveScroll(QPointF Scroll)
{
    CurrentScroll = Scroll;

    if (CurrentId.isEmpty() || !AppDatabase::Get().IsReady())
    {
        return;
    }

    AppDatabase::Get().GetReaderLibrary().SetScroll(CurrentId.toStdString(), Scroll.x(), Scroll.y());
}

void ReaderController::OpenFile(const QString& FilePath, const QString& DisplayName)
{
    if (!AppDatabase::Get().IsReady())
    {
        return;
    }

    const std::string PathStd = FilePath.toStdString();
    const QString NameForDetection = DisplayName.isEmpty() ? FilePath : DisplayName;
    const EFTDocumentFormat Format = DetectDocumentFormat(NameForDetection.toStdString());

    QString Title = QFileInfo(NameForDetection).completeBaseName();

    const bool bPdf = Format == EFTDocumentFormat::Pdf;

#if defined(Q_OS_ANDROID)
    if (bPdf)
    {
        QElapsedTimer TitleTimer;
        TitleTimer.start();

        const std::optional<FTReaderDocument> Extracted = FTPdfDocumentExtractor().Extract(PathStd);

        qCDebug(LogApp) << "Reader: opened" << FilePath << "PDF title read"
                        << TitleTimer.elapsed() << "ms";

        if (Extracted.has_value() && !Extracted->Title.empty())
        {
            Title = QString::fromStdString(Extracted->Title);
        }
    }
#endif

    const std::string Id = AppDatabase::Get().GetReaderLibrary()
                               .AddDocument(PathStd,
                                            Title.toStdString(),
                                            DocumentFormatName(Format))
                               .value_or("");

    refresh();

    if (Id.empty())
    {
        qCWarning(LogApp) << "Reader: could not record" << FilePath;

        return;
    }

    const std::optional<FTReaderDocumentRow> Recorded{AppDatabase::Get().GetReaderLibrary().FindDocument(Id)};

    CurrentId = QString::fromStdString(Id);
    CurrentCloudId = Recorded.has_value() ? QString::fromStdString(Recorded->CloudId) : QString();
    CurrentPositionCount = Recorded.has_value() ? static_cast<int>(Recorded->PositionCount) : 0;
    CurrentPosition = 0;
    CurrentZoom = 0.0;
    CurrentScroll = QPointF();

    if (bPdf)
    {
        CurrentTitle = Title;
        CurrentPath = FilePath;
        ParagraphModel.Clear();
        Chapters.clear();
        bIsPdf = true;
        bReading = true;
        bLoading = false;
        bUnreadable = false;

        emit currentDocumentChanged();

        return;
    }

    ++PendingGeneration;
    const quint64 Generation = PendingGeneration;

    CurrentTitle = Title;
    CurrentPath = FilePath;
    ParagraphModel.Clear();
    Chapters.clear();
    bIsPdf = false;
    bReading = true;
    bLoading = true;
    bUnreadable = false;

    emit currentDocumentChanged();

    StartReflowLoad(Generation, FilePath, Format, true);
}

void ReaderController::removeDocument(const QString& Id)
{
    if (!AppDatabase::Get().IsReady())
    {
        return;
    }

    const std::optional<FTReaderDocumentRow> Row{AppDatabase::Get().GetReaderLibrary().FindDocument(Id.toStdString())};

    if (Row.has_value() && AppBookCloud::GetInstance() != nullptr)
    {
        AppBookCloud::GetInstance()->ForgetLocalCopy(*Row);
    }

    const QString FilePath{Row.has_value() ? QString::fromStdString(Row->FilePath) : QString()};

    if (AppFreeBooks::GetInstance() != nullptr && AppFreeBooks::GetInstance()->OwnsFile(FilePath))
    {
        if (QFile::remove(FilePath))
        {
            qCInfo(LogApp) << "Reader: deleted the free book file" << FilePath;
        }
        else
        {
            qCWarning(LogApp) << "Reader: could not delete the free book file" << FilePath;
        }
    }

    AppDatabase::Get().GetReaderLibrary().RemoveDocument(Id.toStdString());

    refresh();
}

void ReaderController::locateDocument(const QString& Id)
{
#if defined(Q_OS_ANDROID)
    Q_UNUSED(Id);
#else
    if (!AppDatabase::Get().IsReady())
    {
        return;
    }

    const std::optional<FTReaderDocumentRow> Row{AppDatabase::Get().GetReaderLibrary().FindDocument(Id.toStdString())};

    if (!Row.has_value())
    {
        return;
    }

    const QString Extension{QString::fromStdString(Row->Format)};
    const QString Path = QFileDialog::getOpenFileName(
        nullptr, tr("Find %1").arg(QString::fromStdString(Row->Title)), QString(),
        tr("Books (*.%1);;All files (*.*)").arg(Extension));

    if (Path.isEmpty())
    {
        return;
    }

    if (!AppDatabase::Get().GetReaderLibrary().SetFilePath(Row->Id, Path.toStdString()))
    {
        qCWarning(LogApp) << "Reader: could not point" << QString::fromStdString(Row->Title) << "at" << Path;

        return;
    }

    qCInfo(LogApp) << "Reader: located" << QString::fromStdString(Row->Title) << "at" << Path;

    refresh();
#endif
}

void ReaderController::uploadDocument(const QString& Id)
{
    if (AppBookCloud* Cloud = AppBookCloud::GetInstance())
    {
        Cloud->Upload(Id);
    }
}

void ReaderController::downloadBook(const QString& CloudId)
{
    if (AppBookCloud* Cloud = AppBookCloud::GetInstance())
    {
        Cloud->Download(CloudId);
    }
}

void ReaderController::deleteCloudBook(const QString& CloudId)
{
    if (AppBookCloud* Cloud = AppBookCloud::GetInstance())
    {
        Cloud->RemoveFromCloud(CloudId);
    }
}

void ReaderController::cancelTransfer(const QString& Key)
{
    if (AppBookCloud* Cloud = AppBookCloud::GetInstance())
    {
        Cloud->Cancel(Key);
    }
}

void ReaderController::SaveCloudPlace()
{
    if (CurrentCloudId.isEmpty() || !bReading || bLoading || bUnreadable || !AppDatabase::Get().IsReady())
    {
        return;
    }

    const std::optional<FTCloudBookRow> Book{AppDatabase::Get().GetCloudBooks().FindBook(CurrentCloudId.toStdString())};
    const FTReadingPlace Place{CurrentPosition, CurrentZoom, CurrentScroll.x(), CurrentScroll.y(),
                               CurrentPositionCount};

    if (!Book.has_value())
    {
        return;
    }

    if (Book->LastPosition == Place.Position && Book->LastZoom == Place.Zoom && Book->LastScrollX == Place.ScrollX
        && Book->LastScrollY == Place.ScrollY && (Place.PositionCount <= 0 || Book->PositionCount == Place.PositionCount))
    {
        return;
    }

    AppDatabase::Get().GetCloudBooks().SetReadingPlace(CurrentCloudId.toStdString(), Place);

    qCInfo(LogApp) << "Reader: saved the place in" << CurrentTitle << "for other devices, at" << CurrentPosition;
}

void ReaderController::showOnDevice(const QString& Id)
{
    if (!AppDatabase::Get().IsReady())
    {
        return;
    }

    const std::string IdStd = Id.toStdString();

    for (const FTReaderDocumentRow& Row : AppDatabase::Get().GetReaderLibrary().ListDocuments())
    {
        if (Row.Id != IdStd)
        {
            continue;
        }

        const QString FilePath = QString::fromStdString(Row.FilePath);

#if defined(Q_OS_ANDROID)
        AndroidShowOnDevice(FilePath);
#elif defined(Q_OS_IOS)
        qCInfo(LogApp) << "Reader: offering the book to other apps:" << FilePath;

        AppIosShare::ShareFile(FilePath);
#elif defined(Q_OS_WIN)
        qCInfo(LogApp) << "Reader: showing in the file manager:" << FilePath;

        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                { QStringLiteral("/select,"), QDir::toNativeSeparators(FilePath) });
#else
        qCInfo(LogApp) << "Reader: opening the folder of" << FilePath;

        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(FilePath).absolutePath()));
#endif

        return;
    }

    qCWarning(LogApp) << "Reader: cannot show a document that is not in the library" << Id;
}

void ReaderController::StartReflowLoad(quint64 Generation, const QString& FilePath,
                                       EFTDocumentFormat Format, bool bRecordTitle)
{
    const QPointer<ReaderController> Guard(this);

    QElapsedTimer PipelineTimer;
    PipelineTimer.start();

    QThreadPool::globalInstance()->start([Guard, Generation, FilePath, Format, bRecordTitle,
                                          PipelineTimer]()
    {
        QElapsedTimer ExtractTimer;
        ExtractTimer.start();

        const std::optional<FTReaderDocument> Extracted = ExtractDocument(FilePath, Format);
        const qint64 ExtractMs = ExtractTimer.elapsed();

        if (!Guard)
        {
            return;
        }

        QMetaObject::invokeMethod(Guard.data(),
            [Guard, Generation, FilePath, Format, bRecordTitle, Extracted, ExtractMs, PipelineTimer]()
        {
            if (Guard)
            {
                qCDebug(LogApp) << "Reader: opened" << FilePath
                                << "extraction" << ExtractMs << "ms"
                                << "pipeline" << PipelineTimer.elapsed() << "ms"
                                << "paragraphs" << (Extracted.has_value()
                                    ? qsizetype(Extracted->Paragraphs.size()) : qsizetype(0));

                Guard->OnReflowExtracted(Generation, FilePath, Format, bRecordTitle, Extracted);
            }
        }, Qt::QueuedConnection);
    });
}

void ReaderController::OnReflowExtracted(quint64 Generation, const QString& FilePath,
                                         EFTDocumentFormat Format, bool bRecordTitle,
                                         std::optional<FTReaderDocument> Extracted)
{
    if (Generation != PendingGeneration)
    {
        return;
    }

    bLoading = false;

    if (!Extracted.has_value() || Extracted->Paragraphs.empty())
    {
        bReading = true;
        bUnreadable = true;

        emit currentDocumentChanged();

        qCWarning(LogApp) << "Reader: no readable text in" << FilePath;

        return;
    }

    Chapters.clear();

    for (const FTReaderChapter& Chapter : Extracted->Chapters)
    {
        QVariantMap Entry;
        Entry[QStringLiteral("title")] = QString::fromStdString(Chapter.Title);
        Entry[QStringLiteral("page")] = Chapter.Paragraph;
        Entry[QStringLiteral("level")] = Chapter.Level - 1;
        Chapters.append(Entry);
    }

    Vocabulary.refresh();

    ParagraphModel.SetParagraphs(std::move(Extracted->Paragraphs),
                                 std::move(Extracted->HeadingLevels),
                                 std::move(Extracted->Markup),
                                 std::move(Extracted->Layouts),
                                 std::move(Extracted->Images));
    if (bRecordTitle || CurrentTitle.isEmpty())
    {
        CurrentTitle = QString::fromStdString(Extracted->Title);
    }

    if (bRecordTitle)
    {
        const std::string Id = AppDatabase::Get().GetReaderLibrary().AddDocument(
            FilePath.toStdString(), Extracted->Title, DocumentFormatName(Format)).value_or("");

        if (!Id.empty())
        {
            CurrentId = QString::fromStdString(Id);
        }

        refresh();
    }

    bReading = true;
    bUnreadable = false;

    emit currentDocumentChanged();
}

QVariantList ReaderController::GetDocuments() const
{
    return Documents;
}

QVariantList ReaderController::GetFreeBooks() const
{
    return FreeBooks;
}

QVariantMap ReaderController::GetFreeTransfers() const
{
    if (const AppFreeBooks* Free = AppFreeBooks::GetInstance())
    {
        return Free->GetTransfers();
    }

    return {};
}

void ReaderController::refreshFreeBooks()
{
    if (AppFreeBooks* Free = AppFreeBooks::GetInstance())
    {
        Free->Refresh();
    }
}

void ReaderController::downloadFreeBook(const QString& Id)
{
    if (AppFreeBooks* Free = AppFreeBooks::GetInstance())
    {
        Free->Download(Id);
    }
}

void ReaderController::cancelFreeBook(const QString& Id)
{
    if (AppFreeBooks* Free = AppFreeBooks::GetInstance())
    {
        Free->Cancel(Id);
    }
}

QVariantList ReaderController::GetChapters() const
{
    return Chapters;
}

QString ReaderController::GetCurrentId() const
{
    return CurrentId;
}

QString ReaderController::GetCurrentTitle() const
{
    return CurrentTitle;
}

QString ReaderController::GetCurrentPath() const
{
    return CurrentPath;
}

bool ReaderController::IsPdf() const
{
    return bIsPdf;
}

FTParagraphModel* ReaderController::GetParagraphModel()
{
    return &ParagraphModel;
}

ReaderVocabulary* ReaderController::GetVocabulary()
{
    return &Vocabulary;
}

ReaderReadAloud* ReaderController::GetReadAloud()
{
    return &ReadAloud;
}

bool ReaderController::IsLoading() const
{
    return bLoading;
}

bool ReaderController::IsReading() const
{
    return bReading;
}

bool ReaderController::IsUnreadable() const
{
    return bUnreadable;
}

int ReaderController::GetCurrentPosition() const
{
    return CurrentPosition;
}

qreal ReaderController::GetCurrentZoom() const
{
    return CurrentZoom;
}

QPointF ReaderController::GetCurrentScroll() const
{
    return CurrentScroll;
}
