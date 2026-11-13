#include "AppDictionary.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppPaths.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#include <algorithm>

AppDictionary& AppDictionary::Get()
{
    static AppDictionary Instance;

    return Instance;
}

AppDictionaryNotifier& AppDictionary::Notifier()
{
    static AppDictionaryNotifier Instance;

    return Instance;
}

QString AppDictionary::GetInstalledPackagesPath()
{
    // Local rather than roaming application data in a release build, since a dictionary is tens of
    // megabytes and the roaming directory is synchronised between machines on a domain. The
    // trainer database is small enough to roam; these are not. See AppPaths.
    return AppPaths::InstalledDictionariesDirectory();
}

QStringList AppDictionary::GetSearchPaths()
{
    QStringList Paths;

    Paths << GetInstalledPackagesPath();

    // What the package ships with, read-only beside the executable. Searched after the writable
    // location so a dictionary the user downloaded later wins over the bundled one for the same
    // language, which is what makes an update possible without touching the install.
    Paths << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("dictionaries"));

#ifdef PROJECT_ROOT_DIRECTORY
    // The checkout's own build output, so a developer needs no download. Debug only: this is an
    // absolute path on the machine that built the app, and a packaged build that kept it would
    // read and hold the checkout's files on that machine rather than the ones it shipped with.
    Paths << QStringLiteral(PROJECT_ROOT_DIRECTORY "/data/built");
#endif

    return Paths;
}

void AppDictionary::Discover()
{
    Packages.clear();
    OutdatedPackages.clear();

    std::vector<FTDictionaryPackage> InstalledIncompatible;

    for (const QString& Path : GetSearchPaths())
    {
        std::vector<FTDictionaryPackage> Incompatible;

        for (FTDictionaryPackage& Found : FindDictionaryPackages(Path.toStdString(), &Incompatible))
        {
            // An earlier search path already claimed this language, so it wins. Installed packages
            // are searched first for that reason.
            const auto Existing = std::find_if(Packages.begin(), Packages.end(),
                [&Found](const FTDictionaryPackage& Other)
                {
                    return Other.SharesPairWith(Found);
                });

            if (Existing != Packages.end())
            {
                continue;
            }

            qCInfo(LogDatabase) << "Dictionary found:" << QString::fromStdString(Found.FilePath)
                                << "teaches" << QString::fromStdString(Found.TargetLanguage)
                                << "in" << QString::fromStdString(Found.NativeLanguages.front())
                                << "with" << Found.WordCount << "words";

            Packages.push_back(std::move(Found));
        }

        if (Path == GetInstalledPackagesPath())
        {
            InstalledIncompatible = std::move(Incompatible);
        }
    }

    for (FTDictionaryPackage& Outdated : InstalledIncompatible)
    {
        const auto SharesPair = [&Outdated](const FTDictionaryPackage& Other)
        {
            return Other.SharesPairWith(Outdated);
        };

        if (std::any_of(OutdatedPackages.begin(), OutdatedPackages.end(), SharesPair)
            || std::any_of(Packages.begin(), Packages.end(), SharesPair))
        {
            continue;
        }

        qCWarning(LogDatabase) << "Installed dictionary needs an update:"
                               << QString::fromStdString(Outdated.FilePath) << "teaches"
                               << QString::fromStdString(Outdated.TargetLanguage);

        OutdatedPackages.push_back(std::move(Outdated));
    }

    bDiscovered = true;

    // A stable order for the language list in settings, since a directory scan has no order worth
    // showing.
    std::sort(Packages.begin(), Packages.end(),
              [](const FTDictionaryPackage& Left, const FTDictionaryPackage& Right)
              {
                  return Left.TargetLanguage < Right.TargetLanguage;
              });

    if (Packages.empty())
    {
        qCWarning(LogDatabase) << "No dictionaries found. Searched:" << GetSearchPaths();
    }

    // Which pairs exist is decided by the files, so the settings' own idea of what is available is
    // replaced rather than kept alongside it.
    std::vector<FTLanguageRules> Pairs;

    for (const FTDictionaryPackage& Package : Packages)
    {
        for (const std::string& Native : Package.NativeLanguages)
        {
            FTLanguageRules Pair;
            Pair.TargetLanguage = Package.TargetLanguage;
            Pair.NativeLanguage = Native;

            Pairs.push_back(Pair);
        }
    }

    FTSettings::SetAvailableLanguagePairs(std::move(Pairs));

    // The saved pair was accepted unchecked while nothing was known to be installed, so it is
    // checked now that the files have been seen.
    const FTLanguageRules& Saved = FTSettings::Get().GetLanguageRules();

    if (!IsOutdated(Saved.TargetLanguage, Saved.NativeLanguage))
    {
        FTSettings::Get().RevalidateLanguageRules();
    }

    AppDatabase::Get().ApplyActiveLanguages();
}

