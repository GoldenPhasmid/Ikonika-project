import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property var meaning: ({})
    property bool showDefinition: true
    property bool showNativeDefinition: true

    signal wordTapped(string word, string sentence)

    spacing: AppMetrics.gapSmall

    ColumnLayout
    {
        Layout.fillWidth: true
        visible: root.showDefinition || root.showNativeDefinition
        spacing: AppMetrics.gapTight

        Text
        {
            Layout.fillWidth: true
            visible: root.showDefinition && text.length > 0
            text: root.meaning.definition || ""
            font.pixelSize: AppMetrics.fontBody
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
            wrapMode: Text.WordWrap
        }

        Text
        {
            Layout.fillWidth: true
            visible: root.showNativeDefinition && text.length > 0
            text: root.meaning.nativeDefinition || ""
            font.pixelSize: AppMetrics.fontSmall
            font.family: AppMetrics.fontFamily
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
            width: 3
            height: parent.height
            radius: width / 2
            color: AppTheme.colourAccent
            opacity: 0.6
        }

        ColumnLayout
        {
            id: exampleColumn

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: AppMetrics.gapMedium
            spacing: AppMetrics.gapTight

            MobileSentenceText
            {
                sentence: root.meaning.example || ""
                font.pixelSize: AppMetrics.fontBody
                font.weight: Font.Normal
                horizontalAlignment: Text.AlignLeft
                holdEnabled: false
                tapEnabled: true

                onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
            }

            Text
            {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.meaning.exampleTranslation || ""
                font.pixelSize: AppMetrics.fontSmall
                font.italic: true
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourSubtle
                wrapMode: Text.WordWrap
            }
        }
    }
}
