#include "AppHeartbeat.h"

#include <QCoreApplication>

namespace
{
    bool bHeartbeatActive = false;
}

AppHeartbeat::AppHeartbeat(QObject* Parent)
    : QObject(Parent)
{
}

void AppHeartbeat::SetActive(bool bValue)
{
    bHeartbeatActive = bValue;
}

void AppHeartbeat::Beat()
{
    if (!bHeartbeatActive)
    {
        return;
    }

    QCoreApplication::processEvents();
}

void AppHeartbeat::classBegin()
{
}

void AppHeartbeat::componentComplete()
{
    Beat();
}
