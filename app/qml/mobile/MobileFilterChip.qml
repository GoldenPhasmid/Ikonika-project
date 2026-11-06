import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property string label
    property string count: ""
    property color dotColour: "transparent"
    property bool selected: false

    signal clicked()

    implicitWidth: chipRow.implicitWidth + AppMetrics.gapCompact * 2
    implicitHeight: AppMetrics.chipHeight

    radius: AppMetrics.radiusMedium
    color: root.selected ? AppTheme.colourText : "transparent"
    border.width: root.selected ? 0 : AppMetrics.borderWidth
    border.color: AppTheme.colourBorder

    Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

    Accessible.role: Accessible.Button
    Accessible.name: root.count.length > 0 ? qsTr("%1, %2").arg(root.label).arg(root.count) : root.label
    Accessible.checkable: true
    Accessible.checked: root.selected

    RowLayout
    {
        id: chipRow

        anchors.centerIn: parent
        spacing: AppMetrics.gapSmall

        CategoryDot
        {
            visible: root.dotColour.a > 0
            colour: root.dotColour
        }

        Text
        {
            text: root.label
            font.pixelSize: AppMetrics.fontSmall
            font.weight: Font.Medium
            font.family: AppMetrics.fontFamily
            color: root.selected ? AppTheme.colourPanel : AppTheme.colourText
        }

        Text
        {
            visible: root.count.length > 0
            text: root.count
            font.pixelSize: AppMetrics.fontSmall
            font.family: AppMetrics.fontFamily
            color: root.selected ? AppTheme.colourPanel : AppTheme.colourMuted
            opacity: root.selected ? AppTheme.opacitySecondary : 1.0
        }
    }

    TapHandler
    {
        margin: (AppMetrics.buttonHeightLarge - AppMetrics.chipHeight) / 2

        onTapped: root.clicked()
    }
}
