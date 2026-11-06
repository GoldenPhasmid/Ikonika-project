#include "core/FTDocumentExtractor.h"

#include "core/FTCssStyle.h"
#include "core/FTLog.h"

#include "miniz.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringConverter>
#include <QStringList>
#include <QUrl>
#include <QXmlStreamReader>

#include <array>
#include <functional>
#include <cstring>

namespace
{
    std::optional<QByteArray> ReadBytes(const QString& FilePath)
    {
        QFile File(FilePath);

        if (!File.open(QIODevice::ReadOnly))
        {
            return std::nullopt;
        }

        return File.readAll();
    }

    QString DecodeWindows1251(const QByteArray& Bytes)
    {
        static constexpr std::array<char16_t, 64> UpperHalf{
            0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
            0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
            0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
            0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
            0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
            0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
            0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
            0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457
        };

        QString Result;
        Result.reserve(Bytes.size());

        for (const char Byte : Bytes)
        {
            const unsigned char Code = static_cast<unsigned char>(Byte);

            if (Code < 0x80)
            {
                Result += QChar(Code);
            }
            else if (Code < 0xC0)
            {
                Result += QChar(UpperHalf[Code - 0x80]);
            }
            else
            {
                Result += QChar(static_cast<char16_t>(0x0410 + (Code - 0xC0)));
            }
        }

        return Result;
    }

    QString DeclaredXmlEncoding(const QByteArray& Bytes)
    {
        static const QRegularExpression Declaration(
            QStringLiteral(R"(<\?xml[^>]*encoding\s*=\s*["']([^"']+)["'])"));

        const QRegularExpressionMatch Match =
            Declaration.match(QString::fromLatin1(Bytes.left(256)));

