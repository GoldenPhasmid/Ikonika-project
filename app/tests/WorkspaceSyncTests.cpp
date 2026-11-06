#include "SyncTestSupport.h"

#include "AppAccountRegistry.h"
#include "AppCredentialStore.h"
#include "AppWorkspace.h"
#include "AppWorkspaceMerger.h"
#include "AppWorkspaceSync.h"

#include "core/FTCard.h"
#include "core/FTCardRepository.h"
#include "core/FTJournal.h"
#include "core/FTSchema.h"

#include <QTemporaryDir>

#include "miniz.h"

#include <cstring>
#include <memory>

namespace
{
    AppAccount MakeAlice()
    {
        AppAccount Account;
        Account.Id = "1001";
        Account.Email = "alice@gmail.com";
        Account.Name = "Alice Martin";

        return Account;
    }

    AppSyncTimings MakeFastTimings()
    {
        AppSyncTimings Timings;
        Timings.QuietPauseMilliseconds = 60000;
        Timings.ForegroundCooldownMilliseconds = 300000;
        Timings.RetryBaseMilliseconds = 30000;
        Timings.MaximumRetries = 3;
        Timings.MaximumPulls = 3;

        return Timings;
    }

    void AddCard(AppWorkspace& Workspace, const std::string& Word)
    {

        FTCard Card;
        Card.TargetWord = Word;
        Card.Translation = Word + "-translation";

        REQUIRE(Workspace.GetCardRepository().AddCard(Card).has_value());
    }

    int CountCardsIn(const QByteArray& DatabaseBytes)
    {
        QTemporaryDir Folder;
        const QString Path{Folder.filePath("inspected.db")};

        QFile File(Path);
        REQUIRE(File.open(QIODevice::WriteOnly));
        File.write(DatabaseBytes);
        File.close();

        std::optional<FTDatabase> Opened{FTDatabase::Open(Path.toStdString())};
        REQUIRE(Opened.has_value());

        FTStatement Query{Opened->Prepare("SELECT COUNT(*) FROM Cards")};
        REQUIRE(Query.IsValid());
        REQUIRE(Query.Step());

        return Query.GetInt(0);
    }

    QByteArray MakeDatabaseBytes(const std::vector<std::string>& Words,
                                 int SchemaVersion = CurrentSchemaVersion)
    {
        QTemporaryDir Folder;
        const AppWorkspaceFolder CopyFolder{Folder.filePath("cloud")};

        {
            const std::unique_ptr<AppWorkspace> Copy{AppWorkspace::Open(CopyFolder)};
            REQUIRE(Copy != nullptr);

            for (const std::string& Word : Words)
            {
                AddCard(*Copy, Word);
            }

            REQUIRE(Copy->GetConnection().Execute("PRAGMA user_version = "
                                                  + std::to_string(SchemaVersion)));
        }

        QFile File(CopyFolder.GetDatabaseFilePath());
        REQUIRE(File.open(QIODevice::ReadOnly));

        return File.readAll();
    }

    const char* DatabaseEntryName = "Ikonika.db";

    QByteArray MakeArchive(const QByteArray& DatabaseBytes)
    {
        mz_zip_archive Zip;
        std::memset(&Zip, 0, sizeof(Zip));

        REQUIRE(mz_zip_writer_init_heap(&Zip, 0, 0));
        REQUIRE(mz_zip_writer_add_mem(&Zip, DatabaseEntryName, DatabaseBytes.constData(),
                                      static_cast<size_t>(DatabaseBytes.size()),
                                      static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION)));

        void* Buffer{nullptr};
        size_t Size{0};
        REQUIRE(mz_zip_writer_finalize_heap_archive(&Zip, &Buffer, &Size));

        const QByteArray Archive(static_cast<const char*>(Buffer), static_cast<qsizetype>(Size));
        mz_zip_writer_end(&Zip);

