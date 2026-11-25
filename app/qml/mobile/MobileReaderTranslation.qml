import QtQuick
import QtQuick.Layouts

ReaderTranslationBase
{
    id: root

    Rectangle
    {
        id: box

        objectName: "reader_translation"

        readonly property real below: root.geometry ? root.geometry.bottom + AppMetrics.gapSmall : 0
        readonly property real above: root.geometry ? root.geometry.top - AppMetrics.gapSmall - box.height : 0

        visible: root.open && root.onScreen && !root.searching
        width: Math.min(root.width - AppMetrics.gapMedium * 2, AppMetrics.readerPopupWidth)
        height: content.implicitHeight + AppMetrics.gapSmall * 2
        x: Math.max(AppMetrics.gapMedium, Math.min(root.width - box.width - AppMetrics.gapMedium, root.geometry ? root.geometry.left : 0))
        y: box.below + box.height <= root.height - AppMetrics.gapMedium || box.above < AppMetrics.gapMedium ? box.below : box.above

        radius: AppMetrics.radiusLarge
        color: AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder

        Accessible.role: Accessible.Pane
        Accessible.name: qsTr("Translation")

        onVisibleChanged: if (box.visible) entrance.restart()

        NumberAnimation
        {
            id: entrance

            target: box
            property: "opacity"
            from: 0
            to: 1
            duration: AppMetrics.motionFast
        }

        MouseArea
        {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        ColumnLayout
        {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppMetrics.gapSmall
            spacing: AppMetrics.gapTight

            Repeater
            {
                model: root.shownMatches

                delegate: MobileWordCard
                {
                    required property var modelData
                    required property int index

                    objectName: "reader_translation_card_" + index

                    Layout.fillWidth: true

                    word: modelData.displayWord || modelData.word
                    translation: modelData.translation || ""
                    partOfSpeech: root.shownMatches.length > 1 ? (modelData.partOfSpeechLabel || "") : ""
                    verbGroup: modelData.verbGroup || ""
                    stage: modelData.stage !== undefined ? modelData.stage : -1
                    completed: !!modelData.completed
                    maxStage: root.maxStage

                    showVoice: false
                    showCefr: false
                    showEditAction: false
                    showDeleteAction: false
                    showPartOfSpeech: root.shownMatches.length > 1

                    onClicked: root.addMatch(modelData)
                }
            }

            MobileMachineTranslation
            {
                Layout.fillWidth: true
                visible: AppMachineTranslation.enabled && (!root.cardShown || root.lookup.formTerm.length > 0)
                lookup: root.lookup
                term: root.text

                onAddRequested: function(word, translation)
                {
                    root.addWordRequested(word, translation, "phrase", root.sentence)
                }
            }

            Text
            {
                objectName: "reader_translation_empty"

                Layout.fillWidth: true
                visible: !root.cardShown && !root.lookup.translating && !root.translated && !root.downloadShown
                text: root.brief ? qsTr("Not in the dictionary") : qsTr("No translation")
                color: AppTheme.colourMuted
                font.pixelSize: AppMetrics.fontLead
                font.family: AppMetrics.fontFamily
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
            }

            RowLayout
            {
                Layout.fillWidth: true
                Layout.preferredHeight: AppMetrics.iconTapSize
                visible: root.actionsShown
                spacing: 0

                MobileIconButton
                {
                    objectName: "reader_translation_listen"

                    Layout.fillHeight: true

                    iconName: "speaker_on"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Listen")

                    onClicked: root.lookup.speak(root.cardShown ? root.match.word : root.text)
                }

                MobileIconButton
                {
                    objectName: "reader_translation_read_aloud"

                    Layout.fillHeight: true

                    visible: root.readAloudOffered
                    iconName: "headphones"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Read aloud from here")

                    onClicked: root.readAloudRequested()
                }

                MobileIconButton
                {
                    objectName: "reader_translation_copy"

                    Layout.fillHeight: true

                    visible: !root.cardShown && root.translated
                    iconName: "copy"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Copy translation")

                    onClicked: root.lookup.copyToClipboard(root.lookup.machineTranslation)
                }

                MobileIconButton
                {
                    objectName: "reader_translation_dictionary"

                    Layout.fillHeight: true

                    visible: root.cardShown
                    iconName: "nav_words"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Open in dictionary")

                    onClicked: root.dictionaryRequested(root.match.word)
                }

                MobileIconButton
                {
                    objectName: "reader_translation_add"

                    Layout.fillHeight: true

                    visible: root.brief || root.translated
                    iconName: "plus"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Add to My Vocabulary")

                    onClicked: root.addWord()
                }

                MobileIconButton
                {
                    objectName: "reader_translation_known"

                    Layout.fillHeight: true

                    visible: root.knowable
                    iconName: root.known ? "circle_check" : "check"
                    iconColour: root.known ? AppTheme.colourAccent : AppTheme.colourMuted
                    text: root.known ? qsTr("Known. Tap to undo") : qsTr("I know this word")

                    onClicked: root.toggleKnown()
                }

                Item
                {
                    Layout.fillWidth: true
                }
            }
        }
    }
}
