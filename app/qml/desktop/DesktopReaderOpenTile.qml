import QtQuick
import QtQuick.Controls.Basic

Button
{
    id: root

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    padding: 0

    z: root.hovered ? 1 : 0
    scale: root.hovered && root.enabled ? AppMetrics.coverHoverScale : 1.0

    Behavior on scale { NumberAnimation { duration: AppMetrics.motionBase; easing.type: Easing.OutCubic } }

    text: qsTr("Open book")
    Accessible.description: qsTr("Open a PDF, EPUB, FB2 or TXT file")

    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()

    implicitHeight: Math.round(root.width / AppMetrics.readerTileAspect)

    ToolTip.visible: root.hovered
    ToolTip.text: qsTr("Open book, or drop files here")
    ToolTip.delay: AppMetrics.tooltipDelay

    contentItem: Rectangle
    {
        radius: AppMetrics.coverRadius
        color: root.hovered ? Qt.tint(AppTheme.colourText, AppTheme.faded(AppTheme.colourPanel, AppTheme.alphaSoft))
                            : AppTheme.colourText

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopIcon
        {
            anchors.centerIn: parent
            name: "plus"
            width: AppMetrics.readerOpenPlusSize
            height: AppMetrics.readerOpenPlusSize
            color: AppTheme.colourPanel
        }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: AppMetrics.coverRadius
        }
    }

    background: Item {}

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
