import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    required property var lookup
    required property string word

    property int maxStage: 0

    heading: root.word
    maxHeightFraction: 0.6

    Text
    {
        Layout.fillWidth: true
        visible: root.lookup.results.length === 0

        text: qsTr("Not in the dictionary")
        color: AppTheme.colourMuted
        font.pixelSize: AppMetrics.fontBody
        font.family: AppMetrics.fontFamily
        wrapMode: Text.WordWrap
    }

    MobileWordCardList
    {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(root.height * 0.45,
            root.lookup.results.length * (56 + AppMetrics.gapCompact))
        visible: root.lookup.results.length > 0

        model: root.lookup.results
        maxStage: root.maxStage

        showCefr: false
        showEditAction: false
        showDeleteAction: false

        onWordClicked: function(index) { root.lookup.speak(root.lookup.results[index].word) }
        onVoicePlayRequested: function(index) { root.lookup.speak(root.lookup.results[index].word) }
    }
}
