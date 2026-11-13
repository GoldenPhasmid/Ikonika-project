import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    default property alias content: page.content
    property alias footer: footerRow.data
    property alias maximumWidth: page.maximumWidth
    readonly property alias columnWidth: page.columnWidth

    DesktopPage
    {
        id: page

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footerArea.top
        anchors.bottomMargin: AppMetrics.gapMedium
        centred: true
    }

    Item
    {
        id: footerArea

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: AppMetrics.contentPadding
        width: page.columnWidth
        height: footerRow.implicitHeight

        RowLayout
        {
            id: footerRow

            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(footerRow.implicitWidth, footerArea.width)
            spacing: AppMetrics.gapSmall
        }
    }
}
