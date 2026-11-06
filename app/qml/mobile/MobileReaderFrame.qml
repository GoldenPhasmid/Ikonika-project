import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property bool controlsVisible: true
    property alias topContent: topControls.data
    property alias bottomContent: bottomControls.data
    default property alias content: pageArea.data

    Item
    {
        id: pageArea

        anchors.fill: parent
    }

    Rectangle
    {
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
