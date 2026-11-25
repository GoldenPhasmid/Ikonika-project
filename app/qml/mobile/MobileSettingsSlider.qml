import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property string title

    property string description: ""
    property int value: 0
    property int from: 0
    property int to: 100
    property int step: 1
    property string suffix: ""
    property string valueText: ""
    property bool interactive: true
    property bool lastRow: false

    signal edited(int newValue)

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + (AppMetrics.gapMedium * 2)

    opacity: root.interactive ? 1.0 : 0.5

    ColumnLayout
    {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: AppMetrics.contentPadding
        anchors.rightMargin: AppMetrics.contentPadding
        anchors.verticalCenter: parent.verticalCenter

        spacing: AppMetrics.gapSmall

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            Text
            {
                Layout.fillWidth: true

                text: root.title
                font.pixelSize: AppMetrics.fontLabel
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
                elide: Text.ElideRight
            }

            Text
            {
                text: root.valueText.length > 0 ? root.valueText
                    : root.suffix.length > 0
                    ? qsTr("%1 %2").arg(root.value).arg(root.suffix)
                    : root.value

                font.pixelSize: AppMetrics.fontLabel
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourSubtle
            }
        }

        Text
        {
            Layout.fillWidth: true

            visible: root.description.length > 0
            text: root.description
            font.pixelSize: AppMetrics.fontMeta
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourSubtle
            wrapMode: Text.WordWrap
        }

        DesktopSlider
        {
            Layout.fillWidth: true
            implicitHeight: AppMetrics.buttonHeightLarge

            from: root.from
            to: root.to
            step: root.step
            current: root.value
            enabled: root.interactive
            accessibleTitle: root.title

            onEdited: function(newValue) { root.edited(newValue) }
        }
    }

    Rectangle
    {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.contentPadding

        width: parent.width - AppMetrics.contentPadding
        height: AppMetrics.borderWidth

        visible: !root.lastRow
        color: AppTheme.colourBorder
    }
}
