import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property bool compact: false

    signal clicked()

    implicitHeight: root.compact ? AppMetrics.readerRowHeight : Math.round(root.width / AppMetrics.readerTileAspect)

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("New deck")
    Accessible.onPressAction: root.clicked()

    readonly property color fill: tap.pressed ? Qt.tint(AppTheme.colourText, AppTheme.faded(AppTheme.colourPanel, AppTheme.alphaStrong))
                                              : AppTheme.colourText

    Rectangle
    {
        anchors.fill: parent
        visible: !root.compact
        radius: AppMetrics.coverRadius
        color: root.fill

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopIcon
        {
            anchors.centerIn: parent
            name: "plus"
            width: AppMetrics.newDeckIconSize
            height: AppMetrics.newDeckIconSize
            color: AppTheme.colourPanel
        }
    }

    Item
    {
        anchors.fill: parent
        visible: root.compact

        RowLayout
        {
            anchors.fill: parent
            spacing: AppMetrics.gapCompact

            Rectangle
            {
                Layout.preferredWidth: AppMetrics.readerRowCoverWidth
                Layout.preferredHeight: Math.round(AppMetrics.readerRowCoverWidth / AppMetrics.readerTileAspect)
                radius: AppMetrics.radiusSmall
                color: root.fill

                DesktopIcon
                {
                    anchors.centerIn: parent
                    name: "plus"
                    width: AppMetrics.iconSize
                    height: width
                    color: AppTheme.colourPanel
                }
            }

            Text
            {
                Layout.fillWidth: true
                text: qsTr("New deck")
                color: AppTheme.colourText
                font.pixelSize: AppMetrics.fontLabel
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                elide: Text.ElideRight
            }
        }

        Rectangle
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: AppMetrics.borderWidth
            color: AppTheme.colourBorder
        }
    }

    TapHandler
    {
        id: tap

        onTapped: root.clicked()
    }
}
