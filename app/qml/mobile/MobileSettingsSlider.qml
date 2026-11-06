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

    readonly property real fraction: root.to > root.from
        ? Math.max(0, Math.min(1, (root.value - root.from) / (root.to - root.from)))
        : 0

    function valueFromRatio(ratio)
    {
        const range = root.to - root.from
        const steps = Math.round(range / root.step)
        const clampedRatio = Math.max(0, Math.min(1, ratio))
        const snapped = Math.round(clampedRatio * steps) * root.step

        return root.from + Math.max(0, Math.min(range, snapped))
    }

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

        Item
        {
            id: track

            Layout.fillWidth: true
            Layout.preferredHeight: thumb.height

            MobileDotProgress
            {
                anchors.verticalCenter: parent.verticalCenter

                width: parent.width
                height: AppMetrics.progressBarHeight

                progress: root.fraction
            }

            Rectangle
            {
                id: thumb

                x: (track.width - width) * root.fraction
                anchors.verticalCenter: parent.verticalCenter

                width: 20
                height: 20
                radius: height / 2

                color: AppTheme.colourPanel
                border.color: AppTheme.colourText
                border.width: AppMetrics.borderWidth

                Behavior on x { enabled: !dragArea.pressed; NumberAnimation { duration: AppMetrics.motionFast } }
            }

            MouseArea
            {
                id: dragArea

                anchors.fill: parent
                anchors.topMargin: -(AppMetrics.buttonHeightLarge - track.height) / 2
                anchors.bottomMargin: anchors.topMargin
                enabled: root.interactive
                preventStealing: true

                onPressed: function(mouse) { root.edited(root.valueFromRatio(mouse.x / track.width)) }
                onPositionChanged: function(mouse)
                {
                    if (dragArea.pressed)
                    {
                        root.edited(root.valueFromRatio(mouse.x / track.width))
                    }
                }
            }
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
