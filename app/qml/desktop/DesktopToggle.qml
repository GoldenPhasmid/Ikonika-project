import QtQuick
import QtQuick.Controls.Basic

AbstractButton
{
    id: root

    property bool on: false
    property string accessibleName: ""

    implicitWidth: track.implicitWidth
    implicitHeight: AppMetrics.buttonHeight

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: root.enabled ? 1.0 : AppTheme.opacityDisabled

    Accessible.role: Accessible.CheckBox
    Accessible.name: root.accessibleName
    Accessible.checkable: true
    Accessible.checked: root.on

    contentItem: Item
    {
        DesktopSwitch
        {
            id: track

            anchors.centerIn: parent
            on: root.on
            opacity: root.hovered ? AppTheme.opacityHover : 1.0
        }
    }

    background: Item
    {
        DesktopFocusRing
        {
            anchors.fill: undefined
            anchors.centerIn: parent
            width: track.width
            height: track.height
            visible: root.visualFocus
            baseRadius: track.height / 2
        }
    }

    HoverHandler
    {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
