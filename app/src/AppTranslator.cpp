#include "AppTranslator.h"

#include "AppCatalog.h"
#include "AppPackageDownloader.h"
#include "AppPaths.h"

#include "core/FTInContextTranslator.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QAtomicInteger>
#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QThreadPool>

#include <optional>

namespace
{
    QString ModelFolderForPair(const QString& TargetLanguage, const QString& NativeLanguage)
    {
        if (TargetLanguage.isEmpty() || NativeLanguage.isEmpty())
        {
            return {};
        }

        return QStringLiteral("opus-mt-%1-%2").arg(TargetLanguage, NativeLanguage);
    }
    
    QStringList ModelDirectories(const QString& Folder)
    {
        QStringList Directories;

        if (Folder.isEmpty())
        {
            return Directories;
        }

        Directories << QDir(AppPaths::DownloadedModelsDirectory()).filePath(Folder);

        Directories << QDir(QCoreApplication::applicationDirPath())
                           .filePath(QStringLiteral("models/") + Folder);

#ifdef PROJECT_ROOT_DIRECTORY
        Directories << QDir(QStringLiteral(PROJECT_ROOT_DIRECTORY))
                           .filePath(QStringLiteral("third_party/models/") + Folder);
#endif

        return Directories;
    }
    
    QMutex FoundMutex;

    QHash<QString, QString>& FoundDirectories()
    {
        static QHash<QString, QString> Found;

        return Found;
    }

    void ForgetFoundDirectories()
    {
        const QMutexLocker Lock(&FoundMutex);

        FoundDirectories().clear();
    }

    QString FindModelDirectory(const QString& Folder)
    {
        if (Folder.isEmpty())
        {
            return {};
        }

        const QMutexLocker Lock(&FoundMutex);

        QHash<QString, QString>& Found = FoundDirectories();

        const auto Known = Found.constFind(Folder);

        if (Known != Found.constEnd())
        {
            return *Known;
        }

        QString Directory;

        for (const QString& Candidate : ModelDirectories(Folder))
        {
            if (QDir(Candidate).exists(QStringLiteral("encoder_model.onnx")))
            {
                Directory = Candidate;

                break;
            }
        }

        Found.insert(Folder, Directory);

        return Directory;
    }

    /** The folder for whichever pair is being learned now. */
    QString CurrentModelFolder()
    {
        const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

        return ModelFolderForPair(QString::fromStdString(Rules.TargetLanguage),
                                  QString::fromStdString(Rules.NativeLanguage));
    }

    /** Serialises the model: it is loaded on a worker and used from workers, and one decode at a
        time is all this needs. */
    QMutex TranslateMutex;

    std::optional<FTInContextTranslator>& Translator()
    {
        static std::optional<FTInContextTranslator> Instance;

        return Instance;
    }

    std::optional<FTCatalogPackage> CurrentPackage()
    {
        const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

        return AppCatalog::Get().FindTranslationPackage(Rules.TargetLanguage, Rules.NativeLanguage);
    }

    /** Which pair's model is loaded, or the one whose load failed. Empty before the first attempt.
        Held rather than a plain flag so that switching language loads the other model instead of
        going on translating Spanish with the French one. */
    QString LoadedFolder;

    /** Which request is the newest. An older one throws its answer away rather than replacing a
        translation of the word the user has since moved on from. */
    QAtomicInteger<quint64> PendingGeneration = 0;

    QAtomicInteger<int> BusyCount = 0;

    /** Loads on first use, and again when the pair has changed. Called with the lock held, from a
        worker. */
    void EnsureLoaded()
    {
        const QString Folder = CurrentModelFolder();

        if (LoadedFolder == Folder)
        {
            return;
        }

        // Dropped before the next is built rather than after, since two of these hold a pair of
        // ONNX sessions each and there is no reason for both to be resident.
        Translator().reset();

        LoadedFolder = Folder;

        const QString Directory = FindModelDirectory(Folder);

        if (Directory.isEmpty())
        {
            qCWarning(LogTranslate) << "no OPUS-MT model for" << Folder
                                    << "- translation is unavailable for this pair";

            return;
        }

        Translator() = FTInContextTranslator::Create(Directory.toStdString());

        if (!Translator().has_value())
        {
            qCWarning(LogTranslate) << "the OPUS-MT model at" << Directory << "could not be loaded";

            return;
        }

        qCInfo(LogTranslate) << "translation model loaded from" << Directory;
    }
}

