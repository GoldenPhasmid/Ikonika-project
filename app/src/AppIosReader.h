#pragma once

#include <QString>
#include <QUrl>

#include <functional>

class AppIosReader
{
public:
    static void PickDocument(std::function<void(const QString&)> OnPicked);
    static void SetKeepScreenOn(bool bEnabled);

    static QString KeepBook(const QString& SourcePath, bool bMove);
    static QString ImportOpenedUrl(const QUrl& Url);
};
