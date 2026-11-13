#include <catch2/catch_test_macros.hpp>

#include "AppCatalog.h"
#include "AppDictionary.h"
#include "AppPackageDownloader.h"
#include "AppPaths.h"

#include "core/FTDatabase.h"
#include "core/FTDictionaryPackage.h"
#include "core/FTSettings.h"

#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include "miniz.h"

#include <cstring>

namespace
{
    const std::string Language{"nl"};
    const QString PackageId{QStringLiteral("dictionary-nl")};
    const QString DictionaryFileName{QStringLiteral("nl-dictionary.db")};

    void WriteDictionary(const QString& Path, int SchemaVersion)
    {
        QFile::remove(Path);
        REQUIRE(QDir().mkpath(QFileInfo(Path).absolutePath()));

        std::optional<FTDatabase> Target{FTDatabase::Open(Path.toStdString())};
        REQUIRE(Target.has_value());

        REQUIRE(Target->Execute("CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT)"));
        REQUIRE(Target->Execute("CREATE TABLE languages (id INTEGER PRIMARY KEY, code TEXT)"));
        REQUIRE(Target->Execute("CREATE TABLE words (id INTEGER PRIMARY KEY)"));
        REQUIRE(Target->Execute("CREATE TABLE translations (id INTEGER PRIMARY KEY, language_id INTEGER)"));
        REQUIRE(Target->Execute("INSERT INTO metadata VALUES ('schema_version', '"
                                + std::to_string(SchemaVersion) + "')"));
        REQUIRE(Target->Execute("INSERT INTO metadata VALUES ('target_language', '" + Language + "')"));
        REQUIRE(Target->Execute("INSERT INTO languages VALUES (1, 'en')"));
        REQUIRE(Target->Execute("INSERT INTO words VALUES (1)"));
        REQUIRE(Target->Execute("INSERT INTO translations VALUES (1, 1)"));
    }

