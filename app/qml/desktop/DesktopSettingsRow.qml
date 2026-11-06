import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ItemDelegate
{
    id: root

    property string description: ""
    property bool clickable: false
    property bool destructive: false
    property bool lastRow: false
    property bool showChevron: root.clickable
    property bool stacked: false

    default property alias control: controlSlot.data

    Layout.fillWidth: true
    implicitHeight: Math.max(AppMetrics.listRowHeight, contentGrid.implicitHeight + AppMetrics.gapMedium * 2)

    leftPadding: AppMetrics.gapLarge
    rightPadding: AppMetrics.gapLarge
    topPadding: AppMetrics.gapMedium
    bottomPadding: AppMetrics.gapMedium

    hoverEnabled: root.clickable
    focusPolicy: root.clickable ? Qt.StrongFocus : Qt.NoFocus

    Accessible.role: root.clickable ? Accessible.Button : Accessible.Grouping
    Accessible.name: root.text
    Accessible.description: root.description

    contentItem: GridLayout
    {
        id: contentGrid

        columns: root.stacked ? 1 : 3
        columnSpacing: AppMetrics.gapMedium
        rowSpacing: AppMetrics.gapCompact

        ColumnLayout
        {
            id: textColumn

            Layout.fillWidth: true
            spacing: AppMetrics.gapTight

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.text
                font.pixelSize: AppMetrics.fontBody
                font.weight: Font.Medium
                color: root.destructive ? AppTheme.colourWrong : AppTheme.colourText
                wrapMode: Text.WordWrap
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
        }

        RowLayout
        {
            id: controlSlot

            Layout.fillWidth: root.stacked
            Layout.alignment: root.stacked ? Qt.AlignLeft : Qt.AlignVCenter
            spacing: AppMetrics.gapSmall
        }

        DesktopIcon
        {
            visible: root.showChevron
            name: "chevron_right"
            width: AppMetrics.iconSizeSmall
            height: AppMetrics.iconSizeSmall
            color: AppTheme.colourMuted
        }
    }

    background: Rectangle
    {
        color: root.clickable && (root.hovered || root.down) ? AppTheme.colourHover : AppTheme.faded(AppTheme.colourHover, 0)
        radius: AppMetrics.radiusLarge

        DesktopDivider
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: AppMetrics.gapLarge
            anchors.rightMargin: AppMetrics.gapLarge
            visible: !root.lastRow
        }

        DesktopFocusRing
        {
            anchors.margins: 0
            visible: root.visualFocus
            baseRadius: AppMetrics.radiusLarge
        }
    }

    HoverHandler
    {
        enabled: root.clickable
        cursorShape: Qt.PointingHandCursor
    }
}
