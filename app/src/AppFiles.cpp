#include "AppFiles.h"

#include <QFile>

std::optional<QByteArray> ReadWholeFile(const QString& Path)
{
    QFile File(Path);

    if (!File.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }

    QByteArray Contents{File.readAll()};

    if (File.error() != QFileDevice::NoError)
    {
        return std::nullopt;
    }

    return Contents;
}

void RemoveDatabaseFiles(const QString& Path)
{
    QFile::remove(Path);
    QFile::remove(Path + QStringLiteral("-wal"));
    QFile::remove(Path + QStringLiteral("-shm"));
}