void AppDictionary::Close()
{
    if (Connection == nullptr)
    {
        return;
    }

    qCInfo(LogDatabase) << "Closing the open dictionary";

    Repository.reset();
    Connection.reset();
    SentenceTree.reset();
    bSentenceTreeLoaded = false;
    ActiveIndex = -1;
    OpenNativeLanguage.clear();
    ++Generation;
}

bool AppDictionary::Initialise()
{
    if (Connection != nullptr && Repository != nullptr)
    {
        return true;
    }

    if (!bDiscovered)
    {
        Discover();
    }

    if (Packages.empty())
    {
        return false;
    }

    const FTLanguageRules& Wanted = FTSettings::Get().GetLanguageRules();

    if (Open(Wanted.TargetLanguage, Wanted.NativeLanguage))
    {
        return true;
    }

    if (IsOutdated(Wanted.TargetLanguage, Wanted.NativeLanguage))
    {
        return false;
    }

    // The saved pair is not installed, which is what happens when a package is removed. Rather than
    // leaving the app with no dictionary at all, the first installed one is opened and the setting
    // follows it.
    const FTDictionaryPackage& First = Packages.front();

    qCWarning(LogDatabase) << "No dictionary for the saved language pair"
                           << QString::fromStdString(Wanted.TargetLanguage) << "->"
                           << QString::fromStdString(Wanted.NativeLanguage)
                           << "; using" << QString::fromStdString(First.TargetLanguage);

    return Open(First.TargetLanguage, First.NativeLanguages.front());
}

bool AppDictionary::Open(const std::string& TargetLanguage, const std::string& NativeLanguage)
{
    const auto Match = std::find_if(Packages.begin(), Packages.end(),
        [&TargetLanguage, &NativeLanguage](const FTDictionaryPackage& Package)
        {
            return Package.TargetLanguage == TargetLanguage
                && Package.SupportsNative(NativeLanguage);
        });

    if (Match == Packages.end())
    {
        return false;
    }

    std::optional<FTDatabase> Opened = FTDatabase::Open(Match->FilePath);

    if (!Opened.has_value())
    {
        qCWarning(LogDatabase) << "Could not open the dictionary at"
                               << QString::fromStdString(Match->FilePath);

        return false;
    }

    // Replaced only once the new one is open, so a failed switch leaves the working dictionary in
    // place rather than closing it first and ending up with none.
    Connection = std::make_unique<FTDatabase>(std::move(*Opened));

    if (!FTDictionaryRepository::HasSearchIndices(*Connection))
    {
        qCWarning(LogDatabase) << "The dictionary has no search indices, so lookups scan whole tables:"
                               << QString::fromStdString(Match->FilePath);
    }

    // The file holds every native language it was built with, so the repository is told which one to
    // read. Opening the same file for a different native language is a new repository, not a filter
    // applied later.
    Repository = std::make_unique<FTDictionaryRepository>(*Connection, NativeLanguage);

    ++Generation;

    SentenceTree.reset();
    bSentenceTreeLoaded = false;

    // What the old dictionary said about which spellings are also verbs was about that dictionary.
    AppCards::ForgetAnnotations();

    ActiveIndex = static_cast<int>(std::distance(Packages.begin(), Match));
    OpenNativeLanguage = NativeLanguage;

    qCInfo(LogDatabase) << "Dictionary opened:" << QString::fromStdString(Match->FilePath)
                        << QString::fromStdString(TargetLanguage) << "->"
                        << QString::fromStdString(NativeLanguage);

    emit Notifier().languagesChanged();

    return true;
}

