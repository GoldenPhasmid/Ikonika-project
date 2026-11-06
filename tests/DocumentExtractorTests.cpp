#include <catch2/catch_test_macros.hpp>

#include "core/FTDocumentExtractor.h"

#include "miniz.h"

#include <QFile>
#include <QString>

#include <cstring>
#include <string>
#include <vector>

namespace
{
    constexpr const char* FixtureDirectory = SAVED_TEST_DATA_DIRECTORY;

    /** Writes Content to a named temp file and returns its path, or an empty string on failure. */
    std::string WriteFile(const char* Name, const char* Content)
    {
        const QString Path = QString::fromLatin1(FixtureDirectory) + QString::fromLatin1("/")
            + QString::fromLatin1(Name);

        QFile File(Path);

        if (!File.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return {};
        }

        File.write(Content);
        File.close();

        return Path.toStdString();
    }

    /** Builds a zip holding the given entries, at the given path. */
    bool WriteZip(const char* Path, const std::vector<std::pair<const char*, const char*>>& Entries)
    {
        mz_zip_archive Zip;
        std::memset(&Zip, 0, sizeof(Zip));

        if (!mz_zip_writer_init_file(&Zip, Path, 0))
        {
            return false;
        }

        bool bOk = true;

        for (const auto& [Name, Content] : Entries)
        {
            if (!mz_zip_writer_add_mem(&Zip, Name, Content, std::strlen(Content), MZ_BEST_SPEED))
            {
                bOk = false;
                break;
            }
        }

        if (bOk)
        {
            bOk = mz_zip_writer_finalize_archive(&Zip) && mz_zip_writer_end(&Zip);
        }
        else
        {
            mz_zip_writer_end(&Zip);
        }

        return bOk;
    }
}

TEST_CASE("A txt file yields paragraphs separated by blank lines", "[reader][extractor]")
{
    const std::string Path = WriteFile("ReaderFixture.txt",
        "First paragraph, wrapped over two lines.\ncontinues here.\n\n"
        "Second paragraph.\n");

    REQUIRE_FALSE(Path.empty());

    FTTxtDocumentExtractor Extractor;
    const std::optional<FTReaderDocument> Result = Extractor.Extract(Path);

    REQUIRE(Result.has_value());
    REQUIRE(Result->Paragraphs.size() == 2);
    CHECK(Result->Paragraphs[0] == "First paragraph, wrapped over two lines. continues here.");
    CHECK(Result->Paragraphs[1] == "Second paragraph.");
    CHECK(Result->Title == "ReaderFixture");
}

TEST_CASE("A missing file extracts to nothing", "[reader][extractor]")
{
    FTTxtDocumentExtractor Extractor;
    CHECK_FALSE(Extractor.Extract("C:/nowhere/does-not-exist.txt").has_value());
}

TEST_CASE("An fb2 file yields its title and body paragraphs", "[reader][extractor]")
{
    const std::string Path = WriteFile("ReaderFixture.fb2",
        "<?xml version=\"1.0\"?>\n"
        "<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\">\n"
        "<description><title-info><book-title>Le Petit Chien</book-title></title-info></description>\n"
        "<body><section>"
        "<title><p>Chapter One</p></title>\n"
        "<p>Le chien court.</p>\n"
        "<p>Il <strong>mange</strong> vite.</p>\n"
        "</section></body>\n"
        "</FictionBook>\n");

    REQUIRE_FALSE(Path.empty());

    FTFb2DocumentExtractor Extractor;
    const std::optional<FTReaderDocument> Result = Extractor.Extract(Path);

    REQUIRE(Result.has_value());
    CHECK(Result->Title == "Le Petit Chien");
    REQUIRE(Result->Paragraphs.size() == 3);
    CHECK(Result->Paragraphs[0] == "Chapter One");
    CHECK(Result->Paragraphs[1] == "Le chien court.");
    CHECK(Result->Paragraphs[2] == "Il mange vite.");
}

