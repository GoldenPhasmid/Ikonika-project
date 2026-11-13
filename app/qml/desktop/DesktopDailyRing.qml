import QtQuick
import QtQuick.Layouts

DesktopDoubleRing
{
    id: root

    property int doneToday: 0
    property int dailyGoal: 0

    implicitWidth: AppMetrics.todayRingSize
    implicitHeight: AppMetrics.todayRingSize
    stroke: AppMetrics.todayRingStroke
    gap: AppMetrics.todayRingGap

    DesktopLabel
    {
        Layout.alignment: Qt.AlignHCenter
        visible: root.dailyGoal > 0
        text: root.doneToday
        font.pixelSize: AppMetrics.todayRingFigure
        font.weight: Font.Bold
    }

    DesktopLabel
    {
        Layout.alignment: Qt.AlignHCenter
        visible: root.dailyGoal > 0
        text: qsTr("of %1").arg(root.dailyGoal)
        font.pixelSize: AppMetrics.fontSmall
        font.weight: Font.Medium
        color: AppTheme.colourMuted
    }
}
