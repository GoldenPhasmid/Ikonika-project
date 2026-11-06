import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property var controller: null
    property var card: null
    property int stageCount: 1
    property string emptyText: ""

    property string shownKey: ""

    signal saved()
    signal removeRequested(string cardId, string word)
    signal dictionaryRequested(string word, string partOfSpeech)

    readonly property bool inVocabulary: root.card !== null && root.card.inVocabulary
    readonly property var entry: (form.dictionary.entry.headword || "").toLowerCase() === (root.card ? root.card.targetWord.toLowerCase() : "")
        ? form.dictionary.entry : ({})

    readonly property string stageText:
    {
        if (!root.inVocabulary)
        {
            return qsTr("Not in My Vocabulary")
        }

        if (root.card.completed)
        {
            return qsTr("Learned")
        }

        if (root.card.box <= 0)
        {
            return qsTr("Not started")
        }

        return qsTr("Learning, stage %1 of %2").arg(Math.min(root.card.box, root.stageCount)).arg(root.stageCount)
    }

    function show(card, force)
    {
        if (!card)
        {
            root.shownKey = ""

            return
        }

        const key = card.inVocabulary ? card.id : "word:" + card.targetWord + ":" + card.partOfSpeech

        if (!force && key === root.shownKey)
        {
            return
        }

        root.shownKey = key

        if (card.inVocabulary)
        {
            form.openForEdit(card.id, card.targetWord, card.translation, card.gender, card.partOfSpeech, card.box)
        }
        else
        {
            form.openWith(card.targetWord, card.translation, card.partOfSpeech, "")
        }

        page.contentY = 0
    }

    function focusField()
    {
        form.focusField()
    }

    function revert()
    {
        console.info("Card pane reverts edits of", root.shownKey)

        root.show(root.card, true)
    }

    function commit()
    {
        if (!form.canCommit || (form.editing && !form.changed))
        {
            return
        }

        if (!form.commit())
        {
            console.warn("Card pane could not save", form.enteredWord)

            return
        }

        console.info("Card pane saved", form.enteredWord)

        form.markSaved()
        root.saved()
    }

    function speak()
    {
        if (root.card)
        {
            form.dictionary.speak(root.card.targetWord)
        }
    }

    Shortcut
    {
        sequence: "Ctrl+Space"
        enabled: root.visible && root.card !== null
        context: Qt.WindowShortcut

        onActivated: header.speak()
    }

    DesktopLabel
    {
        anchors.centerIn: parent
        width: Math.min(parent.width - AppMetrics.contentPadding * 2, AppMetrics.contentWidthNarrow)
        visible: root.card === null
        text: root.emptyText
        font.pixelSize: AppMetrics.fontLabel
        color: AppTheme.colourMuted
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    DesktopPage
    {
        id: page

        anchors.fill: parent
        visible: root.card !== null
        maximumWidth: AppMetrics.contentWidthNarrow
        spacing: AppMetrics.gapLarge

        DesktopEntryHeader
        {
            id: header

            headword: root.card ? root.card.word : ""
            gender: root.card ? root.card.gender : -1
            phonetic: root.entry.phonetic || ""
            frequencyBucket: root.entry.frequencyBucket || ""
            cefrLevel: root.entry.cefrLevel || ""

            onSpeakRequested: root.speak()
            onWebSearchRequested: form.dictionary.openWebSearch(root.card.targetWord)

            actions: [
                DesktopIconButton
                {
                    objectName: "card_pane_dictionary"

                    iconName: "nav_words"
                    text: qsTr("Open in Words")

                    onClicked: root.dictionaryRequested(root.card.targetWord, root.card.partOfSpeech)
                },
                DesktopIconButton
                {
                    objectName: "card_pane_remove"

                    visible: root.inVocabulary
                    iconName: "trash"
                    text: qsTr("Remove from My Vocabulary")

                    onClicked: root.removeRequested(root.card.id, root.card.targetWord)
                }
            ]
        }

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            DesktopProgressRing
            {
                visible: root.inVocabulary
                stage: root.card ? root.card.box : -1
                stageCount: root.stageCount
                completed: root.card ? root.card.completed : false
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.stageText
                color: AppTheme.colourSubtle
                elide: Text.ElideRight
            }

            DesktopButton
            {
                objectName: "card_pane_complete"

                visible: root.inVocabulary && !root.card.completed
                tone: "ghost"
                iconName: "check"
                text: qsTr("Mark as learned")

                onClicked: root.controller.completeCardProgress(root.card.id)
            }

            DesktopButton
            {
                objectName: "card_pane_reset"

                visible: root.inVocabulary && (root.card.box > 0 || root.card.completed)
                tone: "ghost"
                iconName: "undo"
                text: qsTr("Reset progress")

                onClicked: root.controller.resetCardProgress(root.card.id)
            }
        }

        DesktopCardForm
        {
            id: form

            controller: root.controller
            expanded: true

            onAccepted: root.commit()
            onWordClicked: function(word) { root.dictionaryRequested(word, "") }
        }

        RowLayout
        {
            Layout.alignment: Qt.AlignRight
            spacing: AppMetrics.gapSmall

            DesktopButton
            {
                objectName: "card_pane_revert"

                large: true
                Layout.preferredWidth: AppMetrics.actionButtonWidth
                visible: form.editing
                enabled: form.changed
                text: qsTr("Revert")

                onClicked: root.revert()
            }

            DesktopButton
            {
                objectName: "card_pane_commit"

                large: true
                Layout.preferredWidth: AppMetrics.actionButtonWidth
                tone: "primary"
                text: form.editing ? qsTr("Save") : qsTr("Add")
                keyHint: qsTr("Enter")
                enabled: form.canCommit && (form.changed || !form.editing)

                onClicked: root.commit()
            }
        }
    }
}
