#include <catch2/catch_test_macros.hpp>

#include "FTPdfDocumentExtractor.h"

#include <QFile>
#include <QString>

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    constexpr const char* FixtureDirectory = SAVED_TEST_DATA_DIRECTORY;

    /** Writes Bytes to a named file and returns its path, or an empty string on failure. */
    std::string WriteFile(const char* Name, const std::string& Bytes)
    {
        const QString Path = QString::fromLatin1(FixtureDirectory) + QString::fromLatin1("/")
            + QString::fromLatin1(Name);

        QFile File(Path);

        if (!File.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return {};
        }

        File.write(Bytes.data(), static_cast<qint64>(Bytes.size()));
        File.close();

        return Path.toStdString();
    }

    /**
     * Builds a minimal one-to-two-object PDF by hand so the tests need no binary fixture.
     * Streams holds one content stream per page; Title, when non-empty, becomes the document's
     * metadata /Title. The xref table is written with real byte offsets so PDFium accepts the file.
     */
    std::string BuildPdf(const std::vector<std::string>& Streams, const std::string& Title)
    {
        const int PageCount = static_cast<int>(Streams.size());
        const int FirstPageObject = 3;
        const int FirstStreamObject = FirstPageObject + PageCount;
        const int FontObject = FirstStreamObject + PageCount;
        const int InfoObject = FontObject + 1;

        std::vector<std::string> Bodies(InfoObject + 1);

        Bodies[1] = "<< /Type /Catalog /Pages 2 0 R >>";

        {
            std::string Kids = "[";
            for (int i = 0; i < PageCount; ++i)
            {
                if (i != 0)
                {
                    Kids += " ";
                }
                Kids += std::to_string(FirstPageObject + i) + " 0 R";
            }
            Kids += "]";

            Bodies[2] = "<< /Type /Pages /Kids " + Kids + " /Count " + std::to_string(PageCount)
                + " >>";
        }

        for (int i = 0; i < PageCount; ++i)
        {
            Bodies[FirstPageObject + i] = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
                "/Contents " + std::to_string(FirstStreamObject + i) + " 0 R "
                "/Resources << /Font << /F1 " + std::to_string(FontObject) + " 0 R >> >> >>";
        }

        for (int i = 0; i < PageCount; ++i)
        {
            Bodies[FirstStreamObject + i] = "<< /Length " + std::to_string(Streams[i].size())
                + " >>\nstream\n" + Streams[i] + "endstream";
        }

        Bodies[FontObject] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>";

        const bool bHasInfo = !Title.empty();
        if (bHasInfo)
        {
            Bodies[InfoObject] = "<< /Title (" + Title + ") >>";
        }

        const int ObjectCount = bHasInfo ? InfoObject : FontObject;

        std::string Pdf = "%PDF-1.4\n";
        std::vector<std::size_t> Offsets(InfoObject + 1, 0);

        for (int i = 1; i <= ObjectCount; ++i)
        {
            Offsets[i] = Pdf.size();
            Pdf += std::to_string(i) + " 0 obj\n";
            Pdf += Bodies[i];
            Pdf += "\nendobj\n";
        }

        const std::size_t XrefOffset = Pdf.size();
        Pdf += "xref\n0 " + std::to_string(ObjectCount + 1) + "\n";
        Pdf += "0000000000 65535 f \n";

        for (int i = 1; i <= ObjectCount; ++i)
        {
            std::ostringstream OffsetStream;
            OffsetStream << std::setw(10) << std::setfill('0') << Offsets[i];
            Pdf += OffsetStream.str() + " 00000 n \n";
        }

        Pdf += "trailer\n<< /Size " + std::to_string(ObjectCount + 1) + " /Root 1 0 R";

        if (bHasInfo)
        {
            Pdf += " /Info " + std::to_string(InfoObject) + " 0 R";
        }

        Pdf += " >>\nstartxref\n" + std::to_string(XrefOffset) + "\n%%EOF\n";

        return Pdf;
    }

    constexpr const char* TextStream = "BT /F1 24 Tf 72 720 Td (Bonjour le monde) Tj ET\n";
    constexpr const char* DrawingStream = "0 0 100 100 re f\n";
}

TEST_CASE("A pdf is named by its metadata title", "[reader][extractor][pdf]")
{
    const std::string Path = WriteFile("ReaderFixture.pdf", BuildPdf(
        { TextStream, "BT /F1 24 Tf 72 720 Td (Au revoir) Tj ET\n" }, "Le Petit Chien"));

    REQUIRE_FALSE(Path.empty());

    FTPdfDocumentExtractor Extractor;
    const std::optional<FTReaderDocument> Result = Extractor.Extract(Path);

    REQUIRE(Result.has_value());
    CHECK(Result->Title == "Le Petit Chien");
    CHECK(Result->Paragraphs.empty());
}

TEST_CASE("A pdf without a metadata title leaves the name to its file", "[reader][extractor][pdf]")
{
    const std::string Path = WriteFile("ReaderFixture.pdf", BuildPdf({ TextStream }, ""));

    REQUIRE_FALSE(Path.empty());

    FTPdfDocumentExtractor Extractor;
    const std::optional<FTReaderDocument> Result = Extractor.Extract(Path);

    REQUIRE(Result.has_value());
    CHECK(Result->Title.empty());
    CHECK(Result->Paragraphs.empty());
}

TEST_CASE("A metadata title that is a file name or a placeholder is left out", "[reader][extractor][pdf]")
{
    FTPdfDocumentExtractor Extractor;

    for (const char* Junk : { "Microsoft Word - chapter one", "report-final.docx", "Untitled", "Document1" })
    {
        const std::string Path = WriteFile("ReaderFixture.pdf", BuildPdf({ TextStream }, Junk));

        REQUIRE_FALSE(Path.empty());

        const std::optional<FTReaderDocument> Result = Extractor.Extract(Path);

        REQUIRE(Result.has_value());
        CHECK(Result->Title.empty());
    }
}

TEST_CASE("A scanned pdf still opens as a document", "[reader][extractor][pdf]")
{
    const std::string Path = WriteFile("ReaderFixture.pdf", BuildPdf({ DrawingStream }, ""));

    REQUIRE_FALSE(Path.empty());

    FTPdfDocumentExtractor Extractor;
    const std::optional<FTReaderDocument> Result = Extractor.Extract(Path);

    REQUIRE(Result.has_value());
    CHECK(Result->Paragraphs.empty());
}

TEST_CASE("A corrupt file extracts to nothing", "[reader][extractor][pdf]")
{
    const std::string Path = WriteFile("ReaderFixture.pdf", "this is not a pdf");

    REQUIRE_FALSE(Path.empty());

    FTPdfDocumentExtractor Extractor;
    CHECK_FALSE(Extractor.Extract(Path).has_value());
}

TEST_CASE("A missing pdf file extracts to nothing", "[reader][extractor][pdf]")
{
    FTPdfDocumentExtractor Extractor;
    CHECK_FALSE(Extractor.Extract("C:/nowhere/does-not-exist.pdf").has_value());
}
