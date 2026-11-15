#pragma once

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QtGlobal>

class FAppTranslationRequests
{
public:
    quint64 Begin(quintptr Owner)
    {
        const QMutexLocker Lock(&Mutex);

        const quint64 Generation = ++LastGeneration;

        Latest.insert(Owner, Generation);
        Owners.insert(Generation, Owner);

        return Generation;
    }

    bool IsLatest(quint64 Generation) const
    {
        const QMutexLocker Lock(&Mutex);

        const auto Found = Owners.constFind(Generation);

        return Found != Owners.constEnd() && Latest.value(Found.value()) == Generation;
    }

    bool Finish(quint64 Generation)
    {
        const QMutexLocker Lock(&Mutex);

        const auto Found = Owners.constFind(Generation);

        if (Found == Owners.constEnd())
        {
            return false;
        }

        const quintptr Owner = Found.value();

        Owners.erase(Found);

        if (Latest.value(Owner) != Generation)
        {
            return false;
        }

        Latest.remove(Owner);

        return true;
    }

private:
    mutable QMutex Mutex;

    quint64 LastGeneration = 0;

    QHash<quintptr, quint64> Latest;

    QHash<quint64, quintptr> Owners;
};