TEST_CASE("An epub file yields its title and spine content in order", "[reader][extractor]")
{
    const std::string Path = WriteFile("ReaderFixture.epub", "");

    REQUIRE(WriteZip(Path.c_str(), {
        { "mimetype", "application/epub+zip" },
        { "META-INF/container.xml",
          "<?xml version=\"1.0\"?><container version=\"1.0\" "
          "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
          "<rootfiles><rootfile full-path=\"OEBPS/content.opf\" "
          "media-type=\"application/oebps-package+xml\"/></rootfiles></container>" },
        { "OEBPS/content.opf",
          "<?xml version=\"1.0\"?><package version=\"2.0\" "
          "xmlns=\"http://www.idpf.org/2007/opf\">"
          "<metadata><dc:title xmlns:dc=\"http://purl.org/dc/elements/1.1/\">Les Deux Pages</dc:title>"
          "</metadata><manifest>"
          "<item id=\"c1\" href=\"chapter1.xhtml\" media-type=\"application/xhtml+xml\"/>"
          "<item id=\"c2\" href=\"chapter2.xhtml\" media-type=\"application/xhtml+xml\"/>"
          "</manifest><spine>"
          "<itemref idref=\"c1\"/><itemref idref=\"c2\"/>"
          "</spine></package>" },
        { "OEBPS/chapter1.xhtml",
          "<?xml version=\"1.0\"?><html><body>"
          "<p>Première page.</p>"
          "<p>Deuxième ligne.</p>"
          "</body></html>" },
        { "OEBPS/chapter2.xhtml",
          "<?xml version=\"1.0\"?><html><body>"
          "<p>Deuxième page.</p>"
          "</body></html>" }
    }));

    FTEpubDocumentExtractor Extractor;
    const std::optional<FTReaderDocument> Result = Extractor.Extract(Path);

    REQUIRE(Result.has_value());
    CHECK(Result->Title == "Les Deux Pages");
    REQUIRE(Result->Paragraphs.size() == 3);
    CHECK(Result->Paragraphs[0] == "Première page.");
    CHECK(Result->Paragraphs[1] == "Deuxième ligne.");
    CHECK(Result->Paragraphs[2] == "Deuxième page.");
}

TEST_CASE("A zip that is not an epub extracts to nothing", "[reader][extractor]")
{
    const std::string Path = WriteFile("ReaderFixture.bad", "");

    REQUIRE(WriteZip(Path.c_str(), { { "random.txt", "not an epub" } }));

    FTEpubDocumentExtractor Extractor;
    CHECK_FALSE(Extractor.Extract(Path).has_value());
}

TEST_CASE("Format detection reads the file extension", "[reader][extractor]")
{
    CHECK(DetectDocumentFormat("book.txt") == EFTDocumentFormat::Txt);
    CHECK(DetectDocumentFormat("book.fb2") == EFTDocumentFormat::Fb2);
    CHECK(DetectDocumentFormat("book.epub") == EFTDocumentFormat::Epub);
    CHECK(DetectDocumentFormat("book.pdf") == EFTDocumentFormat::Pdf);
    CHECK(DetectDocumentFormat("book.zip") == EFTDocumentFormat::Unknown);
    CHECK(DetectDocumentFormat("book") == EFTDocumentFormat::Unknown);
    CHECK(DetectDocumentFormat("book.FB2") == EFTDocumentFormat::Fb2);

    // A FictionBook packed as a zip keeps its own name.
    CHECK(DetectDocumentFormat("book.fb2.zip") == EFTDocumentFormat::Fb2);
}

TEST_CASE("Format detection returns a short label", "[reader][extractor]")
{
    CHECK(std::string(DocumentFormatName(EFTDocumentFormat::Txt)) == "TXT");
    CHECK(std::string(DocumentFormatName(EFTDocumentFormat::Fb2)) == "FB2");
    CHECK(std::string(DocumentFormatName(EFTDocumentFormat::Epub)) == "EPUB");
    CHECK(std::string(DocumentFormatName(EFTDocumentFormat::Pdf)) == "PDF");
    CHECK(std::string(DocumentFormatName(EFTDocumentFormat::Unknown)) == "");
}

TEST_CASE("A format label parses back to its format", "[reader][extractor]")
{
    CHECK(ParseDocumentFormat("TXT") == EFTDocumentFormat::Txt);
    CHECK(ParseDocumentFormat("FB2") == EFTDocumentFormat::Fb2);
    CHECK(ParseDocumentFormat("EPUB") == EFTDocumentFormat::Epub);
    CHECK(ParseDocumentFormat("PDF") == EFTDocumentFormat::Pdf);
    CHECK(ParseDocumentFormat("") == EFTDocumentFormat::Unknown);
    CHECK(ParseDocumentFormat("nonsense") == EFTDocumentFormat::Unknown);
}

