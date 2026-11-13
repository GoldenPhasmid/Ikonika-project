#pragma once

#include "core/FTCatalog.h"
#include "core/FTDatabase.h"
#include "core/FTDictionaryPackage.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTSentenceTree.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
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
};

/**
 * Handles installed dictionaries and active connection
 */
class AppDictionary
{
public:
    static AppDictionary& Get();

    /** Emits languagesChanged whenever Open swaps in a different dictionary. */
    static AppDictionaryNotifier& Notifier();

    /**
     * Scans for dictionaries and opens the one matching the saved language settings. Falls back to
     * the first installed dictionary when the saved pair is not among them, which is what happens
     * when a package is removed.
     */
    bool Initialise();

    /** Rescans without changing which dictionary is open. For after a package is added or removed. */
    void Discover();

    /** Releases the open dictionary file. Needed before a package replaces the file on disk, since
        the connection would otherwise be reading a file being rewritten underneath it. */
    void Close();

    const std::vector<FTDictionaryPackage>& GetPackages() const;

    const std::vector<FTDictionaryPackage>& GetOutdatedPackages() const;

    bool IsOutdated(const std::string& TargetLanguage, const std::string& NativeLanguage) const;

    bool IsInstalled(const std::string& TargetLanguage, const std::string& NativeLanguage) const;

    bool IsReplacedBy(const FTCatalogPackage& Published) const;

    /** Nothing when no dictionary is open. */
    const FTDictionaryPackage* GetActivePackage() const;

    /** Closes whatever is open and opens the dictionary for this pair. False when nothing installed
        teaches that language, or cannot translate it into that one. */
    bool Open(const std::string& TargetLanguage, const std::string& NativeLanguage);

    bool IsOpenFor(const std::string& TargetLanguage, const std::string& NativeLanguage) const;

    bool OpenSettingsPair();

    bool IsReady();

    uint64_t GetGeneration() const;

    /** Only valid once a dictionary is open. */
    FTDictionaryRepository& GetRepository();

    const FTSentenceTree* GetSentenceTree();

    /** The tree when it is already loaded, so a background task can read it without loading a second copy. */
    std::shared_ptr<const FTSentenceTree> GetLoadedSentenceTree() const;

    /** Keeps a tree a background task loaded, unless the dictionary changed meanwhile or the tree is already loaded. */
    void AdoptSentenceTree(std::shared_ptr<const FTSentenceTree> Tree, uint64_t ForGeneration);

    static QStringList GetSearchPaths();

    /** Where a downloaded package belongs. Created on demand by whatever writes there. */
    static QString GetInstalledPackagesPath();

private:
    AppDictionary() = default;

    std::unique_ptr<FTDatabase> Connection;
    std::unique_ptr<FTDictionaryRepository> Repository;

    std::shared_ptr<const FTSentenceTree> SentenceTree;
    bool bSentenceTreeLoaded = false;

    std::vector<FTDictionaryPackage> Packages;

    std::vector<FTDictionaryPackage> OutdatedPackages;

    bool bDiscovered = false;

    /** Index into Packages, or -1 when nothing is open. An index rather than a pointer, since a
        rescan rebuilds the vector. */
    int ActiveIndex = -1;

    std::string OpenNativeLanguage;

    uint64_t Generation = 0;
};