        return Archive;
    }

    QByteArray ReadArchiveEntry(const QByteArray& Archive, const char* EntryName)
    {
        mz_zip_archive Zip;
        std::memset(&Zip, 0, sizeof(Zip));

        if (!mz_zip_reader_init_mem(&Zip, Archive.constData(), static_cast<size_t>(Archive.size()), 0))
        {
            return {};
        }

        size_t Size{0};
        void* Buffer{mz_zip_reader_extract_file_to_heap(&Zip, EntryName, &Size, 0)};
        QByteArray Entry;

        if (Buffer != nullptr)
        {
            Entry = QByteArray(static_cast<const char*>(Buffer), static_cast<qsizetype>(Size));
            mz_free(Buffer);
        }

        mz_zip_reader_end(&Zip);

        return Entry;
    }

    QByteArray MakeCloudCopy(const std::vector<std::string>& Words,
                             int SchemaVersion = CurrentSchemaVersion)
    {
        return MakeArchive(MakeDatabaseBytes(Words, SchemaVersion));
    }

    bool IsPlainDatabase(const QByteArray& Bytes)
    {
        return Bytes.startsWith(QByteArrayLiteral("SQLite format 3"));
    }

    struct FSyncHarness
    {
        explicit FSyncHarness(bool bSignedIn = true)
        {
            if (bSignedIn)
            {
                REQUIRE(Registry.SignIn(MakeAlice(), "alice-token"));
            }

            Workspace = AppWorkspace::Open(Registry.GetActiveFolder());
            REQUIRE(Workspace != nullptr);

            Timings.Clock = [this] { return FakeNow; };
        }

        QTimer& QuietTimer()
        {
            QTimer* Timer{Sync->findChild<QTimer*>(QStringLiteral("quietTimer"))};
            REQUIRE(Timer != nullptr);

            return *Timer;
        }

        QTimer& RetryTimer()
        {
            QTimer* Timer{Sync->findChild<QTimer*>(QStringLiteral("retryTimer"))};
            REQUIRE(Timer != nullptr);

            return *Timer;
        }

        void Fire(QTimer& Timer)
        {
            REQUIRE(Timer.isActive());
            Timer.stop();
            REQUIRE(QMetaObject::invokeMethod(&Timer, "timeout"));
        }

        void UseRealMerger()
        {
            RealMerger = std::make_unique<AppWorkspaceMerger>(Workspace->GetSettings());
            ActiveMerger = RealMerger.get();
        }

        void StartSync()
        {
            Sync = std::make_unique<AppWorkspaceSync>(*Workspace, Registry, Store, *ActiveMerger, Timings);

            QObject::connect(Sync.get(), &AppWorkspaceSync::cycleFinished, Sync.get(),
                             [this](bool bSuccess) { Finished.push_back(bSuccess); });
            QObject::connect(Sync.get(), &AppWorkspaceSync::dataMerged, Sync.get(),
                             [this] { ++MergeNotices; });
        }

        void FinishWithNoCloudCopy()
        {
            Store.FinishFind(std::nullopt, std::nullopt);
            Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "1"});
        }

        QTemporaryDir Root;
        AppMemoryCredentialStore Credentials;
        AppAccountRegistry Registry{Root.path(), Credentials};
        std::unique_ptr<AppWorkspace> Workspace;
        FFakeCloudStore Store;
        FFakeMerger Merger;
        std::unique_ptr<AppWorkspaceMerger> RealMerger;
        AppSyncMerger* ActiveMerger = &Merger;
        AppSyncTimings Timings{MakeFastTimings()};
        std::unique_ptr<AppWorkspaceSync> Sync;
        std::vector<bool> Finished;
        int MergeNotices = 0;
        qint64 FakeNow = 1000;
    };
}

TEST_CASE("With no cloud copy, the local database goes up as a new copy", "[sync]")
{
    FSyncHarness Harness;
    AddCard(*Harness.Workspace, "chien");
    Harness.StartSync();

    Harness.Sync->SyncNow();
    REQUIRE(Harness.Store.FindCount == 1);

    Harness.Store.FinishFind(std::nullopt, std::nullopt);

    REQUIRE(Harness.Store.UploadedIds.size() == 1);
    CHECK(Harness.Store.UploadedIds.front().isEmpty());
    CHECK_FALSE(IsPlainDatabase(Harness.Store.UploadedBytes));
    CHECK(CountCardsIn(ReadArchiveEntry(Harness.Store.UploadedBytes, DatabaseEntryName)) == 1);
    CHECK(Harness.Merger.MergeCount == 0);

    Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "1"});

    CHECK(Harness.Finished == std::vector<bool>{true});
}

TEST_CASE("A cycle announces its start once, and a request during it announces nothing", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    int Started{0};
    QObject::connect(Harness.Sync.get(), &AppWorkspaceSync::cycleStarted, [&Started] { ++Started; });

    Harness.Sync->SyncNow();
    Harness.Sync->SyncNow();

    CHECK(Started == 1);
    CHECK(Harness.Sync->IsRunning());
}

