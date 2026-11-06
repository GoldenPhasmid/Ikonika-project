import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property string text: ""
    property bool selected: false
    property int stage: -1
    property bool completed: false

    signal clicked()

    implicitWidth: contentRow.implicitWidth
    implicitHeight: AppMetrics.tabHeight

    Accessible.role: Accessible.PageTab
    Accessible.name: root.text
    Accessible.selected: root.selected
    Accessible.onPressAction: root.clicked()

    RowLayout
    {
        id: contentRow

        anchors.centerIn: parent
        spacing: AppMetrics.gapSmall

        Text
        {
            text: root.text
            font.pixelSize: AppMetrics.fontTab
            font.weight: root.selected ? Font.DemiBold : Font.Medium
            font.family: AppMetrics.fontFamily
            color: root.selected ? AppTheme.colourText : AppTheme.colourMuted
        }

        LearnedMark
        {
            stage: root.stage
            completed: root.completed
        }
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: AppMetrics.tabIndicatorHeight
        radius: height / 2
        color: root.selected ? AppTheme.colourAccent : "transparent"
    }

    MouseArea
    {
        anchors.fill: parent

        onClicked: root.clicked()
    }
}
