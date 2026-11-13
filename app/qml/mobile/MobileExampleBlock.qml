import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property string sentence: ""
    property string translation: ""

    signal wordTapped(string word, string sentence)

    function wordUnder(source, x, y)
    {
        const point = sentenceText.mapFromItem(source, x, y)

        return sentenceText.wordAt(point.x, point.y)
    }

    visible: root.sentence.length > 0
    Layout.fillWidth: true
    spacing: 0

    FontMetrics
    {
        id: sentenceMetrics

        font: sentenceText.font
    }

    FontMetrics
    {
        id: translationMetrics

        font: translationText.font
    }

    Rectangle
    {
        id: panel

        Layout.fillWidth: true
        implicitHeight: column.implicitHeight + AppMetrics.gapMedium * 2
        radius: AppMetrics.radiusMedium
        color: AppTheme.colourSunken

        ColumnLayout
        {
            id: column

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppMetrics.gapMedium
            spacing: 0

            MobileSentenceText
            {
                id: sentenceText

                Layout.bottomMargin: AppMetrics.gapCompact
                Layout.preferredHeight: Math.max(implicitHeight, sentenceMetrics.lineSpacing * 2)
                verticalAlignment: Text.AlignVCenter

                sentence: root.sentence
                font.pixelSize: AppMetrics.fontLead
                font.weight: Font.Normal
                holdEnabled: false
                tapEnabled: true

                onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
            }

            MobileSeparator
            {
                visible: root.translation.length > 0
                strong: true
                Layout.leftMargin: AppMetrics.gapLarge
                Layout.rightMargin: AppMetrics.gapLarge
            }

            Text
            {
                id: translationText

                visible: root.translation.length > 0
                Layout.fillWidth: true
                Layout.topMargin: AppMetrics.gapCompact
                Layout.preferredHeight: Math.max(implicitHeight, translationMetrics.lineSpacing * 2)
                verticalAlignment: Text.AlignVCenter

                text: root.translation
                font.pixelSize: AppMetrics.sessionOptionSize
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourMuted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