TEST_CASE("An existing cloud copy is downloaded, merged, checked and replaced", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});

    REQUIRE(Harness.Store.DownloadedIds == std::vector<QString>{"copy-1"});

    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat", "souris"}));

    CHECK(Harness.Merger.MergeCount == 1);
    CHECK(Harness.Merger.DownloadedCardCount == 2);
    CHECK(Harness.MergeNotices == 1);
    REQUIRE(Harness.Store.VersionCount == 1);

    Harness.Store.FinishVersion(std::nullopt, "5");

    REQUIRE(Harness.Store.UploadedIds == std::vector<QString>{"copy-1"});

    Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "6"});

    CHECK(Harness.Finished == std::vector<bool>{true});
}

TEST_CASE("A copy downloaded while merges are held merges once they are allowed", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();
    Harness.Sync->HoldMerges(true);

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));

    CHECK(Harness.Merger.MergeCount == 0);
    CHECK(Harness.MergeNotices == 0);
    CHECK(Harness.Sync->IsRunning());

    Harness.Sync->HoldMerges(false);
    SpinFor(20);

    CHECK(Harness.Merger.MergeCount == 1);
    CHECK(Harness.MergeNotices == 1);
    CHECK(Harness.Store.VersionCount == 1);
}

TEST_CASE("A held copy is dropped when the sync stops", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();
    Harness.Sync->HoldMerges(true);

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));

    Harness.Sync->Stop();
    Harness.Sync->HoldMerges(false);
    SpinFor(20);

    CHECK(Harness.Merger.MergeCount == 0);
    CHECK(Harness.MergeNotices == 0);
    CHECK_FALSE(Harness.Sync->IsRunning());
}

TEST_CASE("A plain database left by the old sync is still read", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, MakeDatabaseBytes({"chat", "souris"}));

    CHECK(Harness.Merger.MergeCount == 1);
    CHECK(Harness.Merger.DownloadedCardCount == 2);
}

TEST_CASE("The merged database goes up inside an archive", "[sync]")
{
    FSyncHarness Harness;
    AddCard(*Harness.Workspace, "chien");
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));
    Harness.Store.FinishVersion(std::nullopt, "5");

    REQUIRE(Harness.Store.UploadedIds == std::vector<QString>{"copy-1"});
    CHECK_FALSE(IsPlainDatabase(Harness.Store.UploadedBytes));
    CHECK(CountCardsIn(ReadArchiveEntry(Harness.Store.UploadedBytes, DatabaseEntryName)) == 1);
}

TEST_CASE("A cloud copy that moved during the merge is pulled again before the upload", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));
    Harness.Store.FinishVersion(std::nullopt, "6");

    CHECK(Harness.Store.DownloadedIds.size() == 2);
    CHECK(Harness.Store.UploadedIds.empty());

    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat", "souris"}));
    Harness.Store.FinishVersion(std::nullopt, "6");

    CHECK(Harness.Merger.MergeCount == 2);
    CHECK(Harness.Store.UploadedIds == std::vector<QString>{"copy-1"});
}

TEST_CASE("A cloud copy that keeps moving ends the cycle without an upload", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();
    const QByteArray CloudCopy{MakeCloudCopy({"chat"})};

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});

    for (int Pull = 0; Pull < Harness.Timings.MaximumPulls; ++Pull)
    {
        Harness.Store.FinishDownload(std::nullopt, CloudCopy);
        Harness.Store.FinishVersion(std::nullopt, QString::number(6 + Pull));
    }

    CHECK(Harness.Store.DownloadedIds.size() == static_cast<size_t>(Harness.Timings.MaximumPulls));
    CHECK(Harness.Store.UploadedIds.empty());
    CHECK(Harness.Finished == std::vector<bool>{false});
}

TEST_CASE("An offline download ends the cycle with no merge and no upload", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(MakeOffline());

    CHECK(Harness.Merger.MergeCount == 0);
    CHECK(Harness.Store.UploadedIds.empty());
    CHECK(Harness.Finished == std::vector<bool>{false});
}

TEST_CASE("A failed merge uploads nothing and is retried", "[sync]")
{
    FSyncHarness Harness;
    Harness.Merger.bSucceed = false;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));

    CHECK(Harness.Store.UploadedIds.empty());
    CHECK(Harness.Finished == std::vector<bool>{false});

    Harness.Fire(Harness.RetryTimer());

    CHECK(Harness.Store.FindCount == 2);
}

TEST_CASE("A refused credential marks the account and keeps its workspace open", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(MakeRefused(), std::nullopt);

    REQUIRE(Harness.Registry.GetActiveAccount().has_value());
    CHECK(Harness.Registry.GetActiveAccount()->Id == "1001");
    CHECK(Harness.Registry.GetActiveAccount()->bNeedsReconnect);
    CHECK(Harness.Finished == std::vector<bool>{false});

    CHECK_FALSE(Harness.RetryTimer().isActive());

    CHECK(Harness.Store.FindCount == 1);
}

