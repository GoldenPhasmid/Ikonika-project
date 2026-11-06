#include <catch2/catch_test_macros.hpp>

#include "core/FTCsvExporter.h"
#include "core/FTCsvImporter.h"
#include "core/FTImportSource.h"

#include <QFile>
#include <QString>

#include <string>
#include <vector>

namespace
{
    /** Writes Content to a temp file and returns its path, or an empty string on failure. */
    std::string WriteTempCsv(const char* Content)
    {
        const QString Path = QStringLiteral(SAVED_TEST_DATA_DIRECTORY) + QStringLiteral("/CsvImportTest.csv");

        QFile File(Path);

        if (!File.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return {};
        }

        File.write(Content);
        File.close();

        return Path.toStdString();
    }
}

TEST_CASE("A simple CSV file yields word-translation pairs", "[import][csv]")
{
    const std::string Path = WriteTempCsv("le chien,dog\nla maison,house\nun chat,cat\n");
    REQUIRE_FALSE(Path.empty());

    FTCsvImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 3);
    CHECK(Importer.GetRows()[0].TargetWord == "le chien");
    CHECK(Importer.GetRows()[0].Translation == "dog");
    CHECK(Importer.GetRows()[2].TargetWord == "un chat");
}

TEST_CASE("A quoted field keeps its comma inside the translation", "[import][csv]")
{
    const std::string Path = WriteTempCsv("chien,\"dog, hound\"\nchat,cat\n");
    REQUIRE_FALSE(Path.empty());

    FTCsvImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 2);
    CHECK(Importer.GetRows()[0].Translation == "dog, hound");
}

TEST_CASE("Escaped quotes inside a quoted field survive import", "[import][csv]")
{
    const std::string Path = WriteTempCsv("mot,\"a \"\"quoted\"\" word\"\n");
    REQUIRE_FALSE(Path.empty());

    FTCsvImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 1);
    CHECK(Importer.GetRows()[0].Translation == "a \"quoted\" word");
}

TEST_CASE("The exporter's own header line is skipped on import", "[import][csv]")
{
    const std::string Path = WriteTempCsv("Word,Translation\nchien,dog\nchat,cat\n");
    REQUIRE_FALSE(Path.empty());

    FTCsvImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 2);
    CHECK(Importer.GetRows()[0].TargetWord == "chien");
}

TEST_CASE("Blank lines and rows missing a translation are skipped", "[import][csv]")
{
    const std::string Path = WriteTempCsv("\nchien,dog\nchat\n\n");
    REQUIRE_FALSE(Path.empty());

    FTCsvImporter Importer;

    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 1);
    CHECK(Importer.GetRows()[0].TargetWord == "chien");
}

TEST_CASE("An empty CSV file yields an error", "[import][csv]")
{
    const std::string Path = WriteTempCsv("Word,Translation\n");
    REQUIRE_FALSE(Path.empty());

    FTCsvImporter Importer;

    REQUIRE_FALSE(Importer.Parse(Path));
    CHECK_FALSE(Importer.GetLastError().empty());
}

TEST_CASE("Export writes a header line and every pair", "[export][csv]")
{
    std::vector<FTImportRow> Rows;
    Rows.push_back({ "le chien", "dog" });
    Rows.push_back({ "la maison", "house" });

    const std::string Path = std::string(SAVED_TEST_DATA_DIRECTORY) + "/CsvExportTest.csv";

    FTCsvExporter Exporter;
    REQUIRE(Exporter.Write(Path, Rows));

    QFile File(QString::fromStdString(Path));
    REQUIRE(File.open(QIODevice::ReadOnly));

    const QByteArray Content = File.readAll();

    CHECK(Content.startsWith("Word,Translation\n"));
    CHECK(Content.contains("le chien,dog\n"));
    CHECK(Content.contains("la maison,house\n"));
}

TEST_CASE("Export quotes fields that would break the CSV structure", "[export][csv]")
{
    std::vector<FTImportRow> Rows;
    Rows.push_back({ "chien", "dog, hound" });
    Rows.push_back({ "mot", "a \"quoted\" word" });

    const std::string Path = std::string(SAVED_TEST_DATA_DIRECTORY) + "/CsvExportTest.csv";

    FTCsvExporter Exporter;
    REQUIRE(Exporter.Write(Path, Rows));

    QFile File(QString::fromStdString(Path));
    REQUIRE(File.open(QIODevice::ReadOnly));

    const QByteArray Content = File.readAll();

    CHECK(Content.contains("chien,\"dog, hound\"\n"));
    CHECK(Content.contains("mot,\"a \"\"quoted\"\" word\"\n"));
}

TEST_CASE("Export then import round-trips the pairs unchanged", "[export][import][csv]")
{
    std::vector<FTImportRow> Rows;
    Rows.push_back({ "le chien", "dog, hound" });
    Rows.push_back({ "une maison", "house" });

    const std::string Path = std::string(SAVED_TEST_DATA_DIRECTORY) + "/CsvRoundTrip.csv";

    FTCsvExporter Exporter;
    REQUIRE(Exporter.Write(Path, Rows));

    FTCsvImporter Importer;
    REQUIRE(Importer.Parse(Path));
    REQUIRE(Importer.GetRows().size() == 2);
    CHECK(Importer.GetRows()[0].TargetWord == "le chien");
    CHECK(Importer.GetRows()[0].Translation == "dog, hound");
    CHECK(Importer.GetRows()[1].TargetWord == "une maison");
    CHECK(Importer.GetRows()[1].Translation == "house");
}