    void WriteArchive(const QString& ArchivePath, const QString& DictionaryPath)
    {
        mz_zip_archive Zip;
        std::memset(&Zip, 0, sizeof(Zip));

        REQUIRE(mz_zip_writer_init_file(&Zip, ArchivePath.toUtf8().constData(), 0));
        REQUIRE(mz_zip_writer_add_file(&Zip, DictionaryFileName.toUtf8().constData(),
                                       DictionaryPath.toUtf8().constData(), nullptr, 0,
                                       static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION)));
        REQUIRE(mz_zip_writer_finalize_archive(&Zip));
        REQUIRE(mz_zip_writer_end(&Zip));
    }

    void WriteTextArchive(const QString& ArchivePath, const std::vector<std::pair<QString, QByteArray>>& Entries)
    {
        REQUIRE(QDir().mkpath(QFileInfo(ArchivePath).absolutePath()));

        mz_zip_archive Zip;
        std::memset(&Zip, 0, sizeof(Zip));

        REQUIRE(mz_zip_writer_init_file(&Zip, ArchivePath.toUtf8().constData(), 0));

        for (const auto& [Name, Contents] : Entries)
        {
            REQUIRE(mz_zip_writer_add_mem(&Zip, Name.toUtf8().constData(), Contents.constData(),
                                          static_cast<size_t>(Contents.size()),
                                          static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION)));
        }

        REQUIRE(mz_zip_writer_finalize_archive(&Zip));
        REQUIRE(mz_zip_writer_end(&Zip));
    }

    void WriteText(const QString& Path, const QByteArray& Contents)
    {
        REQUIRE(QDir().mkpath(QFileInfo(Path).absolutePath()));

        QFile File{Path};
        REQUIRE(File.open(QIODevice::WriteOnly | QIODevice::Truncate));
        File.write(Contents);
    }

    QByteArray ReadText(const QString& Path)
    {
        QFile File{Path};

        return File.open(QIODevice::ReadOnly) ? File.readAll() : QByteArray();
    }

    bool HasStagingFolder(const QString& StagingRoot)
    {
        return !QDir(StagingRoot).entryList({QStringLiteral("staging-*")}, QDir::Dirs | QDir::NoDotAndDotDot).isEmpty();
    }

    struct FTPublishedPackage
    {
        QString Kind{QStringLiteral("dictionary")};
        QString ApplicationVersion;
        int Version = 1;
        int Schema = DictionarySchemaVersion;
        QString Url{QStringLiteral("https://example.invalid/nl-dictionary.zip")};
        QString Sha256{QString(64, QLatin1Char('a'))};
        qint64 Bytes = 1;
    };

    QByteArray CatalogJson(const FTPublishedPackage& Package)
    {
        const QByteArray Body{QStringLiteral(R"({"schema":1,"packages":[{"id":"%1","kind":"%7","target":"nl",)"
                              R"("natives":["en"],"version":%2,"dictionarySchema":%3,)"
                              R"("file":"nl-dictionary.zip","unpackTo":".","url":"%4",)"
                              R"("bytes":%5,"unpackedBytes":1,"sha256":"%6"}]})")
            .arg(PackageId)
            .arg(Package.Version)
            .arg(Package.Schema)
            .arg(Package.Url)
            .arg(Package.Bytes)
            .arg(Package.Sha256)
            .arg(Package.Kind)
            .toUtf8()};

        if (Package.ApplicationVersion.isEmpty())
        {
            return Body;
        }

        QJsonObject Catalog{QJsonDocument::fromJson(Body).object()};
        Catalog.insert(QStringLiteral("app"), QJsonObject{
            {QStringLiteral("version"), Package.ApplicationVersion},
            {QStringLiteral("url"), QStringLiteral("https://example.invalid/update.exe")},
            {QStringLiteral("sha256"), QString(64, QLatin1Char('a'))}});
        return QJsonDocument{Catalog}.toJson();
    }

    QVariantMap FindLanguage(const std::string& Code)
    {
        for (const QVariant& Entry : AppCatalog::Get().GetLanguages())
        {
            const QVariantMap Row{Entry.toMap()};

            if (Row.value(QStringLiteral("code")).toString().toStdString() == Code)
            {
                return Row;
            }
        }

        return {};
    }

    struct FTDictionaryUpdateFixture
    {
        QTemporaryDir Working;
        FTLanguageRules SavedRules{FTSettings::Get().GetLanguageRules()};

        FTDictionaryUpdateFixture()
        {
            REQUIRE(Working.isValid());

            AppPaths::EnableHeadlessSandbox();

            FTSettings::Get().SetNativeLanguage("en");

            AppDictionary::Get().Close();
        }

        ~FTDictionaryUpdateFixture()
        {
            AppPackageDownloader::Get().forget(PackageId);
            AppDictionary::Get().Close();

            QFile::remove(InstalledPath());

            QSettings Settings;
            Settings.remove(QStringLiteral("packages"));
            Settings.remove(QStringLiteral("update/catalogUrl"));
            Settings.remove(QStringLiteral("update/dismissedAppVersion"));

            FTSettings::Get().SetTargetLanguage(SavedRules.TargetLanguage);
            FTSettings::Get().SetNativeLanguage(SavedRules.NativeLanguage);

            AppDictionary::Get().Discover();
        }

        static QString InstalledPath()
        {
            return QDir(AppPaths::InstalledDictionariesDirectory()).filePath(DictionaryFileName);
        }

        void Install(int SchemaVersion, int RecordedVersion)
        {
            WriteDictionary(InstalledPath(), SchemaVersion);
            AppCatalog::SetInstalledVersion(PackageId, RecordedVersion);
        }

        void LearnThenOutdate(int RecordedVersion)
        {
            Install(DictionarySchemaVersion, RecordedVersion);
            AppDictionary::Get().Discover();
            FTSettings::Get().SetTargetLanguage(Language);
            REQUIRE(FTSettings::Get().GetLanguageRules().TargetLanguage == Language);

            Install(OldestDictionarySchemaVersion - 1, RecordedVersion);
            AppDictionary::Get().Discover();
        }

        bool Publish(const FTPublishedPackage& Package)
        {
            const QString Path{QDir(Working.path()).filePath(QStringLiteral("catalog.json"))};

            QFile File{Path};

            if (!File.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                return false;
            }

            File.write(CatalogJson(Package));
            File.close();

            QSettings().setValue(QStringLiteral("update/catalogUrl"), QUrl::fromLocalFile(Path).toString());

            bool bLoaded = false;
            QEventLoop Loop;

            QObject::connect(&AppCatalog::Get(), &AppCatalog::catalogChanged, &Loop,
                             [&bLoaded, &Loop]()
                             {
                                 bLoaded = true;
                                 Loop.quit();
                             });
            QObject::connect(&AppCatalog::Get(), &AppCatalog::lastErrorChanged, &Loop, &QEventLoop::quit);
            QTimer::singleShot(5000, &Loop, &QEventLoop::quit);

            AppCatalog::Get().Refresh();
            Loop.exec();

            return bLoaded;
        }

        FTPublishedPackage BuildPublishedArchive(int Version)
        {
            const QString Built{QDir(Working.path()).filePath(QStringLiteral("built/") + DictionaryFileName)};
            const QString Archive{QDir(Working.path()).filePath(QStringLiteral("nl-dictionary.zip"))};

            WriteDictionary(Built, DictionarySchemaVersion);
            WriteArchive(Archive, Built);

            QFile File{Archive};
            REQUIRE(File.open(QIODevice::ReadOnly));

            QCryptographicHash Digest{QCryptographicHash::Sha256};
            REQUIRE(Digest.addData(&File));

            FTPublishedPackage Package;
            Package.Version = Version;
            Package.Url = QUrl::fromLocalFile(Archive).toString();
            Package.Sha256 = QString::fromLatin1(Digest.result().toHex());
            Package.Bytes = File.size();

            return Package;
        }
    };

    bool WaitForInstall(const QString& Id)
    {
        bool bInstalled = false;
        QEventLoop Loop;

        QObject::connect(&AppPackageDownloader::Get(), &AppPackageDownloader::packageInstalled, &Loop,
                         [&bInstalled, &Loop, &Id](const QString& Installed)
                         {
                             bInstalled = Installed == Id;
                             Loop.quit();
                         });
        QObject::connect(&AppPackageDownloader::Get(), &AppPackageDownloader::packageFailed, &Loop,
                         [&Loop]() { Loop.quit(); });
        QTimer::singleShot(10000, &Loop, &QEventLoop::quit);

        Loop.exec();

        return bInstalled;
    }
}

