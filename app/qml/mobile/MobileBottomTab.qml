import QtQuick
import QtQuick.Shapes
import QtQuick.Layouts

Item
{
    id: root

    required property string label
    required property bool active

    property string icon: ""
    property real iconMinX: 0
    property real iconMinY: 0
    property real iconWidth: 1
    property real iconHeight: 1

    readonly property real iconBoxSize: 20
    readonly property real iconFitScale: root.iconBoxSize / Math.max(root.iconWidth, root.iconHeight)

    signal activated()

    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout
    {
        anchors.centerIn: parent
        spacing: 6

        Rectangle
        {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignHCenter

            radius: AppMetrics.radiusSmall
            color: root.active ? AppTheme.colourText : "transparent"
            border.color: AppTheme.colourBorder
            border.width: root.active ? 0 : AppMetrics.borderWidth

            Shape
            {
                anchors.centerIn: parent

                visible: root.icon.length > 0
                width: root.iconBoxSize
                height: root.iconBoxSize

                transform:
                [
                    Translate { x: -root.iconMinX; y: -root.iconMinY },
                    Scale { xScale: root.iconFitScale; yScale: root.iconFitScale },
                    Translate
                    {
                        x: (root.iconBoxSize - root.iconWidth * root.iconFitScale) / 2
                        y: (root.iconBoxSize - root.iconHeight * root.iconFitScale) / 2
                    }
                ]

                ShapePath
                {
                    strokeColor: root.active ? AppTheme.colourPanel : AppTheme.colourText
                    strokeWidth: 2
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin

                    PathSvg { path: root.icon }
                }
            }
        }

        Text
        {
            Layout.alignment: Qt.AlignHCenter

            text: root.label.toUpperCase()
            font.pixelSize: 10
            font.weight: root.active ? Font.DemiBold : Font.Medium
            font.family: AppMetrics.fontFamily
            font.letterSpacing: 0.4
            color: root.active ? AppTheme.colourText : AppTheme.colourMuted
        }
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: root.activated()
    }
}