TEST_CASE("The signed-out workspace never syncs", "[sync]")
{
    FSyncHarness Harness{false};
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Sync->OnBackground();
    Harness.Sync->OnNetworkBack();

    CHECK(Harness.Store.FindCount == 0);
}

TEST_CASE("An account that needs reconnect never syncs", "[sync]")
{
    FSyncHarness Harness;
    Harness.Registry.MarkNeedsReconnect("1001");
    Harness.StartSync();

    Harness.Sync->SyncNow();

    CHECK(Harness.Store.FindCount == 0);
}

TEST_CASE("An archive without the database is left alone and not retried", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    mz_zip_archive Zip;
    std::memset(&Zip, 0, sizeof(Zip));
    REQUIRE(mz_zip_writer_init_heap(&Zip, 0, 0));
    REQUIRE(mz_zip_writer_add_mem(&Zip, "notes.txt", "hello", 5, MZ_DEFAULT_COMPRESSION));
    void* Buffer{nullptr};
    size_t Size{0};
    REQUIRE(mz_zip_writer_finalize_heap_archive(&Zip, &Buffer, &Size));
    const QByteArray Archive(static_cast<const char*>(Buffer), static_cast<qsizetype>(Size));
    mz_zip_writer_end(&Zip);

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, Archive);

    CHECK(Harness.Merger.MergeCount == 0);
    CHECK(Harness.Store.UploadedIds.empty());
    CHECK(Harness.Finished == std::vector<bool>{false});

    CHECK_FALSE(Harness.RetryTimer().isActive());

    CHECK(Harness.Store.FindCount == 1);
}

TEST_CASE("A cloud copy that is not a database is replaced by this workspace", "[sync]")
{
    FSyncHarness Harness;
    AddCard(*Harness.Workspace, "chien");
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, "this is not a database");

    CHECK(Harness.Merger.MergeCount == 0);
    CHECK(Harness.MergeNotices == 0);
    REQUIRE(Harness.Store.VersionCount == 1);

    Harness.Store.FinishVersion(std::nullopt, "5");

    REQUIRE(Harness.Store.UploadedIds == std::vector<QString>{"copy-1"});
    CHECK(CountCardsIn(ReadArchiveEntry(Harness.Store.UploadedBytes, DatabaseEntryName)) == 1);

    Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "6"});

    CHECK(Harness.Finished == std::vector<bool>{true});
    CHECK_FALSE(Harness.Sync->HasUnsentWrites());
}

TEST_CASE("A cloud copy that fails the integrity check is replaced by this workspace", "[sync]")
{
    FSyncHarness Harness;
    AddCard(*Harness.Workspace, "chien");
    Harness.StartSync();

    QByteArray Corrupted{MakeDatabaseBytes({"chat", "souris", "cheval"})};
    REQUIRE(Corrupted.size() > 8192);
    Corrupted.replace(4096, 4096, QByteArray(4096, '\xff'));

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, MakeArchive(Corrupted));

    CHECK(Harness.Merger.MergeCount == 0);
    REQUIRE(Harness.Store.VersionCount == 1);

    Harness.Store.FinishVersion(std::nullopt, "5");

    REQUIRE(Harness.Store.UploadedIds == std::vector<QString>{"copy-1"});
    CHECK(CountCardsIn(ReadArchiveEntry(Harness.Store.UploadedBytes, DatabaseEntryName)) == 1);
}

TEST_CASE("A damaged cloud copy fixed by another device meanwhile is pulled again, not replaced", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, "this is not a database");
    Harness.Store.FinishVersion(std::nullopt, "6");

    CHECK(Harness.Store.UploadedIds.empty());
    CHECK(Harness.Store.DownloadedIds.size() == 2);

    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));

    CHECK(Harness.Merger.MergeCount == 1);
}

TEST_CASE("A cloud copy from a newer app is left alone and not retried", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}, CurrentSchemaVersion + 1));

    CHECK(Harness.Merger.MergeCount == 0);
    CHECK(Harness.Store.UploadedIds.empty());
    CHECK(Harness.Store.VersionCount == 0);
    CHECK(Harness.Finished == std::vector<bool>{false});

    CHECK_FALSE(Harness.RetryTimer().isActive());

    CHECK(Harness.Store.FindCount == 1);
}

TEST_CASE("Stopping abandons the cycle even when the download finishes afterward", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});

    Harness.Sync->Stop();

    CHECK(Harness.Store.CancelCount >= 1);
    CHECK_FALSE(Harness.Sync->IsRunning());

    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));

    CHECK(Harness.Merger.MergeCount == 0);
    CHECK(Harness.Store.VersionCount == 0);
    CHECK(Harness.Store.UploadedIds.empty());
}