TEST_CASE("An installed dictionary stays selectable when the catalog only lists one this app cannot read",
          "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;

    Test.Install(DictionarySchemaVersion, 1);
    AppDictionary::Get().Discover();

    FTPublishedPackage Newer;
    Newer.Version = 2;
    Newer.Schema = DictionarySchemaVersion + 1;

    REQUIRE(Test.Publish(Newer));

    const QVariantMap Row{FindLanguage(Language)};

    REQUIRE_FALSE(Row.isEmpty());
    CHECK(Row.value(QStringLiteral("installed")).toBool());
    CHECK(Row.value(QStringLiteral("compatible")).toBool());
    CHECK_FALSE(Row.value(QStringLiteral("outdated")).toBool());

    CHECK_FALSE(AppPackageDownloader::Get().isPending(PackageId));

    REQUIRE(AppDictionary::Get().Open(Language, "en"));
    CHECK(AppCatalog::Get().GetAvailableDictionaryUpdate().isEmpty());
}

TEST_CASE("An installed dictionary an app update outdated waits for permission to download",
          "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;

    Test.LearnThenOutdate(1);

    REQUIRE(AppDictionary::Get().IsOutdated(Language, "en"));
    REQUIRE(FTSettings::Get().GetLanguageRules().TargetLanguage == Language);

    SECTION("the saved language waits for its update instead of switching to another one")
    {
        CHECK_FALSE(AppDictionary::Get().Initialise());
        CHECK(AppDictionary::Get().GetActivePackage() == nullptr);
        CHECK(FTSettings::Get().GetLanguageRules().TargetLanguage == Language);
    }

    SECTION("the readable package is downloaded, installed and opened")
    {
        const FTPublishedPackage Published{Test.BuildPublishedArchive(2)};

        REQUIRE(Test.Publish(Published));

        const QVariantMap Row{FindLanguage(Language)};

        REQUIRE_FALSE(Row.isEmpty());
        CHECK(Row.value(QStringLiteral("outdated")).toBool());
        CHECK(Row.value(QStringLiteral("compatible")).toBool());
        CHECK_FALSE(Row.value(QStringLiteral("installed")).toBool());

        const QVariantMap Offer{AppCatalog::Get().GetAvailableDictionaryUpdate()};

        CHECK(Offer.value(QStringLiteral("id")).toString() == PackageId);
        CHECK_FALSE(Offer.value(QStringLiteral("dismissed")).toBool());

        CHECK_FALSE(AppPackageDownloader::Get().isPending(PackageId));
        AppCatalog::Get().dismissDictionaryUpdate();
        CHECK(AppCatalog::Get().GetDictionaryUpdate().isEmpty());
        CHECK_FALSE(AppCatalog::Get().GetAvailableDictionaryUpdate().isEmpty());

        AppPackageDownloader::Get().download(PackageId);
        REQUIRE(AppPackageDownloader::Get().isPending(PackageId));
        REQUIRE(WaitForInstall(PackageId));

        CHECK_FALSE(AppDictionary::Get().IsOutdated(Language, "en"));
        CHECK(AppCatalog::GetInstalledVersion(PackageId) == 2);

        const FTDictionaryPackage* Active{AppDictionary::Get().GetActivePackage()};

        REQUIRE(Active != nullptr);
        CHECK(Active->TargetLanguage == Language);
        CHECK(Active->IsCompatible());

        CHECK(FindLanguage(Language).value(QStringLiteral("installed")).toBool());
    }
}

