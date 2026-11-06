#pragma once

#include <QQuickImageProvider>

class AppBookImageProvider : public QQuickImageProvider
{
public:
    AppBookImageProvider();

    QImage requestImage(const QString& Id, QSize* Size, const QSize& RequestedSize) override;
};
