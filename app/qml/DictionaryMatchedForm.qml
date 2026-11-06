import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property string form: ""
    property var readings: []
    property color tint: AppTheme.colourAccent

    spacing: AppMetrics.gapSmall
    visible: root.form.length > 0

    Text
    {
        Layout.fillWidth: true
        text: root.form
        textFormat: Text.PlainText
        color: root.tint
        font.family: AppMetrics.fontFamily
        font.pixelSize: AppMetrics.fontTitle
        font.weight: Font.DemiBold
        wrapMode: Text.Wrap
    }

    Flow
    {
        id: chips

        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        Repeater
        {
            model: root.readings

            Rectangle
            {
                id: chip

                required property string modelData
                readonly property int separator: modelData.indexOf(" · ")
                readonly property string tense: separator < 0 ? modelData : modelData.slice(0, separator)
                readonly property string subjects: separator < 0 ? "" : modelData.slice(separator + 3)

                width: Math.min(chips.width, content.implicitWidth + AppMetrics.gapCompact * 2)
                height: content.implicitHeight + AppMetrics.gapSmall * 2
                radius: AppMetrics.radiusSmall
                color: AppTheme.faded(root.tint, AppTheme.alphaFaint)
                border.width: AppMetrics.borderWidth
                border.color: AppTheme.faded(root.tint, AppTheme.alphaMedium)

                RowLayout
                {
                    id: content

                    anchors.fill: parent
                    anchors.leftMargin: AppMetrics.gapCompact
                    anchors.rightMargin: AppMetrics.gapCompact
                    anchors.topMargin: AppMetrics.gapSmall
                    anchors.bottomMargin: AppMetrics.gapSmall
                    spacing: AppMetrics.gapSmall

                    Text
                    {
                        Layout.fillWidth: true
                        text: chip.tense
                        textFormat: Text.PlainText
                        color: root.tint
                        font.family: AppMetrics.fontFamily
                        font.pixelSize: AppMetrics.fontSmall
                        font.weight: Font.Medium
                        wrapMode: Text.Wrap
                    }

                    Rectangle
                    {
                        visible: chip.subjects.length > 0
                        Layout.fillHeight: true
                        implicitWidth: AppMetrics.borderWidth
                        color: AppTheme.faded(root.tint, AppTheme.alphaMedium)
                    }

                    Text
                    {
                        visible: chip.subjects.length > 0
                        Layout.maximumWidth: chips.width / 2
                        text: chip.subjects
                        textFormat: Text.PlainText
                        color: AppTheme.colourText
                        font.family: AppMetrics.fontFamily
                        font.pixelSize: AppMetrics.fontSmall
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
