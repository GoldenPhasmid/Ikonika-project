#include "AppImport.h"

#include "core/FTAnkiImporter.h"
#include "core/FTCsvImporter.h"
#include "core/FTLog.h"

#include <QFileInfo>

std::unique_ptr<FTImportSource> AppImport::Parse(const QString& Path)
{
    if (Path.isEmpty())
    {
        return nullptr;
    }

    std::unique_ptr<FTImportSource> Source;

    if (QFileInfo(Path).suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) == 0)
    {
        Source = std::make_unique<FTCsvImporter>();
    }
    else
    {
        Source = std::make_unique<FTAnkiImporter>();
    }

    if (!Source->Parse(Path.toStdString()))
    {
        qCWarning(LogApp) << "Import failed for" << Path << ":" << QString::fromStdString(Source->GetLastError());

        return nullptr;
    }

    return Source;
}
