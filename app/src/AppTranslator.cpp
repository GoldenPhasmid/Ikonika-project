#include "AppTranslator.h"

#include "AppCatalog.h"
#include "AppPackageDownloader.h"
#include "AppPaths.h"
#include "AppTranslationRequests.h"

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

    QString ModelFolderFor(const std::string& TargetLanguage, const std::string& NativeLanguage)
    {
        return ModelFolderForPair(QString::fromStdString(TargetLanguage), QString::fromStdString(NativeLanguage));
    }

    /** Serialises the model: it is loaded on a worker and used from workers, and one decode at a
        time is all this needs. */
    QMutex TranslateMutex;

    std::optional<FTInContextTranslator>& Translator()
    {
        static std::optional<FTInContextTranslator> Instance;

        return Instance;
    }

    std::optional<FTCatalogPackage> PackageFor(const std::string& TargetLanguage, const std::string& NativeLanguage)
    {
        return AppCatalog::Get().FindTranslationPackage(TargetLanguage, NativeLanguage);
    }

    /** Which pair's model is loaded, or the one whose load failed. Empty before the first attempt.
        Held rather than a plain flag so that switching language loads the other model instead of
        going on translating Spanish with the French one. */
    QString LoadedFolder;

    FAppTranslationRequests Requests;

    QAtomicInteger<int> BusyCount = 0;

    /** Loads on first use, and again when the pair has changed. Called with the lock held, from a
        worker. */
    void EnsureLoaded(const QString& Folder)
    {
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
        const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

        return IsSupportedPair(Rules.TargetLanguage, Rules.NativeLanguage);
    }

    bool IsSupportedPair(const std::string& TargetLanguage, const std::string& NativeLanguage)
    {
        return !FindModelDirectory(ModelFolderFor(TargetLanguage, NativeLanguage)).isEmpty();
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

    void Translate(const QString& Text, quintptr Owner)
    {
        const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

        Translate(Text, Owner, Rules.TargetLanguage, Rules.NativeLanguage);
    }

    void Translate(const QString& Text, quintptr Owner, const std::string& TargetLanguage,
                   const std::string& NativeLanguage)
    {
        const QString Trimmed = Text.trimmed();

        if (Trimmed.isEmpty() || !IsSupportedPair(TargetLanguage, NativeLanguage))
        {
            return;
        }

        const QString Folder = ModelFolderFor(TargetLanguage, NativeLanguage);
        const quint64 Generation = Requests.Begin(Owner);

        BusyCount.fetchAndAddOrdered(1);

        QThreadPool::globalInstance()->start([Trimmed, Generation, Folder]()
        {
            QString Result;

            {
                const QMutexLocker Lock(&TranslateMutex);

                // Checked with the lock held as well: loading the model takes long enough that a
                // second word can be typed while the first is still waiting for it.
                if (Requests.IsLatest(Generation))
                {
                    const bool bWasLoaded = Translator().has_value();

                    EnsureLoaded(Folder);

                    if (!bWasLoaded)
                    {
                        QMetaObject::invokeMethod(&Notifier(),
                                                  &AppTranslatorNotifier::readyChanged,
                                                  Qt::QueuedConnection);
                    }

                    if (Translator().has_value())
                    {
                        Result = QString::fromStdString(Translator()->Translate(
                            Trimmed.toStdString(),
                            [&Trimmed, Generation](const std::string& Partial)
                            {
                                if (!Requests.IsLatest(Generation))
                                {
                                    return false;
                                }

                                const QString PartialText = QString::fromStdString(Partial);

                                QMetaObject::invokeMethod(&Notifier(), [Trimmed, PartialText]()
                                {
                                    emit Notifier().translationProgress(Trimmed, PartialText);
                                }, Qt::QueuedConnection);

                                return true;
                            }));
                    }
                }
            }

            BusyCount.fetchAndAddOrdered(-1);

            // Superseded while decoding. The answer is for a word the user has moved on from.
            if (!Requests.Finish(Generation))
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
        const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

        return IsPairOffered(Rules.TargetLanguage, Rules.NativeLanguage);
    }

    bool IsPairOffered(const std::string& TargetLanguage, const std::string& NativeLanguage)
    {
        return PackageFor(TargetLanguage, NativeLanguage).has_value();
    }

    bool IsDownloading()
    {
        const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

        return IsDownloading(Rules.TargetLanguage, Rules.NativeLanguage);
    }

    bool IsDownloading(const std::string& TargetLanguage, const std::string& NativeLanguage)
    {
        const std::optional<FTCatalogPackage> Package = PackageFor(TargetLanguage, NativeLanguage);

        return Package.has_value()
            && AppPackageDownloader::Get().isPending(QString::fromStdString(Package->Id));
    }

    void Download()
    {
        const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

        Download(Rules.TargetLanguage, Rules.NativeLanguage);
    }

    void Download(const std::string& TargetLanguage, const std::string& NativeLanguage)
    {
        const std::optional<FTCatalogPackage> Package = PackageFor(TargetLanguage, NativeLanguage);

        if (!Package.has_value())
        {
            qCWarning(LogTranslate) << "No translation package is published for this language pair";

            return;
        }

        AppPackageDownloader::Get().download(QString::fromStdString(Package->Id));
    }

    qint64 GetDownloadBytes()
    {
        const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

        return GetDownloadBytes(Rules.TargetLanguage, Rules.NativeLanguage);
    }

    qint64 GetDownloadBytes(const std::string& TargetLanguage, const std::string& NativeLanguage)
    {
        const std::optional<FTCatalogPackage> Package = PackageFor(TargetLanguage, NativeLanguage);

        return Package.has_value() ? Package->Bytes : 0;
    }

    double GetDownloadProgress()
    {
        const FTLanguageRules& Rules = FTSettings::Get().GetLanguageRules();

        return GetDownloadProgress(Rules.TargetLanguage, Rules.NativeLanguage);
    }

    double GetDownloadProgress(const std::string& TargetLanguage, const std::string& NativeLanguage)
    {
        const std::optional<FTCatalogPackage> Package = PackageFor(TargetLanguage, NativeLanguage);
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