        return Match.hasMatch() ? Match.captured(1).trimmed().toLower() : QString();
    }

    QString DecodeXml(const QByteArray& Bytes)
    {
        const QString Encoding = DeclaredXmlEncoding(Bytes);

        if (Encoding == QStringLiteral("windows-1251") || Encoding == QStringLiteral("cp1251"))
        {
            return DecodeWindows1251(Bytes);
        }

        if (!Encoding.isEmpty() && Encoding != QStringLiteral("utf-8"))
        {
            QStringDecoder Decoder(Encoding.toLatin1().constData());

            if (Decoder.isValid())
            {
                return Decoder.decode(Bytes);
            }
        }

        return QString::fromUtf8(Bytes);
    }

    std::optional<QByteArray> ReadFirstZipEntryEndingWith(const QByteArray& Archive,
                                                          const QString& Suffix)
    {
        mz_zip_archive Zip;
        std::memset(&Zip, 0, sizeof(Zip));

        if (!mz_zip_reader_init_mem(&Zip, Archive.constData(), static_cast<size_t>(Archive.size()), 0))
        {
            return std::nullopt;
        }

        std::optional<QByteArray> Result;

        for (mz_uint Index = 0; Index < mz_zip_reader_get_num_files(&Zip); ++Index)
        {
            char Name[512];
            mz_zip_reader_get_filename(&Zip, Index, Name, sizeof(Name));

            if (!QString::fromUtf8(Name).endsWith(Suffix, Qt::CaseInsensitive))
            {
                continue;
            }

            size_t Size = 0;
            void* Data = mz_zip_reader_extract_to_heap(&Zip, Index, &Size, 0);

            if (Data != nullptr)
            {
                Result = QByteArray(static_cast<const char*>(Data), static_cast<int>(Size));
                MZ_FREE(Data);
            }

            break;
        }

        mz_zip_reader_end(&Zip);

        return Result;
    }

    QByteArray ReplaceHtmlEntities(QByteArray Xhtml)
    {
        static const std::array<std::pair<const char*, const char*>, 12> Entities{{
            {"&nbsp;", "&#160;"}, {"&mdash;", "&#8212;"}, {"&ndash;", "&#8211;"},
            {"&laquo;", "&#171;"}, {"&raquo;", "&#187;"}, {"&hellip;", "&#8230;"},
            {"&lsquo;", "&#8216;"}, {"&rsquo;", "&#8217;"}, {"&ldquo;", "&#8220;"},
            {"&rdquo;", "&#8221;"}, {"&copy;", "&#169;"}, {"&shy;", "&#173;"}
        }};

        for (const auto& [Name, Reference] : Entities)
        {
            Xhtml.replace(Name, Reference);
        }

        return Xhtml;
    }

    bool IsBlockElement(QStringView Name)
    {
        static const QSet<QString> Blocks = {
            QStringLiteral("p"), QStringLiteral("div"), QStringLiteral("li"),
            QStringLiteral("blockquote"), QStringLiteral("pre"), QStringLiteral("td"),
            QStringLiteral("th"), QStringLiteral("tr"), QStringLiteral("h1"),
            QStringLiteral("h2"), QStringLiteral("h3"), QStringLiteral("h4"),
            QStringLiteral("h5"), QStringLiteral("h6"), QStringLiteral("title"),
            QStringLiteral("subtitle"), QStringLiteral("poem"), QStringLiteral("stanza"),
            QStringLiteral("epigraph"), QStringLiteral("cite"), QStringLiteral("annotation"),
            QStringLiteral("section"), QStringLiteral("center"), QStringLiteral("hr")
        };

        return Blocks.contains(Name.toString());
    }

    bool IsInlineElement(QStringView Name)
    {
        static const QSet<QString> Inline = {
            QStringLiteral("i"), QStringLiteral("em"), QStringLiteral("b"), QStringLiteral("strong"),
            QStringLiteral("emphasis"), QStringLiteral("dfn"), QStringLiteral("var"), QStringLiteral("span"),
            QStringLiteral("a"), QStringLiteral("small"), QStringLiteral("big"), QStringLiteral("sup"),
            QStringLiteral("sub"), QStringLiteral("u"), QStringLiteral("s"), QStringLiteral("abbr"),
            QStringLiteral("code"), QStringLiteral("q"), QStringLiteral("mark"), QStringLiteral("font"),
            QStringLiteral("tt"), QStringLiteral("strikethrough")
        };

        return Inline.contains(Name.toString());
    }

    std::optional<std::pair<int, int>> ReadImageSize(const QByteArray& Bytes)
    {
        const auto Byte = [&Bytes](qsizetype Index) { return static_cast<unsigned char>(Bytes[Index]); };
        const auto BigEndian16 = [&Byte](qsizetype Index) { return Byte(Index) << 8 | Byte(Index + 1); };

        if (Bytes.size() >= 24 && Bytes.startsWith("\x89PNG"))
        {
            return std::make_pair(BigEndian16(16) << 16 | BigEndian16(18), BigEndian16(20) << 16 | BigEndian16(22));
        }

        if (Bytes.size() >= 10 && (Bytes.startsWith("GIF87a") || Bytes.startsWith("GIF89a")))
        {
            return std::make_pair(Byte(6) | Byte(7) << 8, Byte(8) | Byte(9) << 8);
        }

        if (Bytes.size() < 4 || Byte(0) != 0xFF || Byte(1) != 0xD8)
        {
            return std::nullopt;
        }

        qsizetype Position = 2;

        while (Position + 9 < Bytes.size())
        {
            if (Byte(Position) != 0xFF)
            {
                return std::nullopt;
            }

            const int Marker = Byte(Position + 1);
            const int Length = BigEndian16(Position + 2);
            const bool bFrame = Marker >= 0xC0 && Marker <= 0xCF && Marker != 0xC4 && Marker != 0xC8 && Marker != 0xCC;

            if (bFrame)
            {
                return std::make_pair(BigEndian16(Position + 7), BigEndian16(Position + 5));
            }

            Position += 2 + Length;
        }

        return std::nullopt;
    }

    FTCssElement ReadElement(QStringView Name, const QXmlStreamAttributes& Attributes)
    {
        return {Name.toString().toLower(), Attributes.value(QStringLiteral("id")).toString(),
                Attributes.value(QStringLiteral("class")).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts)};
    }

    FTParagraphLayout ToLayout(const FTCssComputedStyle& Style, const FTCssLength& InsetLeft, const FTCssLength& InsetRight)
    {
        FTParagraphLayout Layout;
        Layout.FontScale = Style.FontScale;
        Layout.TextIndent = Style.TextIndent.Em;
        Layout.TextIndentWidth = Style.TextIndent.Width;
        Layout.MarginTop = Style.MarginTop.Em + Style.PaddingTop.Em;
        Layout.MarginTopWidth = Style.MarginTop.Width + Style.PaddingTop.Width;
        Layout.MarginBottom = Style.MarginBottom.Em + Style.PaddingBottom.Em;
        Layout.MarginBottomWidth = Style.MarginBottom.Width + Style.PaddingBottom.Width;
        Layout.MarginLeft = InsetLeft.Em;
        Layout.MarginLeftWidth = InsetLeft.Width;
        Layout.MarginRight = InsetRight.Em;
        Layout.MarginRightWidth = InsetRight.Width;
        Layout.TextAlign = Style.TextAlign.toStdString();
        Layout.TextTransform = Style.TextTransform.toStdString();
        Layout.bBold = Style.bBold;
        Layout.bItalic = Style.bItalic;
        Layout.bSmallCaps = Style.bSmallCaps;

        return Layout;
    }

    class FTParagraphBuilder
    {
    public:
        struct FTBuiltParagraph
        {
            QString Plain;
            QString Markup;
            FTParagraphLayout Layout;
        };

        void AddText(QStringView Text)
        {
            for (const QChar Character : Text)
            {
                if (Character.isSpace())
                {
                    bPendingSpace = true;

                    continue;
                }

                if (bBoundary && !Plain.isEmpty() && Plain.back() != LineSeparator)
                {
                    bPendingSpace = true;
                }

                bBoundary = false;

                CloseSpans(SharedSpanCount());

                if (bPendingSpace && !Plain.isEmpty() && Plain.back() != LineSeparator)
                {
                    Plain += QLatin1Char(' ');
                    Markup += QLatin1Char(' ');
                }

                bPendingSpace = false;

                OpenSpans();
                Plain += Character;
                AppendEscaped(Character);
            }
        }

        void AddBreak()
        {
            if (Plain.isEmpty())
            {
                return;
            }

            Plain += LineSeparator;
            Markup += QStringLiteral("<br/>");
            bPendingSpace = false;
            bBoundary = false;
        }

        void MarkBoundary()
        {
            bBoundary = true;
        }

        void PushSpan(const QString& Style)
        {
            DesiredSpans.append(Style);
        }

        void PopSpan()
        {
            if (!DesiredSpans.isEmpty())
            {
                DesiredSpans.removeLast();
            }
        }

        bool HasText() const
        {
            return !Plain.isEmpty();
        }

        FTParagraphLayout TakeImageLayout()
        {
            FTParagraphLayout Taken = Layout;
            Layout.MarginTop = 0.0f;
            Layout.MarginTopWidth = 0.0f;

            return Taken;
        }

        void MergeLayout(const FTParagraphLayout& Block)
        {
            if (!Plain.isEmpty())
            {
                return;
            }

            const float MarginTop = qMax(Layout.MarginTop, Block.MarginTop);
            const float MarginTopWidth = qMax(Layout.MarginTopWidth, Block.MarginTopWidth);

            Layout = Block;
            Layout.MarginTop = MarginTop;
            Layout.MarginTopWidth = MarginTopWidth;
        }

        std::optional<FTBuiltParagraph> Take()
        {
            while (Plain.endsWith(LineSeparator))
            {
                Plain.chop(1);
                Markup.chop(5);
            }

            bPendingSpace = false;
            bBoundary = false;

            if (Plain.isEmpty())
            {
                Markup.clear();

                return std::nullopt;
            }

            CloseSpans(0);

            FTBuiltParagraph Built{Plain, Markup, Layout};

            Plain.clear();
            Markup.clear();
            Layout = {};

            return Built;
        }

    private:
        static constexpr QChar LineSeparator{0x2028};

        qsizetype SharedSpanCount() const
        {
            qsizetype Count = 0;

            while (Count < OpenedSpans.size() && Count < DesiredSpans.size() && OpenedSpans[Count] == DesiredSpans[Count])
            {
                ++Count;
            }

            return Count;
        }

        void CloseSpans(qsizetype Keep)
        {
            while (OpenedSpans.size() > Keep)
            {
                if (!OpenedSpans.last().isEmpty())
                {
                    Markup += QStringLiteral("</span>");
                }

                OpenedSpans.removeLast();
            }
        }

        void OpenSpans()
        {
            CloseSpans(SharedSpanCount());

            for (qsizetype Index = OpenedSpans.size(); Index < DesiredSpans.size(); ++Index)
            {
                if (!DesiredSpans[Index].isEmpty())
                {
                    Markup += QStringLiteral("<span style=\"") + DesiredSpans[Index] + QStringLiteral("\">");
                }

                OpenedSpans.append(DesiredSpans[Index]);
            }
        }

        void AppendEscaped(QChar Character)
        {
            if (Character == QLatin1Char('&'))
            {
                Markup += QStringLiteral("&amp;");
            }
            else if (Character == QLatin1Char('<'))
            {
                Markup += QStringLiteral("&lt;");
            }
            else if (Character == QLatin1Char('>'))
            {
                Markup += QStringLiteral("&gt;");
            }
            else
            {
                Markup += Character;
            }
        }

        QString Plain;
        QString Markup;
        FTParagraphLayout Layout;
        QStringList DesiredSpans;
        QStringList OpenedSpans;
        bool bPendingSpace = false;
        bool bBoundary = false;
    };

    bool IsSkippedElement(QStringView Name)
    {
        static const QSet<QString> Skipped = {
            QStringLiteral("script"), QStringLiteral("style"), QStringLiteral("head"),
            QStringLiteral("image"), QStringLiteral("img"), QStringLiteral("svg"),
            QStringLiteral("object"), QStringLiteral("math"), QStringLiteral("description"),
            QStringLiteral("binary")
        };

        return Skipped.contains(Name.toString());
    }

    int HeadingLevelOf(QStringView Name, int SectionDepth)
    {
        if (Name.size() == 2 && Name[0] == QLatin1Char('h') && Name[1] >= QLatin1Char('1')
            && Name[1] <= QLatin1Char('6'))
        {
            return Name[1].unicode() - '0';
        }

        if (Name == QStringLiteral("title"))
        {
            return qMax(1, SectionDepth);
        }

        return 0;
    }

    void LogUnhandledCss(const QString& Title, const QHash<QString, int>& Unhandled,
                         const QHash<QString, FTCssStyleSheet>& StyleSheets)
    {
        QStringList Declarations;

        for (auto Entry = Unhandled.cbegin(); Entry != Unhandled.cend(); ++Entry)
        {
            Declarations.append(QStringLiteral("%1 (%2)").arg(Entry.key()).arg(Entry.value()));
        }

        QSet<QString> Rules;

        for (const FTCssStyleSheet& Sheet : StyleSheets)
        {
            for (const QString& Rule : Sheet.GetSkippedRules())
            {
                Rules.insert(Rule);
            }
        }

        QStringList SortedRules = Rules.values();
        Declarations.sort();
        SortedRules.sort();

        if (!Declarations.isEmpty())
        {
            qCDebug(LogApp).noquote() << "Reader: unhandled CSS in" << Title << "|" << Declarations.join(QStringLiteral("; "));
        }

        if (!SortedRules.isEmpty())
        {
            qCDebug(LogApp).noquote() << "Reader: skipped CSS rules in" << Title << "|" << SortedRules.join(QStringLiteral("; "));
        }
    }

    using FTImageResolver = std::function<std::optional<FTParagraphImage>(const QString&)>;

    void CollectBlockText(QXmlStreamReader& Reader, FTReaderDocument& Document, const FTCssStyleSheet& Sheet,
                          QHash<QString, int>* Anchors = nullptr, const FTImageResolver& ResolveImage = {},
                          QHash<QString, int>* UnhandledCss = nullptr)
    {
        struct FTStyleFrame
        {
            FTCssComputedStyle Style;
            std::optional<FTCssElement> LastChild;
            size_t FirstParagraph = 0;
            FTCssLength InsetLeft;
            FTCssLength InsetRight;
        };

        FTParagraphBuilder Builder;
        std::vector<FTStyleFrame> Frames;
        std::vector<FTCssElement> Path;
        int SkipDepth = 0;
        int SvgDepth = 0;
        int SectionDepth = 0;
        int HeadingLevel = 0;
        int HeadingStart = 0;

        const auto Flush = [&Document, &Builder, &HeadingLevel]()
        {
            std::optional<FTParagraphBuilder::FTBuiltParagraph> Built = Builder.Take();

            if (!Built.has_value())
            {
                return;
            }

            Document.Paragraphs.push_back(Built->Plain.toStdString());
            Document.HeadingLevels.push_back(HeadingLevel);
            Document.Markup.push_back(Built->Markup.toStdString());
            Document.Layouts.push_back(Built->Layout);
            Document.Images.push_back({});
        };

        const auto AddEmptyRow = [&Document, &Builder](const FTParagraphImage& Image, bool bRule)
        {
            FTParagraphLayout Layout = Builder.TakeImageLayout();
            Layout.bRule = bRule;

            Document.Paragraphs.emplace_back();
            Document.HeadingLevels.push_back(0);
            Document.Markup.emplace_back();
            Document.Layouts.push_back(Layout);
            Document.Images.push_back(Image);
        };

        const auto AddImage = [&Builder, &ResolveImage, &AddEmptyRow](const QString& Source)
        {
            if (!ResolveImage || Source.isEmpty() || Builder.HasText())
            {
                return;
            }

            const std::optional<FTParagraphImage> Image = ResolveImage(Source);

            if (!Image.has_value())
            {
                return;
            }

            AddEmptyRow(*Image, false);
        };

        const auto CloseHeading = [&Document, &HeadingLevel, &HeadingStart]()
        {
            const int End = static_cast<int>(Document.Paragraphs.size());

            if (End > HeadingStart)
            {
                QString Title = QString::fromStdString(Document.Paragraphs[static_cast<size_t>(HeadingStart)]);

                for (int Index = HeadingStart + 1; Index < End; ++Index)
                {
                    Title += QStringLiteral(". ") + QString::fromStdString(Document.Paragraphs[static_cast<size_t>(Index)]);
                }

                Title.replace(QChar(0x2028), QLatin1Char(' '));

                Document.Chapters.push_back({Title.toStdString(), HeadingStart, HeadingLevel});
            }

            HeadingLevel = 0;
        };

        while (!Reader.atEnd())
        {
            switch (Reader.readNext())
            {
            case QXmlStreamReader::StartElement:
            {
                if (SkipDepth > 0)
                {
                    ++SkipDepth;

                    if (SvgDepth > 0 && Reader.name() == QStringLiteral("image"))
                    {
                        const QXmlStreamAttributes Attributes = Reader.attributes();
                        const QString Link = Attributes.value(QStringLiteral("http://www.w3.org/1999/xlink"), QStringLiteral("href")).toString();

                        AddImage(Link.isEmpty() ? Attributes.value(QStringLiteral("href")).toString() : Link);
                    }

                    break;
                }

                if (Reader.name() == QStringLiteral("img"))
                {
                    AddImage(Reader.attributes().value(QStringLiteral("src")).toString());
                    SkipDepth = 1;

                    break;
                }

                Path.push_back(ReadElement(Reader.name(), Reader.attributes()));

                const FTCssElement* PreviousSibling =
                    Frames.empty() || !Frames.back().LastChild.has_value() ? nullptr : &*Frames.back().LastChild;
                const FTCssComputedStyle Parent = Frames.empty() ? FTCssComputedStyle{} : Frames.back().Style;

                FTCssDeclarations Declarations = Sheet.Match(Path, PreviousSibling);
                const FTCssDeclarations Inline =
                    FTCssStyleSheet::ParseDeclarations(Reader.attributes().value(QStringLiteral("style")).toString());
                Declarations.insert(Declarations.end(), Inline.begin(), Inline.end());

                FTCssComputedStyle Style = Parent.Inherit();
                Style.Apply(Declarations, UnhandledCss);

                if (Style.bHidden || IsSkippedElement(Reader.name()))
                {
                    Path.pop_back();
                    SkipDepth = 1;
                    SvgDepth = !Style.bHidden && Reader.name() == QStringLiteral("svg") ? 1 : 0;

                    break;
                }

                FTCssLength InsetLeft = Frames.empty() ? FTCssLength{} : Frames.back().InsetLeft;
                FTCssLength InsetRight = Frames.empty() ? FTCssLength{} : Frames.back().InsetRight;

                if (IsBlockElement(Reader.name()))
                {
                    InsetLeft.Em += Style.MarginLeft.Em + Style.PaddingLeft.Em;
                    InsetLeft.Width += Style.MarginLeft.Width + Style.PaddingLeft.Width;
                    InsetRight.Em += Style.MarginRight.Em + Style.PaddingRight.Em;
                    InsetRight.Width += Style.MarginRight.Width + Style.PaddingRight.Width;
                }

                Frames.push_back({Style, std::nullopt, Document.Paragraphs.size(), InsetLeft, InsetRight});

                if (Anchors != nullptr)
                {
                    const QString Id = Reader.attributes().value(QStringLiteral("id")).toString();

                    if (!Id.isEmpty() && !Anchors->contains(Id))
                    {
                        Anchors->insert(Id, static_cast<int>(Document.Paragraphs.size()));
                    }
                }

                if (Reader.name() == QStringLiteral("section"))
                {
                    ++SectionDepth;
                }

                if (Reader.name() == QStringLiteral("br"))
                {
                    Builder.AddBreak();

                    break;
                }

                if (IsBlockElement(Reader.name()))
                {
                    Flush();
                    Builder.MergeLayout(ToLayout(Style, InsetLeft, InsetRight));
                }

                if (Reader.name() == QStringLiteral("hr"))
                {
                    AddEmptyRow({}, true);
                }

                if (IsInlineElement(Reader.name()))
                {
                    Builder.PushSpan(Style.InlineDifference(Parent));
                }
                else
                {
                    Builder.MarkBoundary();
                }

                const int Level = HeadingLevelOf(Reader.name(), SectionDepth);

                if (Level > 0 && HeadingLevel == 0)
                {
                    HeadingLevel = Level;
                    HeadingStart = static_cast<int>(Document.Paragraphs.size());
                }

                break;
            }

            case QXmlStreamReader::EndElement:
            {
                if (SkipDepth > 0)
                {
                    --SkipDepth;
                    SvgDepth = SkipDepth > 0 ? SvgDepth : 0;

                    break;
                }

                if (Frames.empty())
                {
                    break;
                }

                const FTStyleFrame Closed = Frames.back();
                const FTCssElement ClosedElement = Path.back();

                Frames.pop_back();
                Path.pop_back();

                if (!Frames.empty())
                {
                    Frames.back().LastChild = ClosedElement;
                }

                if (IsBlockElement(Reader.name()))
                {
                    Flush();

                    if (Document.Layouts.size() > Closed.FirstParagraph)
                    {
                        FTParagraphLayout& Last = Document.Layouts.back();
                        Last.MarginBottom = qMax(Last.MarginBottom, Closed.Style.MarginBottom.Em + Closed.Style.PaddingBottom.Em);
                        Last.MarginBottomWidth = qMax(Last.MarginBottomWidth,
                                                      Closed.Style.MarginBottom.Width + Closed.Style.PaddingBottom.Width);
                    }
                }

                if (IsInlineElement(Reader.name()))
                {
                    Builder.PopSpan();
                }
                else
                {
                    Builder.MarkBoundary();
                }

                if (HeadingLevel > 0 && HeadingLevelOf(Reader.name(), SectionDepth) > 0)
                {
                    CloseHeading();
                }

                if (Reader.name() == QStringLiteral("section"))
                {
                    SectionDepth = qMax(0, SectionDepth - 1);
                }

                break;
            }

            case QXmlStreamReader::Characters:
            {
                if (SkipDepth == 0)
                {
                    Builder.AddText(Reader.text());
                }

                break;
            }

            default:
                break;
            }
        }

        Flush();
    }

    QString ReadFb2Title(QXmlStreamReader& Reader)
    {
        while (!Reader.atEnd())
        {
            if (Reader.readNext() == QXmlStreamReader::StartElement
                && Reader.name() == QStringLiteral("book-title"))
            {
                return Reader.readElementText().trimmed();
            }
        }

        return {};
    }

    struct FTEpubTocEntry
    {
        QString Title;
        QString Target;
        int Level = 1;
    };

    QString ResolveZipPath(const QString& BaseFile, const QString& Href)
    {
        const QString Directory = BaseFile.section(QStringLiteral("/"), 0, -2);
        const QString Decoded = QUrl::fromPercentEncoding(Href.toUtf8());
        const QString Joined = Directory.isEmpty() ? Decoded : Directory + QStringLiteral("/") + Decoded;

        return QDir::cleanPath(Joined).replace(QStringLiteral("\\"), QStringLiteral("/"));
    }

    std::vector<FTEpubTocEntry> ReadNcxToc(const QByteArray& Ncx, const QString& NcxPath)
    {
        std::vector<FTEpubTocEntry> Entries;
        QXmlStreamReader Reader(Ncx);
        int Depth = 0;
        QString Label;

        while (!Reader.atEnd())
        {
            switch (Reader.readNext())
            {
            case QXmlStreamReader::StartElement:
                if (Reader.name() == QStringLiteral("navPoint"))
                {
                    ++Depth;
                    Label.clear();
                }
                else if (Reader.name() == QStringLiteral("text") && Depth > 0 && Label.isEmpty())
                {
                    Label = Reader.readElementText().simplified();
                }
                else if (Reader.name() == QStringLiteral("content") && Depth > 0)
                {
                    const QString Source = Reader.attributes().value(QStringLiteral("src")).toString();

                    if (!Source.isEmpty() && !Label.isEmpty())
                    {
                        Entries.push_back({Label, ResolveZipPath(NcxPath, Source), Depth});
                    }
                }

                break;

            case QXmlStreamReader::EndElement:
                if (Reader.name() == QStringLiteral("navPoint"))
                {
                    --Depth;
                }

                break;

            default:
                break;
            }
        }

        return Entries;
    }

    std::vector<FTEpubTocEntry> ReadNavToc(const QByteArray& Nav, const QString& NavPath)
    {
        std::vector<FTEpubTocEntry> Entries;
        QXmlStreamReader Reader(ReplaceHtmlEntities(Nav));
        int NavDepth = 0;
        int ListDepth = 0;
        bool bInToc = false;
        bool bTocSeen = false;

        while (!Reader.atEnd())
        {
            switch (Reader.readNext())
            {
            case QXmlStreamReader::StartElement:
                if (Reader.name() == QStringLiteral("nav"))
                {
                    ++NavDepth;

                    const QString Type = Reader.attributes().value(
                        QStringLiteral("http://www.idpf.org/2007/ops"), QStringLiteral("type")).toString();

                    if (!bTocSeen && (Type.contains(QStringLiteral("toc")) || Type.isEmpty()))
                    {
                        bInToc = true;
                        bTocSeen = true;
                    }
                }
                else if (bInToc && Reader.name() == QStringLiteral("ol"))
                {
                    ++ListDepth;
                }
                else if (bInToc && Reader.name() == QStringLiteral("a"))
                {
                    const QString Href = Reader.attributes().value(QStringLiteral("href")).toString();
                    const QString Label =
                        Reader.readElementText(QXmlStreamReader::IncludeChildElements).simplified();

                    if (!Href.isEmpty() && !Label.isEmpty())
                    {
                        Entries.push_back({Label, ResolveZipPath(NavPath, Href), qMax(1, ListDepth)});
                    }
                }

                break;

            case QXmlStreamReader::EndElement:
                if (Reader.name() == QStringLiteral("nav"))
                {
                    --NavDepth;

                    if (NavDepth == 0)
                    {
                        bInToc = false;
                    }
                }
                else if (bInToc && Reader.name() == QStringLiteral("ol"))
                {
                    --ListDepth;
                }

                break;

            default:
                break;
            }
        }

        return Entries;
    }
}

