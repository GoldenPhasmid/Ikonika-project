import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property string title: ""
    property string description: ""
    property int value: 0
    property int from: 0
    property int to: 100
    property int step: 1
    property string suffix: ""
    property bool interactive: true
    property bool lastRow: false
    property real inset: AppMetrics.gapLarge

    signal edited(int newValue)

    property string valueText: root.suffix.length > 0 ? qsTr("%1 %2").arg(root.value).arg(root.suffix)
                                                               : root.value.toString()

    Layout.fillWidth: true
    implicitHeight: column.implicitHeight + AppMetrics.gapMedium * 2

    opacity: root.interactive ? 1.0 : AppTheme.opacityDisabled

    ColumnLayout
    {
        id: column

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.inset
        anchors.rightMargin: root.inset
        anchors.verticalCenter: parent.verticalCenter
        spacing: AppMetrics.gapSmall

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapMedium

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.title
                font.weight: Font.Medium
                wrapMode: Text.WordWrap
            }

            DesktopLabel
            {
                text: root.valueText
                font.weight: Font.DemiBold
                color: AppTheme.colourSubtle
            }
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            visible: root.description.length > 0
            text: root.description
            font.pixelSize: AppMetrics.fontSmall
            color: AppTheme.colourMuted
            wrapMode: Text.WordWrap
        }

        DesktopSlider
        {
            Layout.fillWidth: true

            from: root.from
            to: root.to
            step: root.step
            current: root.value
            enabled: root.interactive
            accessibleTitle: root.title

            onEdited: function(newValue) { root.edited(newValue) }
        }
    }

    DesktopDivider
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.inset
        anchors.rightMargin: root.inset
        visible: !root.lastRow
    }
}