namespace
{
    std::string WriteEpub(const char* Name, const char* Opf,
                          std::vector<std::pair<const char*, const char*>> Files)
    {
        const std::string Path = WriteFile(Name, "");

        Files.insert(Files.begin(), {
            { "mimetype", "application/epub+zip" },
            { "META-INF/container.xml",
              "<?xml version=\"1.0\"?><container version=\"1.0\" "
              "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
              "<rootfiles><rootfile full-path=\"OEBPS/content.opf\" "
              "media-type=\"application/oebps-package+xml\"/></rootfiles></container>" },
            { "OEBPS/content.opf", Opf }
        });

        return WriteZip(Path.c_str(), Files) ? Path : std::string();
    }
}

TEST_CASE("Fb2 declared as windows-1251 decodes to Cyrillic", "[reader][extractor]")
{
    const std::string Path = WriteFile("ReaderFixture1251.fb2",
        "<?xml version=\"1.0\" encoding=\"windows-1251\"?>\n"
        "<FictionBook><body><section><p>\xCF\xF0\xE8\xE2\xE5\xF2</p></section></body></FictionBook>");

    REQUIRE_FALSE(Path.empty());

    const std::optional<FTReaderDocument> Result = FTFb2DocumentExtractor().Extract(Path);

    REQUIRE(Result.has_value());
    REQUIRE(Result->Paragraphs.size() == 1);
    CHECK(Result->Paragraphs[0] == "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");
}

TEST_CASE("Fb2 embedded image data is not a paragraph", "[reader][extractor]")
{
    const std::string Path = WriteFile("ReaderFixtureBinary.fb2",
        "<?xml version=\"1.0\"?>\n"
        "<FictionBook><body><section><p>Texte.</p></section></body>"
        "<binary id=\"cover.jpg\" content-type=\"image/jpeg\">QUJDRA==</binary></FictionBook>");

    REQUIRE_FALSE(Path.empty());

    const std::optional<FTReaderDocument> Result = FTFb2DocumentExtractor().Extract(Path);

    REQUIRE(Result.has_value());
    REQUIRE(Result->Paragraphs.size() == 1);
    CHECK(Result->Paragraphs[0] == "Texte.");
}

TEST_CASE("Fb2 section titles become headings and chapters", "[reader][extractor]")
{
    const std::string Path = WriteFile("ReaderFixtureTitles.fb2",
        "<?xml version=\"1.0\"?>\n"
        "<FictionBook><body>"
        "<section><title><p>Partie I</p></title>"
        "<section><title><p>Chapitre 1</p><p>Le depart</p></title><p>Il part.</p></section>"
        "</section></body></FictionBook>");

    REQUIRE_FALSE(Path.empty());

    const std::optional<FTReaderDocument> Result = FTFb2DocumentExtractor().Extract(Path);

    REQUIRE(Result.has_value());
    REQUIRE(Result->Paragraphs.size() == 4);
    CHECK(Result->HeadingLevels == std::vector<int>{1, 2, 2, 0});
    REQUIRE(Result->Chapters.size() == 2);
    CHECK(Result->Chapters[0].Title == "Partie I");
    CHECK(Result->Chapters[0].Paragraph == 0);
    CHECK(Result->Chapters[0].Level == 1);
    CHECK(Result->Chapters[1].Title == "Chapitre 1. Le depart");
    CHECK(Result->Chapters[1].Paragraph == 1);
    CHECK(Result->Chapters[1].Level == 2);
}

TEST_CASE("Zipped fb2 extracts like a plain one", "[reader][extractor]")
{
    const std::string Path = WriteFile("ReaderFixture.fb2.zip", "");

    REQUIRE(WriteZip(Path.c_str(), { { "book.fb2",
        "<?xml version=\"1.0\"?><FictionBook><body><section><p>Dedans.</p></section></body>"
        "</FictionBook>" } }));

    const std::optional<FTReaderDocument> Result = FTFb2DocumentExtractor().Extract(Path);

    REQUIRE(Result.has_value());
    REQUIRE(Result->Paragraphs.size() == 1);
    CHECK(Result->Paragraphs[0] == "Dedans.");
}

