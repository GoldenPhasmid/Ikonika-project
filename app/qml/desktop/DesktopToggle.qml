import QtQuick
import QtQuick.Controls.Basic

AbstractButton
{
    id: root

    property bool on: false
    property string accessibleName: ""
    property bool onCover: false
    property color coverTint: AppTheme.colourCoverMuted

    readonly property color ringColour: root.onCover ? AppTheme.colourCoverInk : AppTheme.colourAccent

    implicitWidth: row.implicitWidth
    implicitHeight: AppMetrics.buttonHeight

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: root.enabled ? 1.0 : AppTheme.opacityDisabled

    Accessible.role: Accessible.CheckBox
    Accessible.name: root.accessibleName.length > 0 ? root.accessibleName : root.text
    Accessible.checkable: true
    Accessible.checked: root.on

    contentItem: Item
    {
        implicitWidth: row.implicitWidth

        Row
        {
            id: row

            anchors.centerIn: parent
            spacing: AppMetrics.gapSmall

            DesktopLabel
            {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.text.length > 0
                text: root.text
                font.weight: Font.Medium
                color: root.onCover ? AppTheme.colourCoverInk : AppTheme.colourText
            }

            DesktopSwitch
            {
                id: track

                anchors.verticalCenter: parent.verticalCenter
                on: root.on
                onCover: root.onCover
                coverTint: root.coverTint
                opacity: root.hovered ? AppTheme.opacityHover : 1.0
            }
        }
    }

    background: Item
    {
        DesktopFocusRing
        {
            anchors.fill: undefined
            anchors.right: parent.right
            anchors.rightMargin: (parent.width - row.width) / 2 - AppMetrics.focusRingWidth * 2
            anchors.verticalCenter: parent.verticalCenter
            width: track.width + AppMetrics.focusRingWidth * 4
            height: track.height + AppMetrics.focusRingWidth * 4
            visible: root.visualFocus
            colour: root.ringColour
            baseRadius: track.height / 2
        }
    }

    HoverHandler
    {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
