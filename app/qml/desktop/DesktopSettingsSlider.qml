import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property string title: ""
    property string description: ""
    property int value: 0
    property int from: 0
    property int to: 100
    property int step: 1
    property string suffix: ""
    property bool interactive: true
    property bool lastRow: false
    property real inset: AppMetrics.gapLarge

    signal edited(int newValue)

    property string valueText: root.suffix.length > 0 ? qsTr("%1 %2").arg(root.value).arg(root.suffix)
                                                               : root.value.toString()

    Layout.fillWidth: true
    implicitHeight: column.implicitHeight + AppMetrics.gapMedium * 2

    opacity: root.interactive ? 1.0 : AppTheme.opacityDisabled

    ColumnLayout
    {
        id: column

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.inset
        anchors.rightMargin: root.inset
        anchors.verticalCenter: parent.verticalCenter
        spacing: AppMetrics.gapSmall

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapMedium

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.title
                font.weight: Font.Medium
                wrapMode: Text.WordWrap
            }

            DesktopLabel
            {
                text: root.valueText
                font.weight: Font.DemiBold
                color: AppTheme.colourSubtle
            }
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            visible: root.description.length > 0
            text: root.description
            font.pixelSize: AppMetrics.fontSmall
            color: AppTheme.colourMuted
            wrapMode: Text.WordWrap
        }

        Slider
        {
            id: slider

            Layout.fillWidth: true
            implicitHeight: AppMetrics.chipHeight

            from: root.from
            to: root.to
            stepSize: root.step
            snapMode: Slider.SnapAlways
            value: root.value
            enabled: root.interactive
            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            padding: 0

            Accessible.name: root.title

            onMoved: if (Math.round(slider.value) !== root.value) root.edited(Math.round(slider.value))

            background: Item
            {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth
                height: AppMetrics.progressBarHeight

                Rectangle
                {
                    anchors.fill: parent
                    radius: height / 2
                    color: AppTheme.colourFaint
                }

                Rectangle
                {
                    width: slider.visualPosition * parent.width
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
                        color: ratio <= slider.visualPosition ? AppTheme.colourText : AppTheme.colourBorder
                    }
                }
            }

            handle: Rectangle
            {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: AppMetrics.gapLarge - AppMetrics.gapTight
                height: width
                radius: width / 2
                color: AppTheme.colourPanel
                border.width: AppMetrics.focusRingWidth
                border.color: slider.visualFocus ? AppTheme.colourAccent : AppTheme.colourText
                scale: slider.pressed ? 1.15 : (slider.hovered ? 1.08 : 1.0)

                Behavior on scale { NumberAnimation { duration: AppMetrics.motionFast } }
            }

            HoverHandler
            {
                cursorShape: Qt.PointingHandCursor
            }
        }
    }

    DesktopDivider
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.inset
        anchors.rightMargin: root.inset
        visible: !root.lastRow
    }
}
