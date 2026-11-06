#pragma once

#include <QString>

/**
 * Resolves application storage paths based on build configuration.
 *
 * - Debug: Consolidates all data in `./local/` to isolate dev/test data and allow easy resets.
 * - Release: Uses standard OS per-user directories (`AppData/Roaming` for database; `AppData/Local` for dictionaries and logs).
 *
 * Note: `Install()` configures `QSettings` paths prior to execution.
 */
namespace AppPaths
{
    void EnableHeadlessSandbox(bool bKeepDictionaries = false);

    bool IsHeadless();

    bool IsStorePackage();

    void Install();

    QString DatabaseFilePath();
    QString DataRoot();

    /** Where dictionaries the user installed are looked for and put. Not where the bundled ones
        live; those sit beside the executable and are read-only. */
    QString InstalledDictionariesDirectory();

    QString DownloadRoot();

    QString DownloadedModelsDirectory();

    QString DownloadScratchDirectory();

    QString PendingUpdateDirectory();

    /** Where the log file and its predecessors go. */
    QString LogsDirectory();
}