TEST_CASE("Package update offers persist until their version is dismissed or installed", "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;
    Test.LearnThenOutdate(1);

    FTPublishedPackage Published;
    Published.Version = 2;
    REQUIRE(Test.Publish(Published));

    QVariantList Updates{AppCatalog::Get().property("packageUpdates").toList()};
    REQUIRE(Updates.size() == 1);
    CHECK_FALSE(Updates.first().toMap().value(QStringLiteral("dismissed")).toBool());

    REQUIRE(QMetaObject::invokeMethod(&AppCatalog::Get(), "dismissPackageUpdate", Q_ARG(QString, PackageId)));
    REQUIRE(Test.Publish(Published));
    Updates = AppCatalog::Get().property("packageUpdates").toList();
    REQUIRE(Updates.size() == 1);
    CHECK(Updates.first().toMap().value(QStringLiteral("dismissed")).toBool());

    Published.Version = 3;
    REQUIRE(Test.Publish(Published));
    Updates = AppCatalog::Get().property("packageUpdates").toList();
    REQUIRE(Updates.size() == 1);
    CHECK_FALSE(Updates.first().toMap().value(QStringLiteral("dismissed")).toBool());

    AppCatalog::SetInstalledVersion(PackageId, 3);
    CHECK(AppCatalog::Get().property("packageUpdates").toList().isEmpty());
}

TEST_CASE("Installed voices and translations offer newer versions without downloading them", "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;
    FTPublishedPackage Published;
    Published.Version = 2;

    SECTION("voice")
    {
        Published.Kind = QStringLiteral("voice");
    }

    SECTION("translation")
    {
        Published.Kind = QStringLiteral("translation");
    }

    REQUIRE(Test.Publish(Published));
    CHECK(AppCatalog::Get().GetPackageUpdates().isEmpty());

    AppCatalog::SetInstalledVersion(PackageId, 1);
    REQUIRE(Test.Publish(Published));
    const QVariantList Updates{AppCatalog::Get().GetPackageUpdates()};
    REQUIRE(Updates.size() == 1);
    CHECK(Updates.first().toMap().value(QStringLiteral("id")).toString() == PackageId);
    CHECK_FALSE(AppPackageDownloader::Get().isPending(PackageId));

    AppCatalog::Get().dismissPackageUpdate(PackageId);
    REQUIRE(Test.Publish(Published));
    CHECK(AppCatalog::Get().GetPackageUpdates().first().toMap().value(QStringLiteral("dismissed")).toBool());
}

