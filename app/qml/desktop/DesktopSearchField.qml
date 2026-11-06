import QtQuick
import QtQuick.Controls.Basic

DesktopTextField
{
    id: root

    readonly property string term: (root.text + root.preeditText).trim()

    leftPadding: AppMetrics.gapCompact + AppMetrics.iconSize + AppMetrics.gapSmall
    rightPadding: clearButton.visible ? clearButton.width + AppMetrics.gapTight : AppMetrics.gapCompact

    Keys.onEscapePressed: function(event)
    {
        if (root.text.length === 0)
        {
            event.accepted = false

            return
        }

        root.clear()
    }

    DesktopIcon
    {
        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.gapCompact
        anchors.verticalCenter: parent.verticalCenter

        name: "search"
        color: AppTheme.colourMuted
    }

    DesktopIconButton
    {
        id: clearButton

        anchors.right: parent.right
        anchors.rightMargin: AppMetrics.gapTight
        anchors.verticalCenter: parent.verticalCenter

        width: root.height - AppMetrics.gapSmall
        height: width

        visible: root.text.length > 0
        focusPolicy: Qt.NoFocus
        iconName: "close"
        iconColour: AppTheme.colourMuted
        text: qsTr("Clear search")

        onClicked:
        {
            root.clear()
            root.forceActiveFocus()
        }
    }
}
