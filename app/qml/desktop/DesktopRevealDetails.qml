import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property var controller: null

    signal wordClicked(string word, string sentence, rect wordRect)

    readonly property bool hasExample: root.controller !== null && root.controller.example.length > 0
    readonly property bool hasTranslation: root.hasExample && root.controller.exampleTranslation.length > 0
    readonly property bool hasDefinition: root.controller !== null && root.controller.definition.length > 0

    Layout.fillWidth: true
    implicitHeight: root.hasExample ? exampleRow.implicitHeight : definition.implicitHeight
    visible: root.hasExample || root.hasDefinition

    DesktopLabel
    {
        id: definition

        anchors.left: parent.left
        anchors.right: parent.right
        visible: !root.hasExample
        text: root.hasDefinition ? root.controller.definition : ""
        font.pixelSize: AppMetrics.sessionDefinitionSize
        color: AppTheme.colourMuted
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        maximumLineCount: AppMetrics.definitionLineCount
        elide: Text.ElideRight
    }

    RowLayout
    {
        id: exampleRow

        anchors.left: parent.left
        anchors.right: parent.right
        visible: root.hasExample
        spacing: AppMetrics.gapLarge

        Item
        {
            Layout.preferredWidth: root.hasTranslation ? (exampleRow.width - exampleRow.spacing * 2 - AppMetrics.borderWidth) / 2
                                                       : exampleRow.width
            implicitHeight: Math.min(foreignExample.implicitHeight, foreignLines.lineSpacing * AppMetrics.definitionLineCount)
            clip: true

            FontMetrics
            {
                id: foreignLines

                font: foreignExample.font
            }

            DesktopSentenceText
            {
                id: foreignExample

                anchors.left: parent.left
                anchors.right: parent.right
                sentence: root.hasExample ? root.controller.example : ""
                maximumSize: AppMetrics.sessionDefinitionSize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                onWordClicked: function(word, sentence, wordRect) { root.wordClicked(word, sentence, wordRect) }
            }
        }

        Rectangle
        {
            visible: root.hasTranslation
            Layout.fillHeight: true
            Layout.preferredWidth: AppMetrics.borderWidth
            color: AppTheme.colourBorder
        }

        DesktopLabel
        {
            visible: root.hasTranslation
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            text: root.hasTranslation ? root.controller.exampleTranslation : ""
            font.pixelSize: AppMetrics.sessionDefinitionSize
            color: AppTheme.colourMuted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            maximumLineCount: AppMetrics.definitionLineCount
            elide: Text.ElideRight
        }
    }
}
