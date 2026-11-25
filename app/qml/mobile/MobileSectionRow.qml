import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property string key
    property string current: ""
    property string label: ""
    property string iconName: ""
    property color dotColour: "transparent"
    property int count: 0

    signal chosen(string key)
    signal held()

    readonly property bool selected: root.current === root.key

    Layout.fillWidth: true
    implicitHeight: AppMetrics.buttonHeightLarge
    radius: height / 2
    color: root.selected ? AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaMedium)
                         : rowTap.pressed ? AppTheme.colourFaint : "transparent"

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("%1, %2").arg(root.label).arg(root.count)
    Accessible.selected: root.selected

    RowLayout
    {
        anchors.fill: parent
        anchors.leftMargin: AppMetrics.contentPadding
        anchors.rightMargin: AppMetrics.contentPadding
        spacing: AppMetrics.gapMedium

        DesktopIcon
        {
            visible: root.iconName.length > 0
            name: root.iconName
            width: AppMetrics.iconSize
            height: AppMetrics.iconSize
            color: root.selected ? AppTheme.colourText : AppTheme.colourSubtle
        }

        Rectangle
        {
            visible: root.iconName.length === 0
            Layout.preferredWidth: AppMetrics.iconSize
            Layout.preferredHeight: AppMetrics.iconSize

            color: "transparent"

            Rectangle
            {
                anchors.centerIn: parent
                width: AppMetrics.categoryDotSize
                height: width
                radius: AppMetrics.radiusSmall / 2
                color: root.dotColour
            }
        }

        Text
        {
            Layout.fillWidth: true
            text: root.label
            font.pixelSize: AppMetrics.fontLabel
            font.weight: root.selected ? Font.DemiBold : Font.Medium
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
            elide: Text.ElideRight
        }

        Text
        {
            text: root.count
            font.pixelSize: AppMetrics.fontSmall
            font.family: AppMetrics.fontFamily
            font.features: { "tnum": 1 }
            color: root.selected ? AppTheme.colourText : AppTheme.colourMuted
        }
    }

    TapHandler
    {
        id: rowTap

        onTapped: root.chosen(root.key)
        onLongPressed: root.held()
    }
}