TEST_CASE("Dismissing an app update survives catalog refresh and a newer version is offered", "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;
    FTPublishedPackage Published;
    Published.ApplicationVersion = QStringLiteral("99.0.0");
    REQUIRE(Test.Publish(Published));
    CHECK_FALSE(AppCatalog::Get().IsAppUpdateDismissed());
    AppCatalog::Get().dismissAppUpdate();
    REQUIRE(Test.Publish(Published));
    CHECK(AppCatalog::Get().IsAppUpdateDismissed());
    CHECK(AppCatalog::Get().IsUpdateAvailable());

    Published.ApplicationVersion = QStringLiteral("100.0.0");
    REQUIRE(Test.Publish(Published));
    CHECK_FALSE(AppCatalog::Get().IsAppUpdateDismissed());
}

TEST_CASE("Package work counts as pending from the request until the download is dropped",
          "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;
    FTPublishedPackage Published;
    Published.Version = 2;

    REQUIRE(Test.Publish(Published));
    CHECK_FALSE(AppPackageDownloader::Get().HasPendingWork());

    AppPackageDownloader::Get().download(PackageId);

    CHECK(AppPackageDownloader::Get().HasPendingWork());

    AppPackageDownloader::Get().forget(PackageId);

    CHECK_FALSE(AppPackageDownloader::Get().HasPendingWork());
}

TEST_CASE("An installed archive replaces what it names and leaves the rest alone", "[dictionaryupdate]")
{
    QTemporaryDir Root;
    REQUIRE(Root.isValid());

    const QString Destination{Root.filePath(QStringLiteral("dest"))};
    const QString Scratch{Root.filePath(QStringLiteral("scratch"))};
    const QString Archive{QDir(Scratch).filePath(QStringLiteral("package.zip"))};

    WriteText(QDir(Destination).filePath(QStringLiteral("a.db")), "old-a");
    WriteText(QDir(Destination).filePath(QStringLiteral("keep.txt")), "keep");
    WriteTextArchive(Archive, {{QStringLiteral("a.db"), "new-a"}, {QStringLiteral("b.db"), "new-b"}});

    REQUIRE(AppPackageDownloader::InstallArchive(Archive, Destination, Scratch));

    CHECK(ReadText(QDir(Destination).filePath(QStringLiteral("a.db"))) == "new-a");
    CHECK(ReadText(QDir(Destination).filePath(QStringLiteral("b.db"))) == "new-b");
    CHECK(ReadText(QDir(Destination).filePath(QStringLiteral("keep.txt"))) == "keep");
    CHECK(QDir(Destination).entryList({QStringLiteral("*.ikonika-old")}, QDir::AllEntries).isEmpty());
    CHECK_FALSE(HasStagingFolder(Scratch));
}

TEST_CASE("An archive that cannot be read leaves the installed files untouched", "[dictionaryupdate]")
{
    QTemporaryDir Root;
    REQUIRE(Root.isValid());

    const QString Destination{Root.filePath(QStringLiteral("dest"))};
    const QString Scratch{Root.filePath(QStringLiteral("scratch"))};
    const QString Archive{QDir(Scratch).filePath(QStringLiteral("package.zip"))};

    WriteText(QDir(Destination).filePath(QStringLiteral("a.db")), "old-a");
    WriteText(Archive, "this is not a zip archive");

    CHECK_FALSE(AppPackageDownloader::InstallArchive(Archive, Destination, Scratch));

    CHECK(ReadText(QDir(Destination).filePath(QStringLiteral("a.db"))) == "old-a");
    CHECK_FALSE(HasStagingFolder(Scratch));
}

