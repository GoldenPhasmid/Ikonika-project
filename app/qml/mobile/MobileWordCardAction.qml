import QtQuick

Rectangle
{
    id: root

    property string iconSvg: ""
    property real iconMinX: 0
    property real iconMinY: 0
    property real iconWidth: 24
    property real iconHeight: 24

    readonly property real iconBoxSize: 22

    property bool holdToConfirm: false
    property int holdDuration: 600
    property color tint: AppTheme.colourFaint
    property color iconColor: AppTheme.colourText

    signal triggered()

    property real fillProgress: 0

    color: root.tint

    Rectangle
    {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * root.fillProgress
        color: Qt.rgba(1, 1, 1, 0.28)
        visible: root.holdToConfirm
    }

    MobileVectorIcon
    {
        anchors.centerIn: parent

        visible: root.iconSvg.length > 0
        width: root.iconBoxSize
        height: root.iconBoxSize

        minX: root.iconMinX
        minY: root.iconMinY
        sourceWidth: root.iconWidth
        sourceHeight: root.iconHeight
        svgPath: root.iconSvg
        color: root.iconColor
    }

    NumberAnimation
    {
        id: fillUp
        target: root
        property: "fillProgress"
        to: 1.0
        duration: root.holdDuration

        onFinished: { root.fillProgress = 0; root.triggered() }
    }

    NumberAnimation
    {
        id: fillDown
        target: root
        property: "fillProgress"
        to: 0.0
        duration: AppMetrics.motionFast
    }

    TapHandler
    {
        enabled: !root.holdToConfirm
        onTapped: root.triggered()
    }

    TapHandler
    {
        enabled: root.holdToConfirm

        onPressedChanged:
        {
            if (pressed)
            {
                fillDown.stop()
                fillUp.start()
            }
            else
            {
                fillUp.stop()
                fillDown.start()
            }
        }
    }
}
