#include "AppSingleInstance.h"

#include "core/FTLog.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>

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

bool AppSingleInstance::TryAcquire(const QString& BookPath)
{
    QLocalSocket Probe;
    Probe.connectToServer(ServerName());

    if (Probe.waitForConnected(200))
    {
        Probe.write(QJsonDocument(QJsonObject{{QStringLiteral("path"), BookPath}}).toJson(QJsonDocument::Compact) + '\n');
        Probe.waitForBytesWritten(200);
        Probe.waitForReadyRead(200);
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
        auto Payload = std::make_shared<QByteArray>();
        const auto Receive = [this, Socket, Payload]()
        {
            Payload->append(Socket->readAll());
            if (!Payload->contains('\n') && Socket->state() != QLocalSocket::UnconnectedState)
            {
                return;
            }

            const QString Path{QJsonDocument::fromJson(*Payload).object().value(QStringLiteral("path")).toString()};
            if (!Path.isEmpty())
            {
                emit bookRequested(Path);
            }
            emit activationRequested();
            Socket->write("ok");
            Socket->flush();
            Socket->disconnect(this);
            Socket->deleteLater();
        };
        connect(Socket, &QLocalSocket::readyRead, this, Receive);
        connect(Socket, &QLocalSocket::disconnected, this, Receive);
        Receive();
    }
}
