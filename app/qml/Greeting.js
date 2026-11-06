.pragma library

function forHour(hour, name)
{
    if (hour >= 5 && hour < 12)
    {
        return name.length > 0 ? qsTr("Good morning, %1").arg(name) : qsTr("Good morning")
    }

    if (hour >= 12 && hour < 18)
    {
        return name.length > 0 ? qsTr("Good afternoon, %1").arg(name) : qsTr("Good afternoon")
    }

    return name.length > 0 ? qsTr("Good evening, %1").arg(name) : qsTr("Good evening")
}
