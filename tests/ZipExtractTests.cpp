#include <catch2/catch_test_macros.hpp>

#include "core/FTZipExtract.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "miniz.h"

#include <string>

namespace
{
    bool WriteArchive(const QString& Path,
                      const std::vector<std::pair<std::string, std::string>>& Entries)
    {
        mz_zip_archive Zip;
        std::memset(&Zip, 0, sizeof(Zip));

        if (!mz_zip_writer_init_file(&Zip, Path.toUtf8().constData(), 0))
        {
            return false;
        }

        for (const auto& [Name, Content] : Entries)
        {
            if (!mz_zip_writer_add_mem(&Zip, Name.c_str(), Content.data(), Content.size(),
                                       static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION)))
            {
                mz_zip_writer_end(&Zip);

                return false;
            }
        }

        const bool bFinalised = mz_zip_writer_finalize_archive(&Zip);
        mz_zip_writer_end(&Zip);

        return bFinalised;
    }

    QString ReadFile(const QString& Path)
    {
        QFile File(Path);

        if (!File.open(QIODevice::ReadOnly))
        {
            return {};
        }

        return QString::fromUtf8(File.readAll());
    }
}

TEST_CASE("An archive unpacks into a directory that does not exist yet", "[zip]")
{
    QTemporaryDir Working;
    REQUIRE(Working.isValid());

    const QString Archive = QDir(Working.path()).filePath(QStringLiteral("package.zip"));
    REQUIRE(WriteArchive(Archive, {{"fr-dictionary.db", "database bytes"}}));

    const QString Destination = QDir(Working.path()).filePath(QStringLiteral("out/dictionaries"));

    REQUIRE(ExtractZipArchive(Archive.toStdString(), Destination.toStdString()));
    REQUIRE(ReadFile(QDir(Destination).filePath(QStringLiteral("fr-dictionary.db"))) ==
            QStringLiteral("database bytes"));
}

TEST_CASE("Nested entries keep their directories", "[zip]")
{
    QTemporaryDir Working;
    REQUIRE(Working.isValid());

    const QString Archive = QDir(Working.path()).filePath(QStringLiteral("voice.zip"));
    REQUIRE(WriteArchive(Archive, {
        {"tts.json", "config"},
        {"nested/voice.bin", "weights"},
        {"nested/deeper/unicode_indexer.bin", "indexer"}
    }));

    const QString Destination = QDir(Working.path()).filePath(QStringLiteral("models/piper-fr"));

    REQUIRE(ExtractZipArchive(Archive.toStdString(), Destination.toStdString()));

    const QDir Out(Destination);

    REQUIRE(ReadFile(Out.filePath(QStringLiteral("tts.json"))) == QStringLiteral("config"));
    REQUIRE(ReadFile(Out.filePath(QStringLiteral("nested/voice.bin"))) == QStringLiteral("weights"));
    REQUIRE(ReadFile(Out.filePath(QStringLiteral("nested/deeper/unicode_indexer.bin"))) ==
            QStringLiteral("indexer"));
}

TEST_CASE("Unpacking replaces a file already there", "[zip]")
{
    QTemporaryDir Working;
    REQUIRE(Working.isValid());

    const QString Destination = QDir(Working.path()).filePath(QStringLiteral("out"));
    REQUIRE(QDir().mkpath(Destination));

    QFile Existing(QDir(Destination).filePath(QStringLiteral("fr-dictionary.db")));
    REQUIRE(Existing.open(QIODevice::WriteOnly));
    Existing.write("an older and longer copy of the database");
    Existing.close();

    const QString Archive = QDir(Working.path()).filePath(QStringLiteral("package.zip"));
    REQUIRE(WriteArchive(Archive, {{"fr-dictionary.db", "new"}}));

    REQUIRE(ExtractZipArchive(Archive.toStdString(), Destination.toStdString()));
    REQUIRE(ReadFile(QDir(Destination).filePath(QStringLiteral("fr-dictionary.db"))) ==
            QStringLiteral("new"));
}

TEST_CASE("An entry climbing out of the destination is refused", "[zip]")
{
    QTemporaryDir Working;
    REQUIRE(Working.isValid());

    const QString Archive = QDir(Working.path()).filePath(QStringLiteral("hostile.zip"));
    REQUIRE(WriteArchive(Archive, {{"../escaped.txt", "should never be written"}}));

    const QString Destination = QDir(Working.path()).filePath(QStringLiteral("out"));

    REQUIRE_FALSE(ExtractZipArchive(Archive.toStdString(), Destination.toStdString()));
    REQUIRE_FALSE(QFile::exists(QDir(Working.path()).filePath(QStringLiteral("escaped.txt"))));
}

TEST_CASE("An entry naming an absolute path is refused", "[zip]")
{
    QTemporaryDir Working;
    REQUIRE(Working.isValid());

    const QString Archive = QDir(Working.path()).filePath(QStringLiteral("hostile.zip"));
    REQUIRE(WriteArchive(Archive, {{"C:/Windows/System32/planted.dll", "no"}}));

    const QString Destination = QDir(Working.path()).filePath(QStringLiteral("out"));

    REQUIRE_FALSE(ExtractZipArchive(Archive.toStdString(), Destination.toStdString()));
}

TEST_CASE("A file that is not an archive is refused rather than half read", "[zip]")
{
    QTemporaryDir Working;
    REQUIRE(Working.isValid());

    const QString NotAnArchive = QDir(Working.path()).filePath(QStringLiteral("truncated.zip"));

    QFile File(NotAnArchive);
    REQUIRE(File.open(QIODevice::WriteOnly));
    File.write("PK\x03\x04 and then nothing that follows the format");
    File.close();

    const QString Destination = QDir(Working.path()).filePath(QStringLiteral("out"));

    REQUIRE_FALSE(ExtractZipArchive(NotAnArchive.toStdString(), Destination.toStdString()));
}

TEST_CASE("A missing archive is refused", "[zip]")
{
    QTemporaryDir Working;
    REQUIRE(Working.isValid());

    const QString Missing = QDir(Working.path()).filePath(QStringLiteral("absent.zip"));
    const QString Destination = QDir(Working.path()).filePath(QStringLiteral("out"));

    REQUIRE_FALSE(ExtractZipArchive(Missing.toStdString(), Destination.toStdString()));
}

TEST_CASE("An empty archive unpacks to nothing without failing", "[zip]")
{
    QTemporaryDir Working;
    REQUIRE(Working.isValid());

    const QString Archive = QDir(Working.path()).filePath(QStringLiteral("empty.zip"));
    REQUIRE(WriteArchive(Archive, {}));

    const QString Destination = QDir(Working.path()).filePath(QStringLiteral("out"));

    REQUIRE(ExtractZipArchive(Archive.toStdString(), Destination.toStdString()));
    REQUIRE(QDir(Destination).exists());
    REQUIRE(QDir(Destination).entryList(QDir::Files).isEmpty());
}

TEST_CASE("A backslash-separated entry, as PowerShell's Compress-Archive writes, still extracts",
          "[zip]")
{
    QTemporaryDir Working;
    REQUIRE(Working.isValid());

    const QString Archive = QDir(Working.path()).filePath(QStringLiteral("backslash.zip"));

    REQUIRE(WriteArchive(Archive, {
        {"sub\\", ""},
        {"sub\\file.txt", "hello"}
    }));

    const QString Destination = QDir(Working.path()).filePath(QStringLiteral("out"));

    REQUIRE(ExtractZipArchive(Archive.toStdString(), Destination.toStdString()));
    CHECK(ReadFile(QDir(Destination).filePath(QStringLiteral("sub/file.txt"))) == "hello");
}
