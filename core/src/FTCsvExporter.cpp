#include "core/FTCsvExporter.h"

#include <QFile>
#include <QString>

#include <utility>

namespace
{
    /** Quotes a field when a comma, quote or newline would otherwise break the CSV structure,
        doubling embedded quotes the way the RFC asks. */
    QString EscapeCsvField(const QString& Field)
    {
        if (!Field.contains(QLatin1Char(',')) && !Field.contains(QLatin1Char('"'))
            && !Field.contains(QLatin1Char('\n')))
        {
            return Field;
        }

        QString Escaped = Field;
        Escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));

        return QStringLiteral("\"") + Escaped + QStringLiteral("\"");
    }
}

bool FTCsvExporter::Write(const std::string& FilePath, const std::vector<FTImportRow>& Rows)
{
    QFile File(QString::fromStdString(FilePath));

    if (!File.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        LastError = File.errorString().toStdString();
        return false;
    }

    QString Content = QStringLiteral("Word,Translation\n");

    for (const FTImportRow& Row : Rows)
    {
        Content += EscapeCsvField(QString::fromStdString(Row.TargetWord)) + QLatin1Char(',')
                   + EscapeCsvField(QString::fromStdString(Row.Translation)) + QLatin1Char('\n');
    }

    if (File.write(Content.toUtf8()) != Content.toUtf8().size())
    {
        LastError = "could not write the file";
        return false;
    }

    return true;
}
