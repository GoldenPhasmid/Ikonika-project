#pragma once

#include "core/FTReaderDocument.h"

#include <optional>
#include <string>

/**
 * Turns a document file into the text a reader renders: a title and an ordered list of
 * paragraphs. One extractor per format, chosen by DetectDocumentFormat.
 *
 * The result is flowable text only. Layout, images, tables and metadata are not carried over;
 * the reader is for reading, not for reproducing the original page.
 */
class FTDocumentExtractor
{
public:
    virtual ~FTDocumentExtractor() = default;

    /** Extracts the document. Nothing when the file cannot be read or is not that format. */
    virtual std::optional<FTReaderDocument> Extract(const std::string& FilePath) const = 0;
};

/** A plain text file. Paragraphs are blocks separated by blank lines; the title is the file name. */
class FTTxtDocumentExtractor final : public FTDocumentExtractor
{
public:
    std::optional<FTReaderDocument> Extract(const std::string& FilePath) const override;
};

/** FictionBook 2, an XML format. Paragraphs are <p> elements inside the body. */
class FTFb2DocumentExtractor final : public FTDocumentExtractor
{
public:
    std::optional<FTReaderDocument> Extract(const std::string& FilePath) const override;
};

/** EPUB 3/2, a zip of XHTML content documents walked in spine order. */
class FTEpubDocumentExtractor final : public FTDocumentExtractor
{
public:
    std::optional<FTReaderDocument> Extract(const std::string& FilePath) const override;
};
