import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property string accessibleName: ""
    property int accessibleRole: Accessible.PageTabList
    property bool rightSide: false
    property real panelWidth: Math.min(root.width - AppMetrics.buttonHeightLarge * 2, AppMetrics.categoryPaneWidth * 1.4)
    property real reveal: 0

    default property alias content: column.data

    function open()
    {
        root.visible = true
        hideAnimation.stop()
        showAnimation.restart()
    }

    function close()
    {
        showAnimation.stop()
        hideAnimation.restart()
    }

    visible: false

    NumberAnimation
    {
        id: showAnimation

        target: root
        property: "reveal"
        to: 1
        duration: AppMetrics.motionSlow
        easing.type: AppMetrics.motionEasing
    }

    NumberAnimation
    {
        id: hideAnimation

        target: root
        property: "reveal"
        to: 0
        duration: AppMetrics.motionBase
        easing.type: AppMetrics.motionEasing

        onFinished: root.visible = false
    }

    Rectangle
    {
        anchors.fill: parent
        color: AppTheme.colourScrim
        opacity: root.reveal

        MouseArea
        {
            anchors.fill: parent

            onClicked: root.close()
        }
    }

    Rectangle
    {
        id: panel

        x: root.rightSide ? root.width - width * root.reveal : -width * (1 - root.reveal)
        width: Math.max(0, root.panelWidth)
        height: root.height
        color: AppTheme.colourPanel
        topLeftRadius: root.rightSide ? AppMetrics.radiusLarge : 0
        bottomLeftRadius: topLeftRadius
        topRightRadius: root.rightSide ? 0 : AppMetrics.radiusLarge
        bottomRightRadius: topRightRadius

        Accessible.role: root.accessibleRole
        Accessible.name: root.accessibleName

        MouseArea
        {
            anchors.fill: parent
        }

        ColumnLayout
        {
            id: column

            anchors.fill: parent
            anchors.topMargin: root.SafeArea.margins.top + AppMetrics.gapMedium
            anchors.bottomMargin: root.SafeArea.margins.bottom + AppMetrics.contentPadding
            anchors.leftMargin: AppMetrics.gapSmall
            anchors.rightMargin: AppMetrics.gapSmall
            spacing: 0
        }
    }
}