const std::vector<FTDictionaryPackage>& AppDictionary::GetPackages() const
{
    return Packages;
}

const std::vector<FTDictionaryPackage>& AppDictionary::GetOutdatedPackages() const
{
    return OutdatedPackages;
}

bool AppDictionary::IsOutdated(const std::string& TargetLanguage, const std::string& NativeLanguage) const
{
    return std::any_of(OutdatedPackages.begin(), OutdatedPackages.end(),
                       [&TargetLanguage, &NativeLanguage](const FTDictionaryPackage& Package)
                       {
                           return Package.TargetLanguage == TargetLanguage && Package.SupportsNative(NativeLanguage);
                       });
}

bool AppDictionary::IsInstalled(const std::string& TargetLanguage, const std::string& NativeLanguage) const
{
    return std::any_of(Packages.begin(), Packages.end(),
                       [&TargetLanguage, &NativeLanguage](const FTDictionaryPackage& Package)
                       {
                           return Package.TargetLanguage == TargetLanguage && Package.SupportsNative(NativeLanguage);
                       });
}

bool AppDictionary::IsReplacedBy(const FTCatalogPackage& Published) const
{
    return std::any_of(OutdatedPackages.begin(), OutdatedPackages.end(),
                       [&Published](const FTDictionaryPackage& Package)
                       {
                           return Published.Replaces(Package);
                       });
}

const FTDictionaryPackage* AppDictionary::GetActivePackage() const
{
    if (ActiveIndex < 0 || ActiveIndex >= static_cast<int>(Packages.size()))
    {
        return nullptr;
    }

    return &Packages[static_cast<size_t>(ActiveIndex)];
}

uint64_t AppDictionary::GetGeneration() const
{
    return Generation;
}

bool AppDictionary::IsOpenFor(const std::string& TargetLanguage, const std::string& NativeLanguage) const
{
    const FTDictionaryPackage* Active = GetActivePackage();

    return Connection != nullptr && Repository != nullptr && Active != nullptr
        && Active->TargetLanguage == TargetLanguage && OpenNativeLanguage == NativeLanguage;
}

bool AppDictionary::OpenSettingsPair()
{
    const FTLanguageRules& Wanted = FTSettings::Get().GetLanguageRules();

    if (IsOpenFor(Wanted.TargetLanguage, Wanted.NativeLanguage))
    {
        return true;
    }

    return Open(Wanted.TargetLanguage, Wanted.NativeLanguage);
}

bool AppDictionary::IsReady()
{
    if (Connection == nullptr || Repository == nullptr)
    {
        Initialise();
    }

    return Connection != nullptr && Repository != nullptr;
}

FTDictionaryRepository& AppDictionary::GetRepository()
{
    return *Repository;
}

const FTSentenceTree* AppDictionary::GetSentenceTree()
{
    if (!IsReady())
    {
        return nullptr;
    }

    if (!bSentenceTreeLoaded)
    {
        std::optional<FTSentenceTree> Loaded = FTSentenceTree::Load(*Connection);

        if (Loaded.has_value())
        {
            SentenceTree = std::make_shared<const FTSentenceTree>(std::move(*Loaded));
        }

        bSentenceTreeLoaded = true;
    }

    return SentenceTree.get();
}

std::shared_ptr<const FTSentenceTree> AppDictionary::GetLoadedSentenceTree() const
{
    return SentenceTree;
}

void AppDictionary::AdoptSentenceTree(std::shared_ptr<const FTSentenceTree> Tree, uint64_t ForGeneration)
{
    if (Tree == nullptr || ForGeneration != Generation || bSentenceTreeLoaded)
    {
        return;
    }

    SentenceTree = std::move(Tree);
    bSentenceTreeLoaded = true;
}
