#include "core/FTReaderDocument.h"

#include <cctype>
#include <string>

namespace
{
    /** Lowercases an extension without its dot, e.g. "FILE.PDF" -> "pdf". */
    std::string ExtensionOf(const std::string& FilePath)
    {
        const std::string::size_type Dot = FilePath.find_last_of('.');

        if (Dot == std::string::npos || Dot + 1 >= FilePath.size())
        {
            return {};
        }

        std::string Result = FilePath.substr(Dot + 1);

        for (char& Character : Result)
        {
            Character = static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
        }

        return Result;
    }
}

EFTDocumentFormat DetectDocumentFormat(const std::string& FilePath)
{
    // The last dot decides the format, except for ".fb2.zip", where a FictionBook is the payload
    // of a zip that keeps its name through packing. Checked before the generic extension so the
    // more specific reading wins.
    const std::string LowerPath = [&FilePath]()
    {
        std::string Result = FilePath;

        for (char& Character : Result)
        {
            Character = static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
        }

        return Result;
    }();

    if (LowerPath.size() >= 8 && LowerPath.compare(LowerPath.size() - 8, 8, ".fb2.zip") == 0)
    {
        return EFTDocumentFormat::Fb2;
    }

    const std::string Extension = ExtensionOf(FilePath);

    if (Extension == "txt")
    {
        return EFTDocumentFormat::Txt;
    }

    if (Extension == "fb2")
    {
        return EFTDocumentFormat::Fb2;
    }

    if (Extension == "epub")
    {
        return EFTDocumentFormat::Epub;
    }

    if (Extension == "pdf")
    {
        return EFTDocumentFormat::Pdf;
    }

    return EFTDocumentFormat::Unknown;
}

const char* DocumentFormatName(EFTDocumentFormat Format)
{
    switch (Format)
    {
    case EFTDocumentFormat::Txt:
        return "TXT";
    case EFTDocumentFormat::Fb2:
        return "FB2";
    case EFTDocumentFormat::Epub:
        return "EPUB";
    case EFTDocumentFormat::Pdf:
        return "PDF";
    case EFTDocumentFormat::Unknown:
        return "";
    }

    return "";
}

EFTDocumentFormat ParseDocumentFormat(const std::string& Name)
{
    if (Name == "TXT")
    {
        return EFTDocumentFormat::Txt;
    }

    if (Name == "FB2")
    {
        return EFTDocumentFormat::Fb2;
    }

    if (Name == "EPUB")
    {
        return EFTDocumentFormat::Epub;
    }

    if (Name == "PDF")
    {
        return EFTDocumentFormat::Pdf;
    }

    return EFTDocumentFormat::Unknown;
}