TEST_CASE("Epub nav document gives chapters at their anchors", "[reader][extractor]")
{
    const std::string Path = WriteEpub("ReaderFixtureNav.epub",
        "<?xml version=\"1.0\"?><package version=\"3.0\" xmlns=\"http://www.idpf.org/2007/opf\">"
        "<manifest>"
        "<item id=\"nav\" href=\"nav.xhtml\" properties=\"nav\" media-type=\"application/xhtml+xml\"/>"
        "<item id=\"c1\" href=\"text/part.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "</manifest><spine><itemref idref=\"c1\"/></spine></package>",
        {
            { "OEBPS/nav.xhtml",
              "<?xml version=\"1.0\"?><html xmlns=\"http://www.w3.org/1999/xhtml\" "
              "xmlns:epub=\"http://www.idpf.org/2007/ops\"><body>"
              "<nav epub:type=\"toc\"><ol>"
              "<li><a href=\"text/part.xhtml\">Debut</a>"
              "<ol><li><a href=\"text/part.xhtml#second\"><span>Suite</span></a></li></ol></li>"
              "</ol></nav></body></html>" },
            { "OEBPS/text/part.xhtml",
              "<?xml version=\"1.0\"?><html><body>"
              "<p>Un.</p><p>Deux.</p><h2 id=\"second\">Suite</h2><p>Trois.</p>"
              "</body></html>" }
        });

    REQUIRE_FALSE(Path.empty());

    const std::optional<FTReaderDocument> Result = FTEpubDocumentExtractor().Extract(Path);

    REQUIRE(Result.has_value());
    REQUIRE(Result->Paragraphs.size() == 4);
    CHECK(Result->HeadingLevels == std::vector<int>{0, 0, 2, 0});
    REQUIRE(Result->Chapters.size() == 2);
    CHECK(Result->Chapters[0].Title == "Debut");
    CHECK(Result->Chapters[0].Paragraph == 0);
    CHECK(Result->Chapters[0].Level == 1);
    CHECK(Result->Chapters[1].Title == "Suite");
    CHECK(Result->Chapters[1].Paragraph == 2);
    CHECK(Result->Chapters[1].Level == 2);
}

TEST_CASE("Epub ncx gives chapters per spine file", "[reader][extractor]")
{
    const std::string Path = WriteEpub("ReaderFixtureNcx.epub",
        "<?xml version=\"1.0\"?><package version=\"2.0\" xmlns=\"http://www.idpf.org/2007/opf\">"
        "<manifest>"
        "<item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>"
        "<item id=\"c1\" href=\"one.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "<item id=\"c2\" href=\"two.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "</manifest><spine toc=\"ncx\"><itemref idref=\"c1\"/><itemref idref=\"c2\"/></spine>"
        "</package>",
        {
            { "OEBPS/toc.ncx",
              "<?xml version=\"1.0\"?><ncx xmlns=\"http://www.daisy.org/z3986/2005/ncx/\"><navMap>"
              "<navPoint id=\"a\"><navLabel><text>Premier</text></navLabel><content src=\"one.xhtml\"/>"
              "</navPoint>"
              "<navPoint id=\"b\"><navLabel><text>Second</text></navLabel><content src=\"two.xhtml\"/>"
              "</navPoint></navMap></ncx>" },
            { "OEBPS/one.xhtml", "<?xml version=\"1.0\"?><html><body><p>Premier</p><p>B.</p></body></html>" },
            { "OEBPS/two.xhtml", "<?xml version=\"1.0\"?><html><body><p>C.</p></body></html>" }
        });

    REQUIRE_FALSE(Path.empty());

    const std::optional<FTReaderDocument> Result = FTEpubDocumentExtractor().Extract(Path);

    REQUIRE(Result.has_value());
    REQUIRE(Result->Chapters.size() == 2);
    CHECK(Result->Chapters[0].Title == "Premier");
    CHECK(Result->Chapters[0].Paragraph == 0);
    CHECK(Result->Chapters[1].Title == "Second");
    CHECK(Result->Chapters[1].Paragraph == 2);
    CHECK(Result->HeadingLevels == std::vector<int>{1, 0, 0});
}

