#include "FTPdfDocumentExtractor.h"

#include <QPdfDocument>
#include <QRegularExpression>

static bool ReadsAsTitle(const QString& Title)
{
    static const QRegularExpression FileName(QStringLiteral("\\.(pdf|docx?|rtf|odt|indd|qxd|tex|dvi|p65|pmd)$"),
                                             QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression Placeholder(QStringLiteral("^(untitled|document\\d*|microsoft word\\b.*)$"),
                                                QRegularExpression::CaseInsensitiveOption);

    return !Title.isEmpty() && !FileName.match(Title).hasMatch() && !Placeholder.match(Title).hasMatch();
}

std::optional<FTReaderDocument> FTPdfDocumentExtractor::Extract(const std::string& FilePath) const
{
    QPdfDocument Document;
    Document.load(QString::fromStdString(FilePath));

    if (Document.status() != QPdfDocument::Status::Ready)
    {
        return std::nullopt;
    }

    const QString Title = Document.metaData(QPdfDocument::MetaDataField::Title).toString().simplified();

    FTReaderDocument Result;

    if (ReadsAsTitle(Title))
    {
        Result.Title = Title.toStdString();
    }

    return Result;
}
