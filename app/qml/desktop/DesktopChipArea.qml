import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Flickable
{
    id: root

    default property alias content: flow.data
    property alias spacing: flow.spacing
    property alias centred: flow.centred
    property bool followEnd: false
    readonly property real rowLimitHeight: AppMetrics.buttonHeight * AppMetrics.tileRowLimit
        + flow.spacing * (AppMetrics.tileRowLimit - 1)

    function showLastRow()
    {
        contentY = Math.max(0, contentHeight - height)
    }

    Layout.fillWidth: true
    implicitHeight: Math.min(contentHeight, root.rowLimitHeight)
    contentWidth: width
    contentHeight: flow.implicitHeight
    flickableDirection: Flickable.VerticalFlick
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    onContentHeightChanged: if (root.followEnd) Qt.callLater(root.showLastRow)
    onHeightChanged: if (root.followEnd) Qt.callLater(root.showLastRow)

    ScrollBar.vertical: DesktopScrollBar {}

    DesktopCenteredFlow
    {
        id: flow

        width: root.width
        spacing: AppMetrics.gapSmall
    }
}