TEST_CASE("Asking to sync during a cycle runs exactly one more cycle afterward", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Sync->SyncNow();
    Harness.Sync->SyncNow();

    CHECK(Harness.Store.FindCount == 1);

    Harness.FinishWithNoCloudCopy();

    REQUIRE(WaitUntil([&Harness] { return Harness.Store.FindCount == 2; }, 1000));

    Harness.FinishWithNoCloudCopy();

    CHECK_FALSE(Harness.QuietTimer().isActive());
    CHECK_FALSE(Harness.RetryTimer().isActive());
    CHECK(Harness.Store.FindCount == 2);
}

TEST_CASE("Writes made by the merge itself do not start another cycle", "[sync]")
{
    FSyncHarness Harness;
    Harness.Merger.WriteDuringMerge = [](FTDatabase& Local)
    {
        REQUIRE(Local.Execute("UPDATE Cards SET TargetWord = TargetWord"));
    };
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));
    Harness.Store.FinishVersion(std::nullopt, "5");
    Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "6"});

    CHECK_FALSE(Harness.QuietTimer().isActive());
    CHECK(Harness.Store.FindCount == 1);
    CHECK_FALSE(Harness.Sync->HasUnsentWrites());
}

TEST_CASE("Local writes start one cycle after the quiet pause", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    AddCard(*Harness.Workspace, "chien");
    AddCard(*Harness.Workspace, "chat");
    AddCard(*Harness.Workspace, "souris");

    CHECK(Harness.Sync->HasUnsentWrites());
    CHECK(Harness.Store.FindCount == 0);

    CHECK(Harness.QuietTimer().interval() == Harness.Timings.QuietPauseMilliseconds);

    Harness.Fire(Harness.QuietTimer());

    CHECK(Harness.Store.FindCount == 1);
    CHECK_FALSE(Harness.QuietTimer().isActive());
}

TEST_CASE("Going to the background syncs at once only when writes are unsent", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->OnBackground();

    CHECK(Harness.Store.FindCount == 0);

    AddCard(*Harness.Workspace, "chien");
    Harness.Sync->OnBackground();

    CHECK(Harness.Store.FindCount == 1);
}

TEST_CASE("A finished upload leaves nothing unsent", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    AddCard(*Harness.Workspace, "chien");
    Harness.Sync->SyncNow();
    Harness.FinishWithNoCloudCopy();

    CHECK_FALSE(Harness.Sync->HasUnsentWrites());

    Harness.Sync->OnBackground();

    CHECK(Harness.Store.FindCount == 1);
}

TEST_CASE("A write made while a cycle runs is still unsent after it", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, std::nullopt);

    AddCard(*Harness.Workspace, "chien");

    Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "1"});

    CHECK(Harness.Sync->HasUnsentWrites());
}

TEST_CASE("Returning to the foreground syncs only after the cooldown", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.FinishWithNoCloudCopy();

    Harness.Sync->OnForeground();

    CHECK(Harness.Store.FindCount == 1);

    Harness.FakeNow += Harness.Timings.ForegroundCooldownMilliseconds + 1;
    Harness.Sync->OnForeground();

    CHECK(Harness.Store.FindCount == 2);
}

TEST_CASE("An offline cycle retries a limited number of times, waiting longer each time", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();

    for (int Retry = 0; Retry < Harness.Timings.MaximumRetries; ++Retry)
    {
        Harness.Store.FinishFind(MakeOffline(), std::nullopt);

        REQUIRE(Harness.RetryTimer().isActive());

        const int Base{Harness.Timings.RetryBaseMilliseconds << Retry};
        CHECK(Harness.RetryTimer().interval() >= Base);
        CHECK(Harness.RetryTimer().interval() < Base + Base / 4);

        Harness.Fire(Harness.RetryTimer());

        CHECK(Harness.Store.FindCount == Retry + 2);
    }

    Harness.Store.FinishFind(MakeOffline(), std::nullopt);

    CHECK_FALSE(Harness.RetryTimer().isActive());
}

TEST_CASE("The network coming back after a failed cycle syncs at once", "[sync]")
{
    FSyncHarness Harness;
    Harness.Timings.RetryBaseMilliseconds = 60000;
    Harness.StartSync();

    Harness.Sync->OnNetworkBack();

    CHECK(Harness.Store.FindCount == 0);

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(MakeOffline(), std::nullopt);
    Harness.Sync->OnNetworkBack();

    CHECK(Harness.Store.FindCount == 2);
}

