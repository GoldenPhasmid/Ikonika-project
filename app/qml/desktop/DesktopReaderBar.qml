import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle
{
    id: root

    property int count: 0
    property int index: 0
    property bool paged: false
    property bool fitting: true

    signal positionRequested(int index)
    signal stepRequested(bool forward)
    signal zoomRequested(bool closer)
    signal fitRequested()

    readonly property int shownIndex: slider.pressed ? Math.round(slider.value) : root.index

    function percentOf(position)
    {
        return root.count > 1 ? Math.round(100 * position / (root.count - 1)) : 0
    }

    function labelOf(position)
    {
        return root.paged ? qsTr("%1 / %2").arg(position + 1).arg(Math.max(root.count, 1))
                          : qsTr("%1%").arg(root.percentOf(position))
    }

    implicitHeight: AppMetrics.readerBarHeight
    color: AppTheme.colourPanel

    Accessible.role: Accessible.ToolBar
    Accessible.name: qsTr("Reading position")

    DesktopDivider
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }

    RowLayout
    {
        anchors.fill: parent
        anchors.leftMargin: AppMetrics.gapMedium
        anchors.rightMargin: AppMetrics.gapMedium
        spacing: AppMetrics.gapSmall

        DesktopIconButton
        {
            objectName: "reader_previous"

            iconName: "chevron_left"
            text: root.paged ? qsTr("Previous page (Left)") : qsTr("Page up (Left)")
            enabled: root.index > 0

            onClicked: root.stepRequested(false)
        }

        DesktopLabel
        {
            Layout.preferredWidth: AppMetrics.readerPositionWidth
            text: root.labelOf(root.shownIndex)
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter

            Accessible.name: root.paged ? qsTr("Page %1 of %2").arg(root.shownIndex + 1).arg(root.count)
                                        : qsTr("%1 percent").arg(root.percentOf(root.shownIndex))
        }

        DesktopIconButton
        {
            objectName: "reader_next"

            iconName: "chevron_right"
            text: root.paged ? qsTr("Next page (Right)") : qsTr("Page down (Right)")
            enabled: root.index + 1 < root.count

            onClicked: root.stepRequested(true)
        }

        Slider
        {
            id: slider

            objectName: "reader_scrubber"

            Layout.fillWidth: true
            Layout.leftMargin: AppMetrics.gapSmall
            Layout.rightMargin: AppMetrics.gapSmall
            implicitHeight: AppMetrics.chipHeight

            from: 0
            to: Math.max(1, root.count - 1)
            stepSize: 1
            value: root.index
            enabled: root.count > 1
            focusPolicy: Qt.NoFocus
            hoverEnabled: true
            padding: 0

            Accessible.name: qsTr("Position in the book")

            onPressedChanged:
            {
                if (!slider.pressed)
                {
                    root.positionRequested(Math.round(slider.value))
                    slider.value = Qt.binding(function() { return root.index })
                }
            }

            background: Item
            {
                y: slider.availableHeight / 2 - height / 2
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
            }

            handle: Rectangle
            {
                x: slider.visualPosition * (slider.availableWidth - width)
                y: slider.availableHeight / 2 - height / 2
                width: AppMetrics.gapLarge - AppMetrics.gapTight
                height: width
                radius: width / 2
                color: AppTheme.colourPanel
                border.width: AppMetrics.focusRingWidth
                border.color: AppTheme.colourText
            }

            HoverHandler
            {
                cursorShape: Qt.PointingHandCursor
            }
        }

        DesktopIconButton
        {
            objectName: "reader_zoom_out"

            iconName: "minus"
            shortcutKey: "-"
            text: root.paged ? qsTr("Zoom out (Ctrl+-)") : qsTr("Smaller text (Ctrl+-)")

            onClicked: root.zoomRequested(false)
        }

        DesktopButton
        {
            objectName: "reader_fit"

            visible: root.paged
            tone: "ghost"
            text: qsTr("Fit")
            keyHint: qsTr("Ctrl+0")
            hintOnControl: true
            enabled: !root.fitting

            onClicked: root.fitRequested()
        }

        DesktopIconButton
        {
            objectName: "reader_zoom_in"

            iconName: "plus"
            shortcutKey: "+"
            text: root.paged ? qsTr("Zoom in (Ctrl++)") : qsTr("Larger text (Ctrl++)")

            onClicked: root.zoomRequested(true)
        }
    }
}
