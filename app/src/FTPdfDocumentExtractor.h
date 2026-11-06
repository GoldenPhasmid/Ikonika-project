#pragma once

#include "core/FTDocumentExtractor.h"

/**
 * Title lookup for the book reader. The reader renders the actual pages (QtQuick.Pdf) and never
 * needs the document as text, so all this does is name a PDF when it is added to the library.
 *
 * The name is the document's own Title metadata when it reads as a title. A file handed over by
 * Android often has a meaningless name, and the metadata is the only real title there is. A title
 * that is empty, a file name or a tool's placeholder is left out, and the caller keeps the file
 * name. Opening the document is still what says the file is a readable PDF at all, which is what
 * nullopt reports.
 */
class FTPdfDocumentExtractor final : public FTDocumentExtractor
{
public:
    std::optional<FTReaderDocument> Extract(const std::string& FilePath) const override;
};
