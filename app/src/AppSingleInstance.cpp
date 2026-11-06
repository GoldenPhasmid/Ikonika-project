#include "AppSingleInstance.h"

#include "core/FTLog.h"

#include <QLocalServer>
#include <QLocalSocket>

namespace
{
    /** Debug and release each get their own name, so a developer's build never fights the
        installed one for the same named pipe. */
    QString ServerName()
    {
#ifdef QT_DEBUG
        return QStringLiteral("IkonikaSingleInstanceDebug");
#else
        return QStringLiteral("IkonikaSingleInstance");
#endif
    }
}

AppSingleInstance::AppSingleInstance(QObject* Parent)
    : QObject(Parent)
{
}

AppSingleInstance::~AppSingleInstance()
{
    if (Server != nullptr)
    {
        Server->close();
    }
}

bool AppSingleInstance::TryAcquire()
{
    QLocalSocket Probe;
    Probe.connectToServer(ServerName());

    if (Probe.waitForConnected(200))
    {
        Probe.write("show");
        Probe.waitForBytesWritten(200);
        Probe.disconnectFromServer();

        qCInfo(LogApp) << "another instance is already running, asked it to show its window";

        return false;
    }

    // A stale socket file can outlive a crashed process on some platforms. Windows named pipes do
    // not leave one behind, so this is a no-op there; harmless either way.
    QLocalServer::removeServer(ServerName());

    Server = new QLocalServer(this);
    QObject::connect(Server, &QLocalServer::newConnection, this,
                      &AppSingleInstance::OnNewConnection);

    if (!Server->listen(ServerName()))
    {
        qCWarning(LogApp) << "failed to claim the single-instance server, a second launch will "
                              "open its own window instead of activating this one:"
                           << Server->errorString();
    }

    return true;
}

void AppSingleInstance::OnNewConnection()
{
    while (QLocalSocket* Socket = Server->nextPendingConnection())
    {
        Socket->waitForReadyRead(200);
        Socket->deleteLater();
    }

    emit activationRequested();
}
