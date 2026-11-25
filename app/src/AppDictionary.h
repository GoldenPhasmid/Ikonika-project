#pragma once

#include "core/FTCatalog.h"
#include "core/FTDatabase.h"
#include "core/FTDictionaryPackage.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTSentenceTree.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

/**
 * Notifies when user changes foreign language
 */
class AppDictionaryNotifier : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

signals:
    void languagesChanged();
    void dictionaryClosed(const QString& TargetLanguage);
};

/**
 * One foreign language's dictionary. Opens on first use and stays at the same address for the life
 * of the app, so a reference to it survives other languages opening and closing.
 */
class AppLanguageDictionary
{
public:
    explicit AppLanguageDictionary(std::string InTargetLanguage);

    /** Opens the dictionary when it is not open yet. */
    bool IsReady();

    bool IsOpen() const;

    const std::string& GetTargetLanguage() const;

    /** The language its translations are in. The user's native language when the file has it. */
    const std::string& GetTranslationLanguage() const;

    /** Nothing when the dictionary is not open. */
    const FTDictionaryPackage* GetPackage() const;

    /** Changes every time the dictionary closes. */
    uint64_t GetGeneration() const;

    /** Only valid once the dictionary is open. */
    FTDictionaryRepository& GetRepository();

    const FTSentenceTree* GetSentenceTree();

    /** The tree when it is already loaded, so a background task can read it without loading a second copy. */
    std::shared_ptr<const FTSentenceTree> GetLoadedSentenceTree() const;

    /** Keeps a tree a background task loaded, unless the dictionary closed meanwhile or the tree is already loaded. */
    void AdoptSentenceTree(std::shared_ptr<const FTSentenceTree> Tree, uint64_t ForGeneration);

private:
    friend class AppDictionary;

    bool OpenPackage(const FTDictionaryPackage& InPackage, const std::string& InRequestedNative,
                     const std::string& InTranslationLanguage);

    void Release();

    std::string TargetLanguage;
    std::string RequestedNative;
    std::string TranslationLanguage;
    std::optional<FTDictionaryPackage> Package;

    std::unique_ptr<FTDatabase> Connection;
    std::unique_ptr<FTDictionaryRepository> Repository;

    std::shared_ptr<const FTSentenceTree> SentenceTree;
    bool bSentenceTreeLoaded = false;

    uint64_t Generation = 0;
};

/**
 * Handles installed dictionaries and one open dictionary per foreign language
 */
class AppDictionary
{
public:
    static AppDictionary& Get();

    /** The dictionary for this foreign language, opened on first use. */
    static AppLanguageDictionary& Get(const std::string& TargetLanguage);

    /** The dictionary for the language being learned. */
    static AppLanguageDictionary& GetActive();

    static AppDictionaryNotifier& Notifier();

    /**
     * Scans for dictionaries and opens the one matching the saved language settings. Falls back to
     * the first installed dictionary when the saved pair is not among them, which is what happens
     * when a package is removed.
     */
    bool Initialise();

    /** Rescans without changing which dictionaries are open. For after a package is added or removed. */
    void Discover();

    /** Releases every open dictionary file. */
    void Close();

    /** Releases one language's dictionary file. Needed before a package replaces the file on disk, since
        the connection would otherwise be reading a file being rewritten underneath it. */
    void Close(const std::string& TargetLanguage);

    const std::vector<FTDictionaryPackage>& GetPackages() const;

    const std::vector<FTDictionaryPackage>& GetOutdatedPackages() const;

    bool IsOutdated(const std::string& TargetLanguage, const std::string& NativeLanguage) const;

    bool IsInstalled(const std::string& TargetLanguage, const std::string& NativeLanguage) const;

    bool IsReplacedBy(const FTCatalogPackage& Published) const;

    /** The language whose open dictionary this package would replace. Empty when none would be. */
    std::string FindOpenLanguageReplacedBy(const FTCatalogPackage& Published);

    /** Makes this pair the one being learned: opens its dictionary, closes the previous language's
        and announces the change. False when nothing installed teaches that language, or cannot
        translate it into that one. */
    bool Open(const std::string& TargetLanguage, const std::string& NativeLanguage);

    bool IsOpenFor(const std::string& TargetLanguage, const std::string& NativeLanguage);

    bool OpenSettingsPair();

    static QStringList GetSearchPaths();

    /** Where a downloaded package belongs. Created on demand by whatever writes there. */
    static QString GetInstalledPackagesPath();

private:
    friend class AppLanguageDictionary;

    AppDictionary() = default;

    AppLanguageDictionary& Find(const std::string& TargetLanguage);

    bool OpenOnDemand(AppLanguageDictionary& Dictionary);

    void Release(AppLanguageDictionary& Dictionary);

    std::map<std::string, std::unique_ptr<AppLanguageDictionary>> Dictionaries;

    std::string ActiveLanguage;

    std::vector<FTDictionaryPackage> Packages;

    std::vector<FTDictionaryPackage> OutdatedPackages;

    bool bDiscovered = false;

    std::recursive_mutex Lock;
};
