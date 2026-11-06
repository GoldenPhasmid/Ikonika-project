#include "core/FTZipExtract.h"

#include "core/FTLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

#include "miniz.h"

#include <cstring>

namespace
{
    bool IsContainedRelativePath(const QString& Name)
    {
        if (Name.isEmpty())
        {
            return false;
        }

        if (QDir::isAbsolutePath(Name) || Name.contains(QLatin1Char(':')))
        {
            return false;
        }

        const QStringList Parts = Name.split(QLatin1Char('/'), Qt::SkipEmptyParts);

        for (const QString& Part : Parts)
        {
            if (Part == QLatin1String(".."))
            {
                return false;
            }
        }

        return !Parts.isEmpty();
    }

    size_t WriteExtractedBytes(void* Opaque, mz_uint64 FileOffset, const void* Buffer, size_t Size)
    {
        Q_UNUSED(FileOffset);

        QFile* Target = static_cast<QFile*>(Opaque);

        return Target->write(static_cast<const char*>(Buffer), static_cast<qint64>(Size)) ==
                       static_cast<qint64>(Size)
                   ? Size
                   : 0;
    }

    size_t ReadArchiveBytes(void* Opaque, mz_uint64 FileOffset, void* Buffer, size_t Size)
    {
        QFile* Source = static_cast<QFile*>(Opaque);

        if (!Source->seek(static_cast<qint64>(FileOffset)))
        {
            return 0;
        }

        const qint64 Read = Source->read(static_cast<char*>(Buffer), static_cast<qint64>(Size));

        return Read < 0 ? 0 : static_cast<size_t>(Read);
    }
}

bool ExtractZipArchive(const std::string& ArchivePath, const std::string& DestinationDirectory)
{
    const QString Archive = QString::fromStdString(ArchivePath);
    const QDir Destination(QString::fromStdString(DestinationDirectory));

    if (!QDir().mkpath(Destination.absolutePath()))
    {
        qCWarning(LogUpdate) << "Cannot create" << Destination.absolutePath();

        return false;
    }

    mz_zip_archive Zip;
    std::memset(&Zip, 0, sizeof(Zip));

    if (!mz_zip_reader_init_file(&Zip, Archive.toUtf8().constData(), 0))
    {
        qCWarning(LogUpdate) << "Not a readable archive:" << Archive;

        return false;
    }

    const mz_uint EntryCount = mz_zip_reader_get_num_files(&Zip);
    bool bSucceeded = true;

    for (mz_uint Index = 0; Index < EntryCount && bSucceeded; ++Index)
    {
        mz_zip_archive_file_stat Entry;
        std::memset(&Entry, 0, sizeof(Entry));

        if (!mz_zip_reader_file_stat(&Zip, Index, &Entry))
        {
            qCWarning(LogUpdate) << "Cannot read entry" << Index << "of" << Archive;
            bSucceeded = false;

            break;
        }

        QString Name = QString::fromUtf8(Entry.m_filename);
        Name.replace(QLatin1Char('\\'), QLatin1Char('/'));

        if (Name.endsWith(QLatin1Char('/')))
        {
            continue;
        }

        if (!IsContainedRelativePath(Name))
        {
            qCWarning(LogUpdate) << "Refusing an archive entry that points outside the "
                                    "destination:" << Name;
            bSucceeded = false;

            break;
        }

        const QString TargetPath = Destination.filePath(Name);

        if (!QDir().mkpath(QFileInfo(TargetPath).absolutePath()))
        {
            qCWarning(LogUpdate) << "Cannot create the directory for" << TargetPath;
            bSucceeded = false;

            break;
        }

        QFile Target(TargetPath);

        if (!Target.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            qCWarning(LogUpdate) << "Cannot write" << TargetPath << Target.errorString();
            bSucceeded = false;

            break;
        }

        if (!mz_zip_reader_extract_to_callback(&Zip, Index, WriteExtractedBytes, &Target, 0))
        {
            qCWarning(LogUpdate) << "Failed to unpack" << Name << "from" << Archive;
            bSucceeded = false;
        }

        Target.close();
    }

    mz_zip_reader_end(&Zip);

    return bSucceeded;
}

std::optional<std::string> ReadZipEntry(const std::string& ArchivePath, const std::string& EntryName)
{
    QFile Source(QString::fromStdString(ArchivePath));

    if (!Source.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }

    mz_zip_archive Zip;
    std::memset(&Zip, 0, sizeof(Zip));
    Zip.m_pRead = ReadArchiveBytes;
    Zip.m_pIO_opaque = &Source;

    if (!mz_zip_reader_init(&Zip, static_cast<mz_uint64>(Source.size()), 0))
    {
        return std::nullopt;
    }

    size_t Size = 0;
    void* Data = mz_zip_reader_extract_file_to_heap(&Zip, EntryName.c_str(), &Size, 0);

    mz_zip_reader_end(&Zip);

    if (Data == nullptr)
    {
        return std::nullopt;
    }

    std::string Result(static_cast<const char*>(Data), Size);
    MZ_FREE(Data);

    return Result;
}
