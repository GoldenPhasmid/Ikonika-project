import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property bool controlsVisible: true
    property alias topContent: topControls.data
    property alias bottomContent: bottomControls.data
    default property alias content: pageArea.data

    readonly property real topCover: root.controlsVisible ? topPanel.height : 0
    readonly property real bottomCover: root.controlsVisible ? bottomPanel.height : 0

    property real topReserve: 0
    property real bottomReserve: 0

    onTopCoverChanged: if (root.topCover > 0) root.topReserve = root.topCover
    onBottomCoverChanged: if (root.bottomCover > 0) root.bottomReserve = root.bottomCover

    Component.onCompleted:
    {
        root.topReserve = root.topCover
        root.bottomReserve = root.bottomCover
    }

    Item
    {
        id: pageArea

        anchors.fill: parent
    }

    Rectangle
    {
        id: topPanel

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: topControls.implicitHeight + root.SafeArea.margins.top + AppMetrics.contentPadding
        z: 1
        visible: root.controlsVisible
        color: AppTheme.colourPanel

        MouseArea
        {
            anchors.fill: parent
        }

        ColumnLayout
        {
            id: topControls

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: root.SafeArea.margins.top
            spacing: AppMetrics.gapSmall
        }
    }

    Rectangle
    {
        id: bottomPanel

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: bottomControls.implicitHeight + AppMetrics.contentPadding * 2 + root.SafeArea.margins.bottom
        z: 1
        visible: root.controlsVisible
        color: AppTheme.colourPanel

        MouseArea
        {
            anchors.fill: parent
        }

        ColumnLayout
        {
            id: bottomControls

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: AppMetrics.contentPadding
            anchors.bottomMargin: AppMetrics.contentPadding + root.SafeArea.margins.bottom
            spacing: AppMetrics.gapSmall
        }
    }
}
