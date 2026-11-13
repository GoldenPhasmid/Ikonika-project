import QtQuick
import QtQuick.Controls.Basic

Button
{
    id: root

    property bool pdf: true
    property ReaderReadAloud readAloud: null

    implicitWidth: AppMetrics.headerIconTapWidth
    implicitHeight: AppMetrics.headerIconTapWidth

    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    text: qsTr("Appearance")
    Accessible.name: root.text

    ToolTip.visible: root.hovered && !appearancePopover.visible
    ToolTip.text: root.text
    ToolTip.delay: AppMetrics.tooltipDelay

    onClicked: appearancePopover.visible ? appearancePopover.close() : appearancePopover.open()

    contentItem: Item
    {
        DesktopIcon
        {
            anchors.centerIn: parent
            name: "text_size"
            color: appearancePopover.visible ? AppTheme.colourAccent : AppTheme.colourText
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: root.down || appearancePopover.visible
               ? AppTheme.colourPressed
               : (root.hovered ? AppTheme.colourHover : AppTheme.faded(AppTheme.colourHover, 0))

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopFocusRing
        {
            visible: root.visualFocus
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }

    DesktopPopover
    {
        id: appearancePopover

        objectName: "reader_appearance"

        x: root.width - width
        y: root.height + AppMetrics.gapTight
        width: AppMetrics.readerSettingsWidth
        heading: qsTr("Appearance")

        DesktopReaderSettingsRows
        {
            showPdf: root.pdf
            showText: !root.pdf
            readAloud: root.pdf ? null : root.readAloud
            inset: AppMetrics.gapCompact
        }
    }
}
