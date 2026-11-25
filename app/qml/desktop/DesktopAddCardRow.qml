import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    implicitHeight: AppMetrics.listRowHeight
    leftPadding: AppMetrics.gapMedium
    rightPadding: AppMetrics.gapSmall
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    text: qsTr("Add card")

    Accessible.role: Accessible.Button
    Accessible.name: root.text

    contentItem: RowLayout
    {
        spacing: AppMetrics.gapCompact

        Rectangle
        {
            implicitWidth: AppMetrics.progressRingSize
            implicitHeight: AppMetrics.progressRingSize
            radius: width / 2
            color: Qt.tint(AppTheme.colourText, AppTheme.faded(AppTheme.colourPanel, root.down ? AppTheme.alphaStrong : (root.hovered ? AppTheme.alphaSoft : 0)))

            Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

            DesktopIcon
            {
                anchors.centerIn: parent
                name: "plus"
                color: AppTheme.colourPanel
            }
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.text
            font.weight: Font.DemiBold
            color: AppTheme.colourText
            elide: Text.ElideRight
        }
    }

    background: DesktopSelectionBackground
    {
        hovered: root.hovered
        pressed: root.down

        DesktopFocusRing
        {
            anchors.margins: 0
            radius: baseRadius
            visible: root.visualFocus
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