namespace AppTranslator
{
    AppTranslatorNotifier& Notifier()
    {
        static AppTranslatorNotifier Instance;

        return Instance;
    }

    bool IsSupportedPair()
    {
        // Whether a model for this pair is on disk, and nothing else. A pair is supported by
        // fetching its model, so there is no list here to forget to add a language to.
        return !FindModelDirectory(CurrentModelFolder()).isEmpty();
    }

    bool IsAvailable()
    {
        const QMutexLocker Lock(&TranslateMutex);

        return Translator().has_value();
    }

    bool IsBusy()
    {
        return BusyCount.loadRelaxed() > 0;
    }

    void Translate(const QString& Text)
    {
        const QString Trimmed = Text.trimmed();

        if (Trimmed.isEmpty() || !IsSupportedPair())
        {
            return;
        }

        const quint64 Generation = ++PendingGeneration;

        BusyCount.fetchAndAddOrdered(1);

        QThreadPool::globalInstance()->start([Trimmed, Generation]()
        {
            QString Result;

            {
                const QMutexLocker Lock(&TranslateMutex);

                // Checked with the lock held as well: loading the model takes long enough that a
                // second word can be typed while the first is still waiting for it.
                if (Generation == PendingGeneration.loadRelaxed())
                {
                    const bool bWasLoaded = Translator().has_value();

                    EnsureLoaded();

                    if (!bWasLoaded)
                    {
                        QMetaObject::invokeMethod(&Notifier(),
                                                  &AppTranslatorNotifier::readyChanged,
                                                  Qt::QueuedConnection);
                    }

                    if (Translator().has_value())
                    {
                        Result = QString::fromStdString(Translator()->Translate(
                            Trimmed.toStdString()));
                    }
                }
            }

            BusyCount.fetchAndAddOrdered(-1);

            // Superseded while decoding. The answer is for a word the user has moved on from.
            if (Generation != PendingGeneration.loadRelaxed())
            {
                return;
            }

            QMetaObject::invokeMethod(&Notifier(), [Trimmed, Result]()
            {
                emit Notifier().translated(Trimmed, Result);
            }, Qt::QueuedConnection);
        });
    }

    void Reset()
    {
        {
            const QMutexLocker Lock(&TranslateMutex);

            Translator().reset();
            LoadedFolder.clear();
        }

        ForgetFoundDirectories();

        qCInfo(LogTranslate) << "translation models released, the next request looks for them again";

        emit Notifier().modelsChanged();
    }

    bool IsPairOffered()
    {
        return CurrentPackage().has_value();
    }

    bool IsDownloading()
    {
        const std::optional<FTCatalogPackage> Package = CurrentPackage();

        return Package.has_value()
            && AppPackageDownloader::Get().isPending(QString::fromStdString(Package->Id));
    }

    void Download()
    {
        const std::optional<FTCatalogPackage> Package = CurrentPackage();

        if (!Package.has_value())
        {
            qCWarning(LogTranslate) << "No translation package is published for this language pair";

            return;
        }

        AppPackageDownloader::Get().download(QString::fromStdString(Package->Id));
    }

    qint64 GetDownloadBytes()
    {
        const std::optional<FTCatalogPackage> Package = CurrentPackage();

        return Package.has_value() ? Package->Bytes : 0;
    }

    double GetDownloadProgress()
    {
        const std::optional<FTCatalogPackage> Package = CurrentPackage();
        const AppPackageDownloader& Downloader = AppPackageDownloader::Get();

        if (!Package.has_value() || Downloader.GetActiveId() != QString::fromStdString(Package->Id)
            || Downloader.GetTotalBytes() <= 0)
        {
            return 0.0;
        }

        return static_cast<double>(Downloader.GetReceivedBytes())
            / static_cast<double>(Downloader.GetTotalBytes());
    }

    QString GetAttribution()
    {
        return {};
    }
}