std::optional<FTReaderDocument> FTTxtDocumentExtractor::Extract(const std::string& FilePath) const
{
    const QString Path = QString::fromStdString(FilePath);
    const std::optional<QByteArray> Bytes = ReadBytes(Path);

    if (!Bytes.has_value())
    {
        return std::nullopt;
    }

    const QString Content = QString::fromUtf8(*Bytes);

    FTReaderDocument Result;
    Result.Title = QFileInfo(Path).completeBaseName().toStdString();

    QString Pending;

    for (const QString& Line : Content.split(QStringLiteral("\n")))
    {
        if (Line.trimmed().isEmpty())
        {
            if (!Pending.trimmed().isEmpty())
            {
                Result.Paragraphs.push_back(Pending.trimmed().toStdString());
                Pending.clear();
            }

            continue;
        }

        if (!Pending.isEmpty())
        {
            Pending += QLatin1Char(' ');
        }

        Pending += Line.trimmed();
    }

    if (!Pending.trimmed().isEmpty())
    {
        Result.Paragraphs.push_back(Pending.trimmed().toStdString());
    }

    Result.HeadingLevels.assign(Result.Paragraphs.size(), 0);

    return Result;
}

std::optional<FTReaderDocument> FTFb2DocumentExtractor::Extract(const std::string& FilePath) const
{
    std::optional<QByteArray> Bytes = ReadBytes(QString::fromStdString(FilePath));

    if (!Bytes.has_value())
    {
        return std::nullopt;
    }

    if (Bytes->startsWith("PK"))
    {
        Bytes = ReadFirstZipEntryEndingWith(*Bytes, QStringLiteral(".fb2"));

        if (!Bytes.has_value())
        {
            return std::nullopt;
        }
    }

    const QString Content = DecodeXml(*Bytes);

    FTReaderDocument Result;

    QXmlStreamReader TitleReader(Content);
    Result.Title = ReadFb2Title(TitleReader).toStdString();

    QXmlStreamReader Body(Content);
    CollectBlockText(Body, Result, FTCssStyleSheet::ForFb2());

    return Result;
}