TEST_CASE("Epub html entities keep the text around them", "[reader][extractor]")
{
    const std::string Path = WriteEpub("ReaderFixtureEntities.epub",
        "<?xml version=\"1.0\"?><package version=\"2.0\" xmlns=\"http://www.idpf.org/2007/opf\">"
        "<manifest><item id=\"c1\" href=\"one.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "</manifest><spine><itemref idref=\"c1\"/></spine></package>",
        {
            { "OEBPS/one.xhtml",
              "<?xml version=\"1.0\"?><!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" "
              "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\"><html><body>"
              "<p>M.&nbsp;Dupont &mdash; ici.</p><p>Fin.</p></body></html>" }
        });

    REQUIRE_FALSE(Path.empty());

    const std::optional<FTReaderDocument> Result = FTEpubDocumentExtractor().Extract(Path);

    REQUIRE(Result.has_value());
    REQUIRE(Result->Paragraphs.size() == 2);
    CHECK(Result->Paragraphs[0] == "M. Dupont \xE2\x80\x94 ici.");
    CHECK(Result->Paragraphs[1] == "Fin.");
}

TEST_CASE("Epub italics, bold and line breaks reach the markup and keep the plain text aligned", "[reader][extractor]")
{
    const std::string Path = WriteEpub("ReaderFixtureInline.epub",
        "<?xml version=\"1.0\"?><package version=\"2.0\" xmlns=\"http://www.idpf.org/2007/opf\">"
        "<manifest><item id=\"c1\" href=\"one.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "</manifest><spine><itemref idref=\"c1\"/></spine></package>",
        {
            { "OEBPS/one.xhtml",
              "<?xml version=\"1.0\"?><html><body>"
              "<p>MADAME, <i>riant.\xE2\x80\x94</i>Et <b>toi</b> &amp; moi?</p>"
              "<p>Un<em> bourri</em>, c'est un \xC3\xA2ne.</p>"
              "<p>Premier vers,<br/>second vers.<br/></p>"
              "</body></html>" }
        });

    REQUIRE_FALSE(Path.empty());

    const std::optional<FTReaderDocument> Result = FTEpubDocumentExtractor().Extract(Path);

    REQUIRE(Result.has_value());
    REQUIRE(Result->Paragraphs.size() == 3);
    REQUIRE(Result->Markup.size() == 3);

    CHECK(Result->Paragraphs[0] == "MADAME, riant.\xE2\x80\x94" "Et toi & moi?");
    CHECK(Result->Markup[0] == "MADAME, <span style=\"font-style:italic\">riant.\xE2\x80\x94</span>Et "
                               "<span style=\"font-weight:700\">toi</span> &amp; moi?");

    CHECK(Result->Paragraphs[1] == "Un bourri, c'est un \xC3\xA2ne.");
    CHECK(Result->Markup[1] == "Un <span style=\"font-style:italic\">bourri</span>, c'est un \xC3\xA2ne.");

    CHECK(Result->Paragraphs[2] == "Premier vers,\xE2\x80\xA8second vers.");
    CHECK(Result->Markup[2] == "Premier vers,<br/>second vers.");
}

