import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property var meaning: ({})
    property bool showDefinition: true
    property bool showNativeDefinition: true

    signal wordClicked(string word, string sentence)

    spacing: AppMetrics.gapCompact

    ColumnLayout
    {
        Layout.fillWidth: true
        visible: root.showDefinition || root.showNativeDefinition
        spacing: AppMetrics.gapTight

        DesktopLabel
        {
            Layout.fillWidth: true
            visible: root.showDefinition && text.length > 0
            text: root.meaning.definition || ""
            font.pixelSize: AppMetrics.dictionaryFontBody
            wrapMode: Text.WordWrap
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            visible: root.showNativeDefinition && text.length > 0
            text: root.meaning.nativeDefinition || ""
            color: AppTheme.colourSubtle
            wrapMode: Text.WordWrap
        }
    }

    Item
    {
        Layout.fillWidth: true
        visible: (root.meaning.example || "").length > 0
        implicitHeight: exampleColumn.implicitHeight

        Rectangle
        {
            width: AppMetrics.focusRingWidth
            height: parent.height
            radius: width / 2
            color: AppTheme.colourAccent
            opacity: AppTheme.opacitySecondary
        }

        ColumnLayout
        {
            id: exampleColumn

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: AppMetrics.gapMedium
            spacing: AppMetrics.gapTight

            DesktopSentenceText
            {
                sentence: root.meaning.example || ""
                maximumSize: AppMetrics.fontBody
                font.weight: Font.Normal
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignTop

                onWordClicked: function(word, sentence) { root.wordClicked(word, sentence) }
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.meaning.exampleTranslation || ""
                font.italic: true
                color: AppTheme.colourSubtle
                wrapMode: Text.WordWrap
            }
        }
    }
}
