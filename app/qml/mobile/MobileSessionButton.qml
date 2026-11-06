import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property string label

    property string leadingArrow: ""
    property string trailingArrow: ""
    property bool filled: true
    property bool interactive: true
    property bool destructive: false

    signal clicked()

    Layout.fillWidth: true
    Layout.preferredHeight: AppMetrics.buttonHeightLarge

    radius: AppMetrics.radiusSmall
    color: root.filled ? (root.destructive ? AppTheme.colourWrong : AppTheme.colourText) : "transparent"
    border.color: root.destructive ? AppTheme.colourWrong : AppTheme.colourBorder
    border.width: root.filled ? 0 : AppMetrics.borderWidth
    opacity: root.interactive ? 1.0 : 0.5

    readonly property color contentColour: root.filled
        ? AppTheme.colourPanel
        : (root.destructive ? AppTheme.colourWrong : AppTheme.colourText)

    RowLayout
    {
        anchors.centerIn: parent
        spacing: AppMetrics.gapSmall

        MobileArrowIcon
        {
            visible: root.leadingArrow.length > 0
            direction: root.leadingArrow
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            color: root.contentColour
        }

        Text
        {
            text: root.label
            font.pixelSize: AppMetrics.fontBody
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            color: root.contentColour
        }

        MobileArrowIcon
        {
            visible: root.trailingArrow.length > 0
            direction: root.trailingArrow
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            color: root.contentColour
        }
    }

    MouseArea
    {
        anchors.fill: parent
        enabled: root.interactive
        onClicked: root.clicked()
    }
}
