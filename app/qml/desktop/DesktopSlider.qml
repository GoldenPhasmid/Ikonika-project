import QtQuick
import QtQuick.Controls.Basic

Slider
{
    id: root

    property int step: 1
    property int current: 0
    property string accessibleTitle: ""

    signal edited(int newValue)

    implicitHeight: AppMetrics.chipHeight

    value: root.current
    stepSize: root.step
    snapMode: Slider.SnapAlways
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    padding: 0

    Accessible.name: root.accessibleTitle

    onMoved: if (Math.round(root.value) !== root.current) root.edited(Math.round(root.value))

    background: Item
    {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: AppMetrics.progressBarHeight

        Rectangle
        {
            anchors.fill: parent
            radius: height / 2
            color: AppTheme.colourFaint
        }

        Rectangle
        {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: AppTheme.colourText
        }

        Repeater
        {
            model: root.step > 0 ? Math.floor((root.to - root.from) / root.step) + 1 : 0

            delegate: Rectangle
            {
                required property int index

                readonly property real ratio: index / Math.max(1, Math.floor((root.to - root.from) / root.step))

                x: ratio * (parent.width - width)
                anchors.verticalCenter: parent.verticalCenter
                width: AppMetrics.borderWidth * 2
                height: AppMetrics.gapSmall
                radius: width / 2
                visible: (root.to - root.from) / root.step <= 20
                         && parent.width * root.step / (root.to - root.from) >= AppMetrics.sliderTickSpacing
                color: ratio <= root.visualPosition ? AppTheme.colourText : AppTheme.colourBorder
            }
        }
    }

    handle: Rectangle
    {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: AppMetrics.gapLarge - AppMetrics.gapTight
        height: width
        radius: width / 2
        color: AppTheme.colourPanel
        border.width: AppMetrics.focusRingWidth
        border.color: root.visualFocus ? AppTheme.colourAccent : AppTheme.colourText
        scale: root.pressed ? 1.15 : (root.hovered ? 1.08 : 1.0)

        Behavior on scale { NumberAnimation { duration: AppMetrics.motionFast } }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
