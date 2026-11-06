#include "AppBookImageProvider.h"

#include "core/FTLog.h"
#include "core/FTZipExtract.h"

#include <QUrl>

AppBookImageProvider::AppBookImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage AppBookImageProvider::requestImage(const QString& Id, QSize* Size, const QSize& RequestedSize)
{
    const qsizetype Separator = Id.indexOf(QLatin1Char('/'));

    if (Separator < 0)
    {
        return {};
    }

    const QString BookPath = QUrl::fromPercentEncoding(Id.left(Separator).toUtf8());
    const QString Entry = QUrl::fromPercentEncoding(Id.mid(Separator + 1).toUtf8());
    const std::optional<std::string> Bytes = ReadZipEntry(BookPath.toStdString(), Entry.toStdString());

    if (!Bytes.has_value())
    {
        qCWarning(LogApp) << "Reader: no image" << Entry << "in" << BookPath;

        return {};
    }

    QImage Image = QImage::fromData(QByteArray::fromStdString(*Bytes));

    if (Image.isNull())
    {
        qCWarning(LogApp) << "Reader: image" << Entry << "in" << BookPath << "could not be decoded";

        return {};
    }

    if (Size != nullptr)
    {
        *Size = Image.size();
    }

    if (RequestedSize.isValid() && (RequestedSize.width() < Image.width() || RequestedSize.height() < Image.height()))
    {
        Image = Image.scaled(RequestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return Image;
}
