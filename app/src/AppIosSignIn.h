#pragma once

#include <QString>
#include <QUrl>

#include <functional>

class AppIosSignIn
{
public:
    using FinishedCallback = std::function<void(const QUrl& Callback, const QString& Error, bool bCancelled)>;

    static QString ReadClientId();
    static bool Start(const QUrl& AuthorizationUrl, const QString& CallbackScheme, FinishedCallback Done);
};