TEST_CASE("An empty account workspace waits for its first cycle before setup", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    CHECK(Harness.Sync->IsRestoring());

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));

    CHECK(Harness.Sync->IsRestoring());

    Harness.Store.FinishVersion(std::nullopt, "5");
    Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "6"});

    CHECK_FALSE(Harness.Sync->IsRestoring());
}

TEST_CASE("An offline first cycle ends the wait with the workspace still empty", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(MakeOffline(), std::nullopt);

    CHECK_FALSE(Harness.Sync->IsRestoring());
}

TEST_CASE("A workspace that already has cards does not wait", "[sync]")
{
    FSyncHarness Harness;
    AddCard(*Harness.Workspace, "chien");
    Harness.StartSync();

    CHECK_FALSE(Harness.Sync->IsRestoring());
}

TEST_CASE("The signed-out workspace never waits for a cloud copy", "[sync]")
{
    FSyncHarness Harness{false};
    Harness.StartSync();

    CHECK_FALSE(Harness.Sync->IsRestoring());
}

TEST_CASE("Writes left unsent when the app is killed are still unsent after a restart", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    AddCard(*Harness.Workspace, "chien");
    Harness.Sync.reset();

    Harness.StartSync();

    CHECK(Harness.Sync->HasUnsentWrites());
}

TEST_CASE("Writes left unsent by a killed app go up after the quiet pause of the next start", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    AddCard(*Harness.Workspace, "chien");
    Harness.Sync.reset();

    Harness.StartSync();

    CHECK(Harness.Store.FindCount == 0);

    Harness.Fire(Harness.QuietTimer());

    CHECK(Harness.Store.FindCount == 1);
}

TEST_CASE("Returning to the foreground right after a restart with unsent writes syncs at once", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    AddCard(*Harness.Workspace, "chien");
    Harness.Sync.reset();

    Harness.StartSync();
    Harness.Sync->OnForeground();

    CHECK(Harness.Store.FindCount == 1);
}

TEST_CASE("A restart after everything was sent has nothing unsent and starts no cycle", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    AddCard(*Harness.Workspace, "chien");
    Harness.Sync->SyncNow();
    Harness.FinishWithNoCloudCopy();
    REQUIRE(Harness.Finished == std::vector<bool>{true});

    Harness.Sync.reset();
    Harness.StartSync();

    CHECK_FALSE(Harness.Sync->HasUnsentWrites());
    CHECK_FALSE(Harness.QuietTimer().isActive());

    CHECK(Harness.Store.FindCount == 1);
}

TEST_CASE("A write during the upload is still unsent after a restart", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    AddCard(*Harness.Workspace, "chien");
    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, std::nullopt);
    AddCard(*Harness.Workspace, "chat");
    Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "1"});

    Harness.Sync.reset();
    Harness.StartSync();

    CHECK(Harness.Sync->HasUnsentWrites());
}

TEST_CASE("Every cloud copy is merged and the extra ones are deleted after the upload", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFindAll(std::nullopt, {AppCloudCopy{"copy-1", "5"}, AppCloudCopy{"copy-2", "3"}});
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));

    REQUIRE(Harness.Store.DownloadedIds == std::vector<QString>{"copy-1", "copy-2"});

    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"souris"}));

    CHECK(Harness.Merger.MergeCount == 2);
    REQUIRE(Harness.Store.VersionCount == 1);

    Harness.Store.FinishVersion(std::nullopt, "5");

    REQUIRE(Harness.Store.UploadedIds == std::vector<QString>{"copy-1"});
    CHECK(Harness.Store.DeletedIds.empty());

    Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "6"});

    REQUIRE(Harness.Store.DeletedIds == std::vector<QString>{"copy-2"});
    CHECK(Harness.Finished.empty());

    Harness.Store.FinishDelete(std::nullopt);

    CHECK(Harness.Finished == std::vector<bool>{true});
}

TEST_CASE("An extra copy that cannot be deleted does not fail the sync", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFindAll(std::nullopt, {AppCloudCopy{"copy-1", "5"}, AppCloudCopy{"copy-2", "3"}});
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"souris"}));
    Harness.Store.FinishVersion(std::nullopt, "5");
    Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "6"});
    Harness.Store.FinishDelete(MakeOffline());

    CHECK(Harness.Finished == std::vector<bool>{true});
}