#if defined(Q_OS_WIN)
TEST_CASE("A swap that fails halfway puts every replaced file back", "[dictionaryupdate]")
{
    QTemporaryDir Root;
    REQUIRE(Root.isValid());

    const QString Destination{Root.filePath(QStringLiteral("dest"))};
    const QString Scratch{Root.filePath(QStringLiteral("scratch"))};
    const QString Archive{QDir(Scratch).filePath(QStringLiteral("package.zip"))};
    const QString Blocked{QDir(Destination).filePath(QStringLiteral("b.db"))};
    const QString Inner{QDir(Blocked).filePath(QStringLiteral("inner.txt"))};

    WriteText(QDir(Destination).filePath(QStringLiteral("a.db")), "old-a");
    WriteText(Inner, "inner");
    WriteTextArchive(Archive, {{QStringLiteral("a.db"), "new-a"}, {QStringLiteral("b.db"), "new-b"}});

    QFile Holder{Inner};
    REQUIRE(Holder.open(QIODevice::ReadOnly));

    CHECK_FALSE(AppPackageDownloader::InstallArchive(Archive, Destination, Scratch));

    Holder.close();

    CHECK(ReadText(QDir(Destination).filePath(QStringLiteral("a.db"))) == "old-a");
    CHECK(ReadText(Inner) == "inner");
    CHECK(QDir(Destination).entryList({QStringLiteral("*.ikonika-old")}, QDir::AllEntries).isEmpty());
    CHECK_FALSE(HasStagingFolder(Scratch));
}
#endif

TEST_CASE("Recovery puts back a file whose replacement never arrived and clears the rest", "[dictionaryupdate]")
{
    QTemporaryDir Root;
    REQUIRE(Root.isValid());

    const QString Destination{Root.filePath(QStringLiteral("dest"))};
    const QString Scratch{Root.filePath(QStringLiteral("scratch"))};

    WriteText(QDir(Destination).filePath(QStringLiteral("lost.db.ikonika-old")), "old-lost");
    WriteText(QDir(Destination).filePath(QStringLiteral("done.db")), "new-done");
    WriteText(QDir(Destination).filePath(QStringLiteral("done.db.ikonika-old")), "old-done");
    WriteText(QDir(Destination).filePath(QStringLiteral("nested/deep.db.ikonika-old")), "old-deep");
    WriteText(QDir(Scratch).filePath(QStringLiteral("staging-leftover/part.db")), "half");
    WriteText(QDir(Scratch).filePath(QStringLiteral("keep.zip")), "download");

    AppPackageDownloader::RecoverInterruptedInstalls({Destination}, Scratch);

    CHECK(ReadText(QDir(Destination).filePath(QStringLiteral("lost.db"))) == "old-lost");
    CHECK(ReadText(QDir(Destination).filePath(QStringLiteral("done.db"))) == "new-done");
    CHECK(ReadText(QDir(Destination).filePath(QStringLiteral("nested/deep.db"))) == "old-deep");
    CHECK_FALSE(QFileInfo::exists(QDir(Destination).filePath(QStringLiteral("done.db.ikonika-old"))));
    CHECK_FALSE(QFileInfo::exists(QDir(Destination).filePath(QStringLiteral("lost.db.ikonika-old"))));
    CHECK_FALSE(HasStagingFolder(Scratch));
    CHECK(ReadText(QDir(Scratch).filePath(QStringLiteral("keep.zip"))) == "download");
}

TEST_CASE("Downloads in flight are saved and start again after a restart", "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;
    FTPublishedPackage Published;
    Published.Version = 2;

    REQUIRE(Test.Publish(Published));

    const QString SavedKey{QStringLiteral("downloads/queue")};

    QSettings().remove(SavedKey);

    AppPackageDownloader::Get().download(PackageId);

    CHECK(QSettings().value(SavedKey).toStringList() == QStringList{PackageId});

    AppPackageDownloader Restarted;

    CHECK_FALSE(Restarted.isPending(PackageId));

    Restarted.RestoreQueue();

    CHECK(Restarted.isPending(PackageId));

    Restarted.forget(PackageId);
    AppPackageDownloader::Get().forget(PackageId);

    CHECK_FALSE(QSettings().contains(SavedKey));
}

