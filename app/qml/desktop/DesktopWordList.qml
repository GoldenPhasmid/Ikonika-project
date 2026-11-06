import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ListView
{
    id: root

    property string emptyText: ""

    readonly property real rowWidth: root.width - (scrollBar.visible ? scrollBar.width : 0)

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.leftMargin: -AppMetrics.gapSmall
    Layout.rightMargin: -AppMetrics.gapSmall

    clip: true
    spacing: AppMetrics.borderWidth
    boundsBehavior: Flickable.StopAtBounds
    keyNavigationEnabled: false

    Accessible.role: Accessible.List

    ScrollBar.vertical: DesktopScrollBar { id: scrollBar }

    DesktopLabel
    {
        anchors.centerIn: parent
        width: parent.width - AppMetrics.gapLarge * 2
        visible: root.count === 0
        text: root.emptyText
        color: AppTheme.colourMuted
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}
