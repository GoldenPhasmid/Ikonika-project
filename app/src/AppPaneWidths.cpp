#include "AppPaneWidths.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QSettings>

namespace
{
    constexpr int MinimumPaneWidth = 200;
    constexpr int MaximumPaneWidth = 640;

    QString MakeKey(const QString& Key)
    {
        return QStringLiteral("panes/") + Key;
    }
}

AppPaneWidths& AppPaneWidths::Get()
{
    static AppPaneWidths Instance;

    return Instance;
}

int AppPaneWidths::width(const QString& Key, int DefaultWidth) const
{
    const QSettings Settings(FTSettings::Get().OpenStore());

    return qBound(MinimumPaneWidth, Settings.value(MakeKey(Key), DefaultWidth).toInt(), MaximumPaneWidth);
}

void AppPaneWidths::setWidth(const QString& Key, int Width)
{
    const int Bounded{qBound(MinimumPaneWidth, Width, MaximumPaneWidth)};

    QSettings Settings(FTSettings::Get().OpenStore());

    if (Settings.value(MakeKey(Key)).toInt() == Bounded)
    {
        return;
    }

    Settings.setValue(MakeKey(Key), Bounded);

    qCInfo(LogApp) << "Pane" << Key << "resized to" << Bounded;

    ++Revision;

    emit changed();
}

int AppPaneWidths::GetRevision() const
{
    return Revision;
}

int AppPaneWidths::GetMinimumWidth() const
{
    return MinimumPaneWidth;
}

int AppPaneWidths::GetMaximumWidth() const
{
    return MaximumPaneWidth;
}