TEST_CASE("A saved download that is already installed is not started again", "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;
    FTPublishedPackage Published;
    Published.Version = 2;

    REQUIRE(Test.Publish(Published));

    const QString SavedKey{QStringLiteral("downloads/queue")};

    AppCatalog::SetInstalledVersion(PackageId, 2);
    QSettings().setValue(SavedKey, QStringList{PackageId});

    AppPackageDownloader Restarted;
    Restarted.RestoreQueue();

    CHECK_FALSE(Restarted.isPending(PackageId));
    CHECK_FALSE(QSettings().contains(SavedKey));
}

TEST_CASE("A saved download whose package is gone is dropped", "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;
    FTPublishedPackage Published;
    Published.Version = 2;

    REQUIRE(Test.Publish(Published));

    const QString SavedKey{QStringLiteral("downloads/queue")};

    QSettings().setValue(SavedKey, QStringList{QStringLiteral("dictionary-gone")});

    AppPackageDownloader Restarted;
    Restarted.RestoreQueue();

    CHECK_FALSE(Restarted.isPending(QStringLiteral("dictionary-gone")));
    CHECK_FALSE(QSettings().contains(SavedKey));
}

TEST_CASE("A new download before the catalog is read does not erase the saved ones", "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;

    const QString SavedKey{QStringLiteral("downloads/queue")};

    QSettings().setValue(SavedKey, QStringList{PackageId});

    AppPackageDownloader Restarted;
    Restarted.download(QStringLiteral("dictionary-unknown"));

    CHECK(QSettings().value(SavedKey).toStringList() == QStringList{PackageId});

    QSettings().remove(SavedKey);
}

TEST_CASE("An outdated dictionary is never replaced with a package this app cannot read",
          "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;

    Test.LearnThenOutdate(1);

    FTPublishedPackage Unreadable;
    Unreadable.Version = 2;
    Unreadable.Schema = OldestDictionarySchemaVersion - 1;

    REQUIRE(Test.Publish(Unreadable));

    CHECK_FALSE(AppPackageDownloader::Get().isPending(PackageId));
    CHECK(AppCatalog::Get().GetAvailableDictionaryUpdate().isEmpty());

    const QVariantMap Row{FindLanguage(Language)};

    REQUIRE_FALSE(Row.isEmpty());
    CHECK(Row.value(QStringLiteral("outdated")).toBool());
    CHECK_FALSE(Row.value(QStringLiteral("compatible")).toBool());
}

TEST_CASE("A published version already installed is not downloaded again", "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;

    Test.LearnThenOutdate(2);

    FTPublishedPackage Same;
    Same.Version = 2;

    REQUIRE(Test.Publish(Same));

    CHECK_FALSE(AppPackageDownloader::Get().isPending(PackageId));
    CHECK(AppCatalog::Get().GetAvailableDictionaryUpdate().isEmpty());
}

TEST_CASE("Returning to the app reads the catalog again only once the cooldown has run out",
          "[dictionaryupdate]")
{
    FTDictionaryUpdateFixture Test;

    REQUIRE(Test.Publish(FTPublishedPackage{}));

    QTimer* Cooldown{AppCatalog::Get().findChild<QTimer*>(QStringLiteral("CatalogRefreshCooldownTimer"))};
    REQUIRE(Cooldown != nullptr);
    CHECK(Cooldown->isActive());

    AppCatalog::Get().RefreshWhenStale();

    CHECK_FALSE(AppCatalog::Get().IsBusy());

    Cooldown->stop();

    QEventLoop Loop;
    QObject::connect(&AppCatalog::Get(), &AppCatalog::catalogChanged, &Loop, &QEventLoop::quit);
    QObject::connect(&AppCatalog::Get(), &AppCatalog::lastErrorChanged, &Loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &Loop, &QEventLoop::quit);

    AppCatalog::Get().RefreshWhenStale();

    CHECK(AppCatalog::Get().IsBusy());
    CHECK(Cooldown->isActive());

    Loop.exec();

    CHECK_FALSE(AppCatalog::Get().IsBusy());
    CHECK(AppCatalog::Get().IsLoaded());
}
