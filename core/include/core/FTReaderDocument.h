#pragma once

#include <string>
#include <vector>

/** The document format, detected from the file extension. Unknown means the app cannot read it. */
enum class EFTDocumentFormat
{
    Unknown,
    Txt,
    Fb2,
    Epub,
    Pdf
};

struct FTReaderChapter
{
    std::string Title;
    int Paragraph = 0;
    int Level = 1;
};

struct FTParagraphLayout
{
    float FontScale = 1.0f;
    float TextIndent = 0.0f;
    float TextIndentWidth = 0.0f;
    float MarginTop = 0.0f;
    float MarginTopWidth = 0.0f;
    float MarginBottom = 0.0f;
    float MarginBottomWidth = 0.0f;
    float MarginLeft = 0.0f;
    float MarginLeftWidth = 0.0f;
    float MarginRight = 0.0f;
    float MarginRightWidth = 0.0f;
    std::string TextAlign;
    std::string TextTransform;
    bool bBold = false;
    bool bItalic = false;
    bool bSmallCaps = false;
    bool bRule = false;
};

struct FTParagraphImage
{
    std::string Path;
    int Width = 0;
    int Height = 0;
};

/** What a format's text extraction produced: a title and the paragraphs in reading order. */
struct FTReaderDocument
{
    std::string Title;
    std::vector<std::string> Paragraphs;
    std::vector<int> HeadingLevels;
    std::vector<std::string> Markup;
    std::vector<FTParagraphLayout> Layouts;
    std::vector<FTParagraphImage> Images;
    std::vector<FTReaderChapter> Chapters;
};

/** Maps an extension to a format. "fb2.zip" reads as FB2; a zip that is not a FictionBook is not
    supported. */
EFTDocumentFormat DetectDocumentFormat(const std::string& FilePath);

/** A short human label for the format, shown in the library list. */
const char* DocumentFormatName(EFTDocumentFormat Format);

EFTDocumentFormat ParseDocumentFormat(const std::string& Name);
