#include "AppViewLayouts.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QSettings>

namespace
{
    QString MakeKey(const QString& Key)
    {
        return QStringLiteral("views/") + Key + QStringLiteral("List");
    }

    const QString NavigationOrderKey = QStringLiteral("views/navigationOrder");

    const QStringList DefaultNavigationOrder{
        QStringLiteral("today"),
        QStringLiteral("words"),
        QStringLiteral("reader"),
        QStringLiteral("decks"),
        QStringLiteral("vocabulary")
    };

    QStringList CompleteNavigationOrder(const QStringList& Order)
    {
        QStringList Complete;

        for (const QString& Key : Order)
        {
            if (DefaultNavigationOrder.contains(Key) && !Complete.contains(Key))
            {
                Complete.append(Key);
            }
        }

        for (const QString& Key : DefaultNavigationOrder)
        {
            if (!Complete.contains(Key))
            {
                Complete.append(Key);
            }
        }

        return Complete;
    }
}

AppViewLayouts& AppViewLayouts::Get()
{
    static AppViewLayouts Instance;

    return Instance;
}

bool AppViewLayouts::isList(const QString& Key) const
{
    const QSettings Settings(FTSettings::Get().OpenStore());

    return Settings.value(MakeKey(Key), false).toBool();
}

void AppViewLayouts::setList(const QString& Key, bool bList)
{
    QSettings Settings(FTSettings::Get().OpenStore());

    if (Settings.value(MakeKey(Key), false).toBool() == bList)
    {
        return;
    }

    Settings.setValue(MakeKey(Key), bList);

    qCInfo(LogApp) << "View" << Key << "shows" << (bList ? "a list" : "tiles");

    ++Revision;

    emit changed();
}

QStringList AppViewLayouts::navigationOrder() const
{
    const QSettings Settings(FTSettings::Get().OpenStore());

    return CompleteNavigationOrder(Settings.value(NavigationOrderKey).toStringList());
}

void AppViewLayouts::setNavigationOrder(const QStringList& Order)
{
    const QStringList Complete = CompleteNavigationOrder(Order);

    if (Complete == navigationOrder())
    {
        return;
    }

    QSettings Settings(FTSettings::Get().OpenStore());

    Settings.setValue(NavigationOrderKey, Complete);

    qCInfo(LogApp) << "Navigation order is" << Complete.join(QStringLiteral(", "));

    ++Revision;

    emit changed();
}

int AppViewLayouts::GetRevision() const
{
    return Revision;
}