std::optional<FTReaderDocument> FTEpubDocumentExtractor::Extract(const std::string& FilePath) const
{
    const QString Path = QString::fromStdString(FilePath);
    const std::optional<QByteArray> Bytes = ReadBytes(Path);

    if (!Bytes.has_value())
    {
        return std::nullopt;
    }

    mz_zip_archive Zip;
    std::memset(&Zip, 0, sizeof(Zip));

    if (!mz_zip_reader_init_mem(&Zip, Bytes->constData(), static_cast<size_t>(Bytes->size()), 0))
    {
        return std::nullopt;
    }

    const auto ReadEntry = [&Zip](const QString& Name) -> std::optional<QByteArray>
    {
        size_t Size = 0;
        void* Data = mz_zip_reader_extract_file_to_heap(&Zip, Name.toUtf8().constData(), &Size, 0);

        if (Data == nullptr)
        {
            return std::nullopt;
        }

        QByteArray Result(static_cast<const char*>(Data), static_cast<int>(Size));
        MZ_FREE(Data);

        return Result;
    };

    const std::optional<QByteArray> Container = ReadEntry(QStringLiteral("META-INF/container.xml"));

    if (!Container.has_value())
    {
        mz_zip_reader_end(&Zip);

        return std::nullopt;
    }

    QXmlStreamReader ContainerReader(*Container);
    QString OpfPath;

    while (!ContainerReader.atEnd())
    {
        if (ContainerReader.readNext() == QXmlStreamReader::StartElement
            && ContainerReader.name() == QStringLiteral("rootfile"))
        {
            OpfPath = ContainerReader.attributes().value(QStringLiteral("full-path")).toString();
            break;
        }
    }

    if (OpfPath.isEmpty())
    {
        mz_zip_reader_end(&Zip);

        return std::nullopt;
    }

    const std::optional<QByteArray> OpfData = ReadEntry(OpfPath);

    if (!OpfData.has_value())
    {
        mz_zip_reader_end(&Zip);

        return std::nullopt;
    }

    QXmlStreamReader OpfReader(*OpfData);

    QString Title;
    QString NcxId;
    QString NavHref;
    QHash<QString, QString> ManifestById;
    QHash<QString, QString> MediaTypeById;
    QStringList SpineOrder;
    bool bInManifest = false;
    bool bInSpine = false;

    while (!OpfReader.atEnd())
    {
        switch (OpfReader.readNext())
        {
        case QXmlStreamReader::StartElement:
        {
            if (OpfReader.namespaceUri() == QStringLiteral("http://purl.org/dc/elements/1.1/")
                && OpfReader.qualifiedName() == QStringLiteral("dc:title"))
            {
                Title = OpfReader.readElementText().trimmed();
            }
            else if (OpfReader.name() == QStringLiteral("manifest"))
            {
                bInManifest = true;
            }
            else if (OpfReader.name() == QStringLiteral("spine"))
            {
                bInSpine = true;
                NcxId = OpfReader.attributes().value(QStringLiteral("toc")).toString();
            }
            else if (bInManifest && OpfReader.name() == QStringLiteral("item"))
            {
                const QXmlStreamAttributes Attributes = OpfReader.attributes();
                const QString Id = Attributes.value(QStringLiteral("id")).toString();
                const QString Href = Attributes.value(QStringLiteral("href")).toString();
                const QString Properties = Attributes.value(QStringLiteral("properties")).toString();

                if (!Id.isEmpty())
                {
                    ManifestById.insert(Id, Href);
                    MediaTypeById.insert(Id, Attributes.value(QStringLiteral("media-type")).toString());
                }

                if (Properties.split(QLatin1Char(' ')).contains(QStringLiteral("nav")))
                {
                    NavHref = Href;
                }
            }
            else if (bInSpine && OpfReader.name() == QStringLiteral("itemref"))
            {
                const QString Id = OpfReader.attributes().value(QStringLiteral("idref")).toString();

                if (!Id.isEmpty())
                {
                    SpineOrder.append(Id);
                }
            }

            break;
        }

        case QXmlStreamReader::EndElement:
        {
            if (OpfReader.name() == QStringLiteral("manifest"))
            {
                bInManifest = false;
            }
            else if (OpfReader.name() == QStringLiteral("spine"))
            {
                bInSpine = false;
            }

            break;
        }

        default:
            break;
        }
    }

    if (SpineOrder.isEmpty())
    {
        mz_zip_reader_end(&Zip);

        return std::nullopt;
    }

    FTReaderDocument Result;
    Result.Title = Title.toStdString();

    QHash<QString, int> FileStarts;
    QHash<QString, QHash<QString, int>> FileAnchors;
    QHash<QString, FTCssStyleSheet> StyleSheets;
    QHash<QString, int> UnhandledCss;

    const auto StyleSheetFor = [&ReadEntry, &StyleSheets](const QByteArray& Content, const QString& Entry) -> const FTCssStyleSheet&
    {
        static const QRegularExpression Link(QStringLiteral("<link\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression Href(QStringLiteral("href\\s*=\\s*[\"']([^\"']+)[\"']"));
        static const QRegularExpression Embedded(QStringLiteral("<style\\b[^>]*>(.*?)</style>"),
                                                 QRegularExpression::CaseInsensitiveOption
                                                     | QRegularExpression::DotMatchesEverythingOption);

        const QString Text = QString::fromUtf8(Content);
        QStringList Files;
        QStringList Embeds;

        for (QRegularExpressionMatchIterator Links = Link.globalMatch(Text); Links.hasNext();)
        {
            const QString Tag = Links.next().captured(0);
            const QRegularExpressionMatch Target = Href.match(Tag);

            if (Tag.contains(QStringLiteral("stylesheet"), Qt::CaseInsensitive) && Target.hasMatch())
            {
                Files.append(ResolveZipPath(Entry, Target.captured(1)));
            }
        }

        for (QRegularExpressionMatchIterator Styles = Embedded.globalMatch(Text); Styles.hasNext();)
        {
            Embeds.append(Styles.next().captured(1));
        }

        const QString Key = Files.join(QLatin1Char('\n')) + QChar(0) + Embeds.join(QChar(0));
        const auto Found = StyleSheets.constFind(Key);

        if (Found != StyleSheets.cend())
        {
            return *Found;
        }

        FTCssStyleSheet Sheet = FTCssStyleSheet::ForXhtml();

        for (const QString& File : Files)
        {
            if (const std::optional<QByteArray> Css = ReadEntry(File); Css.has_value())
            {
                Sheet.Append(QString::fromUtf8(*Css));
            }
        }

        for (const QString& Css : Embeds)
        {
            Sheet.Append(Css);
        }

        return *StyleSheets.insert(Key, Sheet);
    };

    for (const QString& Id : SpineOrder)
    {
        const QString Href = ManifestById.value(Id);

        if (Href.isEmpty())
        {
            continue;
        }

        const QString Entry = ResolveZipPath(OpfPath, Href);

        if (Entry.startsWith(QStringLiteral("../")))
        {
            continue;
        }

        const std::optional<QByteArray> Content = ReadEntry(Entry);

        if (!Content.has_value())
        {
            continue;
        }

        FileStarts.insert(Entry, static_cast<int>(Result.Paragraphs.size()));

        const auto ResolveImage = [&ReadEntry, &Entry](const QString& Source) -> std::optional<FTParagraphImage>
        {
            const QString ImagePath = ResolveZipPath(Entry, Source.section(QLatin1Char('#'), 0, 0));
            const std::optional<QByteArray> Bytes = ReadEntry(ImagePath);

            if (!Bytes.has_value())
            {
                return std::nullopt;
            }

            const std::optional<std::pair<int, int>> Size = ReadImageSize(*Bytes);

            return FTParagraphImage{ImagePath.toStdString(), Size ? Size->first : 0, Size ? Size->second : 0};
        };

        QXmlStreamReader ContentReader(ReplaceHtmlEntities(*Content));
        CollectBlockText(ContentReader, Result, StyleSheetFor(*Content, Entry), &FileAnchors[Entry], ResolveImage, &UnhandledCss);
    }

    LogUnhandledCss(Title, UnhandledCss, StyleSheets);

    if (NcxId.isEmpty())
    {
        for (auto Item = MediaTypeById.cbegin(); Item != MediaTypeById.cend(); ++Item)
        {
            if (Item.value() == QStringLiteral("application/x-dtbncx+xml"))
            {
                NcxId = Item.key();
            }
        }
    }

    std::vector<FTEpubTocEntry> Toc;

    if (!NavHref.isEmpty())
    {
        const QString NavPath = ResolveZipPath(OpfPath, NavHref);

        if (const std::optional<QByteArray> Nav = ReadEntry(NavPath); Nav.has_value())
        {
            Toc = ReadNavToc(*Nav, NavPath);
        }
    }

    if (Toc.empty() && !NcxId.isEmpty())
    {
        const QString NcxPath = ResolveZipPath(OpfPath, ManifestById.value(NcxId));

        if (const std::optional<QByteArray> Ncx = ReadEntry(NcxPath); Ncx.has_value())
        {
            Toc = ReadNcxToc(*Ncx, NcxPath);
        }
    }

    mz_zip_reader_end(&Zip);

    std::vector<FTReaderChapter> TocChapters;
    const int LastParagraph = static_cast<int>(Result.Paragraphs.size()) - 1;

    for (const FTEpubTocEntry& Entry : Toc)
    {
        const QString File = Entry.Target.section(QLatin1Char('#'), 0, 0);
        const QString Fragment = Entry.Target.section(QLatin1Char('#'), 1);

        if (!FileStarts.contains(File) || LastParagraph < 0)
        {
            continue;
        }

        int Paragraph = FileStarts.value(File);

        if (!Fragment.isEmpty() && FileAnchors.value(File).contains(Fragment))
        {
            Paragraph = FileAnchors.value(File).value(Fragment);
        }

        Paragraph = qMin(Paragraph, LastParagraph);

        const size_t Index = static_cast<size_t>(Paragraph);

        if (Result.HeadingLevels[Index] == 0
            && QString::fromStdString(Result.Paragraphs[Index]).compare(Entry.Title, Qt::CaseInsensitive) == 0)
        {
            Result.HeadingLevels[Index] = Entry.Level;
        }

        TocChapters.push_back({Entry.Title.toStdString(), Paragraph, Entry.Level});
    }

    if (!TocChapters.empty())
    {
        Result.Chapters = std::move(TocChapters);
    }

    return Result;
}