TEST_CASE("An extra copy from a newer app is neither merged nor deleted", "[sync]")
{
    FSyncHarness Harness;
    Harness.StartSync();

    Harness.Sync->SyncNow();
    Harness.Store.FinishFindAll(std::nullopt, {AppCloudCopy{"copy-1", "5"}, AppCloudCopy{"copy-2", "3"}});
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"chat"}));
    Harness.Store.FinishDownload(std::nullopt, MakeCloudCopy({"souris"}, CurrentSchemaVersion + 1));

    CHECK(Harness.Merger.MergeCount == 1);

    Harness.Store.FinishVersion(std::nullopt, "5");
    Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "6"});

    CHECK(Harness.Store.DeletedIds.empty());
    CHECK(Harness.Finished == std::vector<bool>{true});
}

namespace
{
    std::string AddLearnedCard(AppWorkspace& Workspace, const std::string& Word, FTDayNumber Day)
    {

        FTCard Card;
        Card.TargetWord = Word;
        Card.Translation = Word + "-translation";

        const std::optional<std::string> CardId{Workspace.GetCardRepository().AddCard(Card)};
        REQUIRE(CardId.has_value());
        REQUIRE(Workspace.GetCardRepository().RecordProgressEvent(*CardId, JournalLearned, 0, Day));

        return *CardId;
    }

    int ReadStage(AppWorkspace& Workspace, const std::string& CardId)
    {
        FTStatement Query{Workspace.GetConnection().Prepare("SELECT LeitnerStage FROM Cards WHERE Id = ?1")};
        REQUIRE(Query.IsValid());

        Query.BindText(1, CardId);
        REQUIRE(Query.Step());

        return Query.GetInt(0);
    }

    QByteArray MakeCopyBeforeTheJournal(const std::string& Word, int Stage, std::string& CardId)
    {
        QTemporaryDir Folder;
        const AppWorkspaceFolder CopyFolder{Folder.filePath("cloud")};
        const FTDayNumber Today{GetTodayDayNumber()};

        {
            const std::unique_ptr<AppWorkspace> Copy{AppWorkspace::Open(CopyFolder)};
            REQUIRE(Copy != nullptr);

            FTCard Card;
            Card.TargetWord = Word;
            Card.Translation = Word + "-translation";

            const std::optional<std::string> Added{Copy->GetCardRepository().AddCard(Card)};
            REQUIRE(Added.has_value());
            CardId = *Added;

            FTCard Placed{**Copy->GetCardRepository().LoadCard(CardId)};
            Placed.LeitnerStage = Stage;
            Placed.BoxChangedDate = Today - 1;
            Placed.DueDate = Today + 3;
            REQUIRE(Copy->GetCardRepository().UpdateCardSchedule(Placed));

            REQUIRE(Copy->GetConnection().Execute("DELETE FROM Journal"));
            REQUIRE(Copy->GetConnection().Execute("PRAGMA user_version = 29"));
        }

        QFile File(CopyFolder.GetDatabaseFilePath());
        REQUIRE(File.open(QIODevice::ReadOnly));

        return File.readAll();
    }
}

TEST_CASE("A cloud copy made before the journal keeps its progress through the real merger", "[sync][merge]")
{
    FSyncHarness Harness;
    Harness.UseRealMerger();
    Harness.StartSync();

    std::string CardId;
    const QByteArray OldCopy{MakeCopyBeforeTheJournal("chat", 3, CardId)};

    Harness.Sync->SyncNow();
    Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
    Harness.Store.FinishDownload(std::nullopt, OldCopy);
    Harness.Store.FinishVersion(std::nullopt, "5");
    Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "6"});

    REQUIRE(Harness.Finished == std::vector<bool>{true});
    CHECK(ReadStage(*Harness.Workspace, CardId) == 3);
    CHECK(CountCardsIn(ReadArchiveEntry(Harness.Store.UploadedBytes, DatabaseEntryName)) == 1);
}

TEST_CASE("Two workspaces reach the same cards and progress through the cloud", "[sync][merge]")
{
    const FTDayNumber Today{GetTodayDayNumber()};

    FSyncHarness Phone;
    Phone.UseRealMerger();
    const std::string CardId{AddLearnedCard(*Phone.Workspace, "chien", Today - 1)};
    Phone.StartSync();

    Phone.Sync->SyncNow();
    Phone.Store.FinishFind(std::nullopt, std::nullopt);
    Phone.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "1"});
    const QByteArray FromPhone{Phone.Store.UploadedBytes};

    FSyncHarness Desktop;
    Desktop.UseRealMerger();
    Desktop.StartSync();

    Desktop.Sync->SyncNow();
    Desktop.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "1"});
    Desktop.Store.FinishDownload(std::nullopt, FromPhone);

    REQUIRE(ReadStage(*Desktop.Workspace, CardId) == FirstLadderStage);
    REQUIRE(Desktop.Workspace->GetCardRepository().RecordProgressEvent(CardId, JournalReviewed, 1, Today));

    Desktop.Store.FinishVersion(std::nullopt, "1");
    Desktop.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "2"});
    REQUIRE(Desktop.Finished == std::vector<bool>{true});
    const QByteArray FromDesktop{Desktop.Store.UploadedBytes};

    Phone.Sync->SyncNow();
    Phone.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "2"});
    Phone.Store.FinishDownload(std::nullopt, FromDesktop);

    CHECK(ReadStage(*Phone.Workspace, CardId) == 2);
    CHECK(ReadStage(*Desktop.Workspace, CardId) == 2);
}


