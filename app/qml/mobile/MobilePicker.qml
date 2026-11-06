import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

ColumnLayout
{
    id: root

    required property string label
    required property var sheet

    property string title: root.label
    property var model: []
    property string textRole: ""
    property int currentIndex: -1

    signal picked(int index)

    readonly property var names:
    {
        const result = []

        for (let i = 0; i < root.model.length; i++)
        {
            result.push(root.textRole.length > 0 ? root.model[i][root.textRole] : root.model[i])
        }

        return result
    }

    readonly property string displayText:
        root.currentIndex >= 0 && root.currentIndex < root.names.length
            ? root.names[root.currentIndex] : ""

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    MobileSectionLabel { text: root.label }

    Rectangle
    {
        Layout.fillWidth: true
        Layout.preferredHeight: 47

        radius: AppMetrics.radiusMedium
        color: "transparent"
        opacity: root.enabled ? 1.0 : 0.5

        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder

        Text
        {
            anchors.left: parent.left
            anchors.right: chevron.left
            anchors.leftMargin: AppMetrics.contentPadding
            anchors.rightMargin: AppMetrics.gapSmall
            anchors.verticalCenter: parent.verticalCenter

            text: root.displayText
            font.pixelSize: AppMetrics.fontLabel
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
            elide: Text.ElideRight
        }

        MobileVectorIcon
        {
            id: chevron

            anchors.right: parent.right
            anchors.rightMargin: AppMetrics.contentPadding
            anchors.verticalCenter: parent.verticalCenter

            width: 12
            height: 12

            sourceWidth: Icons.Icons.chevron_down.width
            sourceHeight: Icons.Icons.chevron_down.height
            svgPath: Icons.Icons.chevron_down.path
            color: AppTheme.colourText
        }

        MouseArea
        {
            anchors.fill: parent
            enabled: root.enabled && root.names.length > 0

            onClicked: root.sheet.open(root.title,root.names, root.currentIndex,
                                       function(index) { root.picked(index) })
        }
    }
}
