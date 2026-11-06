import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property string letter: ""
    property bool today: false
    property bool past: false
    property real review: 0
    property real learn: 0

    spacing: AppMetrics.gapTight

    DesktopLabel
    {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: root.letter
        font.pixelSize: AppMetrics.fontMeta
        font.weight: root.today ? Font.Bold : Font.Medium
        color: root.today ? AppTheme.colourText : AppTheme.colourMuted
    }

    Rectangle
    {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: AppMetrics.weekDayCellSize
        Layout.preferredHeight: AppMetrics.weekDayCellSize
        radius: width / 2
        color: root.today ? AppTheme.colourFaint : "transparent"

        DesktopDoubleRing
        {
            anchors.centerIn: parent
            width: AppMetrics.weekRingSize
            height: width
            visible: root.today || root.past

            stroke: AppMetrics.weekRingStroke
            gap: AppMetrics.weekRingGap
            outerProgress: root.review
            innerProgress: root.learn
        }

        Rectangle
        {
            anchors.centerIn: parent
            visible: !root.today && !root.past
            width: AppMetrics.weekFutureDotSize
            height: width
            radius: width / 2
            color: AppTheme.colourBorder
        }
    }
}