namespace
{
    QByteArray MakeOldCloudCopy(const std::string& Word, int SchemaVersion,
                                const std::vector<std::string>& OldTableStatements)
    {
        QTemporaryDir Folder;
        const AppWorkspaceFolder CopyFolder{Folder.filePath("cloud")};

        {
            const std::unique_ptr<AppWorkspace> Copy{AppWorkspace::Open(CopyFolder)};
            REQUIRE(Copy != nullptr);

            AddCard(*Copy, Word);

            for (const std::string& Statement : OldTableStatements)
            {
                REQUIRE(Copy->GetConnection().Execute(Statement));
            }

            REQUIRE(Copy->GetConnection().Execute("PRAGMA user_version = " + std::to_string(SchemaVersion)));
        }

        QFile File(CopyFolder.GetDatabaseFilePath());
        REQUIRE(File.open(QIODevice::ReadOnly));

        return File.readAll();
    }

    bool HasLiveCard(AppWorkspace& Workspace, const std::string& Word)
    {
        FTStatement Query{Workspace.GetConnection().Prepare("SELECT 1 FROM Cards WHERE TargetWord = ?1 AND Deleted = 0")};
        REQUIRE(Query.IsValid());

        Query.BindText(1, Word);

        return Query.Step();
    }

    void SyncWithOldCloudCopy(FSyncHarness& Harness, const QByteArray& OldCopy)
    {
        Harness.UseRealMerger();
        Harness.StartSync();

        Harness.Sync->SyncNow();
        Harness.Store.FinishFind(std::nullopt, AppCloudCopy{"copy-1", "5"});
        Harness.Store.FinishDownload(std::nullopt, OldCopy);
        Harness.Store.FinishVersion(std::nullopt, "5");
        Harness.Store.FinishUpload(std::nullopt, AppCloudCopy{"copy-1", "6"});
    }
}

TEST_CASE("A cloud copy that still has the reader tables is upgraded and merged", "[sync][merge]")
{
    FSyncHarness Harness;
    const QByteArray OldCopy{MakeOldCloudCopy("chat", 27,
                                              {"CREATE TABLE ReaderDocuments (Id TEXT PRIMARY KEY)",
                                               "CREATE TABLE YoutubeVideos (Id TEXT PRIMARY KEY)"})};

    SyncWithOldCloudCopy(Harness, OldCopy);

    REQUIRE(Harness.Finished == std::vector<bool>{true});
    CHECK(HasLiveCard(*Harness.Workspace, "chat"));
    CHECK(CountCardsIn(ReadArchiveEntry(Harness.Store.UploadedBytes, DatabaseEntryName)) == 1);
}

TEST_CASE("A cloud copy that still has decks is upgraded and merged", "[sync][merge]")
{
    FSyncHarness Harness;
    const QByteArray OldCopy{MakeOldCloudCopy(
        "chat", 31,
        {"CREATE TABLE Decks (Id TEXT PRIMARY KEY, TargetLanguage TEXT NOT NULL DEFAULT '', "
         "NativeLanguage TEXT NOT NULL DEFAULT '', Role TEXT NOT NULL DEFAULT '', Deleted INTEGER NOT NULL DEFAULT 0)",
         "CREATE TABLE DeckCards (DeckId TEXT NOT NULL, CardId TEXT NOT NULL, Deleted INTEGER NOT NULL DEFAULT 0)",
         "INSERT INTO Decks (Id, TargetLanguage, NativeLanguage, Role) VALUES ('deck-1', 'fr', 'en', 'pool')",
         "INSERT INTO DeckCards (DeckId, CardId) SELECT 'deck-1', Id FROM Cards"})};

    SyncWithOldCloudCopy(Harness, OldCopy);

    REQUIRE(Harness.Finished == std::vector<bool>{true});
    CHECK(HasLiveCard(*Harness.Workspace, "chat"));
    CHECK(CountCardsIn(ReadArchiveEntry(Harness.Store.UploadedBytes, DatabaseEntryName)) == 1);
}
