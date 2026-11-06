import QtQuick
import QtQuick.Layouts

DesktopDoubleRing
{
    id: root

    property int doneToday: 0
    property int dailyGoal: 0

    DesktopLabel
    {
        Layout.alignment: Qt.AlignHCenter
        visible: root.dailyGoal > 0
        text: root.doneToday
        font.pixelSize: AppMetrics.heroFigureSize
        font.weight: Font.Bold
    }

    DesktopLabel
    {
        Layout.alignment: Qt.AlignHCenter
        visible: root.dailyGoal > 0
        text: qsTr("of %1").arg(root.dailyGoal)
        font.pixelSize: AppMetrics.fontSmall
        color: AppTheme.colourMuted
    }
}