TEST_CASE("Epub paragraphs take their layout from the book's own stylesheet", "[reader][extractor]")
{
    const std::string Path = WriteEpub("ReaderFixtureStyles.epub",
        "<?xml version=\"1.0\"?><package version=\"2.0\" xmlns=\"http://www.idpf.org/2007/opf\">"
        "<manifest><item id=\"c1\" href=\"text/one.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "</manifest><spine><itemref idref=\"c1\"/></spine></package>",
        {
            { "OEBPS/styles/book.css",
              "p { margin-top: 1em; margin-bottom: 0 }"
              "body > p { text-indent: 1em; text-align: justify }"
              "h2 + p { text-indent: 0 }"
              "h2 { margin-top: 3em; margin-bottom: 2em }"
              ".center { text-align: center }"
              "p:first-child { text-indent: 4em }"
              "@media print { p { text-indent: 5em } }"
              ".hidden { display: none }" },
            { "OEBPS/text/one.xhtml",
              "<?xml version=\"1.0\"?><html><head>"
              "<link rel=\"stylesheet\" type=\"text/css\" href=\"../styles/book.css\"/></head><body>"
              "<h2>Titre</h2>"
              "<p>Premier.</p>"
              "<p>Deuxi\xC3\xA8me.</p>"
              "<div><p>Dans un bloc.</p></div>"
              "<p class=\"hidden\">Invisible.</p>"
              "<p class=\"center\" style=\"margin-top: 2em\">Milieu.</p>"
              "</body></html>" }
        });

    REQUIRE_FALSE(Path.empty());

    const std::optional<FTReaderDocument> Result = FTEpubDocumentExtractor().Extract(Path);

    REQUIRE(Result.has_value());
    REQUIRE(Result->Paragraphs.size() == 5);
    REQUIRE(Result->Layouts.size() == 5);

    const FTParagraphLayout& Heading = Result->Layouts[0];
    CHECK(Heading.FontScale == 1.5f);
    CHECK(Heading.bBold);
    CHECK(Heading.MarginTop == 4.5f);
    CHECK(Heading.MarginBottom == 3.0f);

    CHECK(Result->Layouts[1].TextIndent == 0.0f);
    CHECK(Result->Layouts[1].TextAlign == "justify");
    CHECK(Result->Layouts[1].MarginTop == 1.0f);

    CHECK(Result->Layouts[2].TextIndent == 1.0f);

    CHECK(Result->Layouts[3].TextIndent == 0.0f);
    CHECK(Result->Layouts[3].TextAlign.empty());

    CHECK(Result->Paragraphs[4] == "Milieu.");
    CHECK(Result->Layouts[4].TextAlign == "center");
    CHECK(Result->Layouts[4].MarginTop == 2.0f);
    CHECK(Result->Layouts[4].TextIndent == 1.0f);
}

TEST_CASE("Epub images become their own rows with the path and size of the picture", "[reader][extractor]")
{
    const std::string Path = WriteEpub("ReaderFixtureImages.epub",
        "<?xml version=\"1.0\"?><package version=\"2.0\" xmlns=\"http://www.idpf.org/2007/opf\">"
        "<manifest><item id=\"c1\" href=\"text/one.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "</manifest><spine><itemref idref=\"c1\"/></spine></package>",
        {
            { "OEBPS/images/a.gif", "GIF89a\x01\x01\x02\x01" "rest" },
            { "OEBPS/images/b.gif", "GIF89a\x03\x01\x04\x01" "rest" },
            { "OEBPS/text/one.xhtml",
              "<?xml version=\"1.0\"?><html xmlns=\"http://www.w3.org/1999/xhtml\" "
              "xmlns:xlink=\"http://www.w3.org/1999/xlink\"><body>"
              "<p>Avant.</p>"
              "<div class=\"figcenter\"><img src=\"../images/a.gif\" alt=\"\"/><p>L\xC3\xA9gende.</p></div>"
              "<p>Texte avec <img src=\"../images/a.gif\" alt=\"\"/> une image.</p>"
              "<div><svg><image xlink:href=\"../images/b.gif\"/></svg></div>"
              "<p><img src=\"../images/missing.gif\" alt=\"\"/>Fin.</p>"
              "</body></html>" }
        });

    REQUIRE_FALSE(Path.empty());

    const std::optional<FTReaderDocument> Result = FTEpubDocumentExtractor().Extract(Path);

    REQUIRE(Result.has_value());
    REQUIRE(Result->Paragraphs.size() == 6);
    REQUIRE(Result->Images.size() == 6);

    CHECK(Result->Paragraphs[0] == "Avant.");
    CHECK(Result->Images[0].Path.empty());

    CHECK(Result->Paragraphs[1].empty());
    CHECK(Result->Images[1].Path == "OEBPS/images/a.gif");
    CHECK(Result->Images[1].Width == 257);
    CHECK(Result->Images[1].Height == 258);

    CHECK(Result->Paragraphs[2] == "L\xC3\xA9gende.");
    CHECK(Result->Paragraphs[3] == "Texte avec une image.");

    CHECK(Result->Images[4].Path == "OEBPS/images/b.gif");
    CHECK(Result->Images[4].Width == 259);

    CHECK(Result->Paragraphs[5] == "Fin.");
    CHECK(Result->Images[5].Path.empty());
}
