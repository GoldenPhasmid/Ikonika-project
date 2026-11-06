import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Item
{
    id: root

    property bool compact: false

    signal clicked()

    implicitHeight: root.compact ? AppMetrics.readerRowHeight : Math.round(root.width / AppMetrics.readerTileAspect)

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Open book")

    Rectangle
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: root.compact ? Math.round(AppMetrics.readerRowCoverWidth / AppMetrics.readerTileAspect) : parent.height
        radius: root.compact ? AppMetrics.radiusSmall : AppMetrics.coverRadius
        color: AppTheme.colourText
        opacity: openTap.pressed ? AppTheme.opacityHover : 1.0

        MobileVectorIcon
        {
            anchors.centerIn: parent
            width: root.compact ? AppMetrics.readerOpenIconSize : AppMetrics.readerOpenPlusSize
            height: width

            svgPath: Icons.Icons.plus.path
            color: AppTheme.colourPanel
        }
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: AppMetrics.borderWidth
        visible: root.compact
        color: AppTheme.colourBorder
    }

    TapHandler
    {
        id: openTap

        onTapped: root.clicked()
    }
}
