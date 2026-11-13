import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property string name: ""
    property bool today: false
    property bool past: false
    property real review: 0
    property real learn: 0

    spacing: AppMetrics.todayNameGap

    DesktopLabel
    {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredHeight: AppMetrics.todayLabelHeight
        verticalAlignment: Text.AlignVCenter
        text: root.name
        font.pixelSize: AppMetrics.fontMeta
        font.weight: root.today ? Font.Bold : Font.Medium
        color: root.today ? AppTheme.colourText : AppTheme.colourMuted
    }

    Item
    {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: AppMetrics.weekRingSize
        Layout.preferredHeight: AppMetrics.weekRingSize

        Rectangle
        {
            anchors.centerIn: parent
            visible: root.today
            width: AppMetrics.weekDayCellSize
            height: width
            radius: width / 2
            color: AppTheme.colourFaint
            border.width: AppMetrics.borderWidth
            border.color: AppTheme.colourBorder
        }

        DesktopDoubleRing
        {
            anchors.fill: parent
            visible: root.today || root.past

            stroke: AppMetrics.weekRingStroke
            gap: AppMetrics.weekRingGap
            outerProgress: root.review
            innerProgress: root.learn
        }

        DesktopDashedRing
        {
            anchors.fill: parent
            visible: !root.today && !root.past
        }
    }
}
