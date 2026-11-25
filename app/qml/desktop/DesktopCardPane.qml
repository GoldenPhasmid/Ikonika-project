import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    required property DictionaryController dictionary

    property var controller: null
    property var card: null
    property int stageCount: 1
    property string emptyText: ""
    property bool removable: true
    property bool quiet: false
    property bool compact: false
    property bool actionsButtonShown: true

    readonly property bool unsaved: root.card !== null && form.canCommit && form.changed
    readonly property bool typing: form.typing

    property string shownKey: ""

    signal saved()
    signal removeRequested(string cardId, string word)
    signal dictionaryRequested(string word, string partOfSpeech)
    signal actionsRequested()
    signal progressEdited(var card)

    readonly property bool inVocabulary: root.card !== null && root.card.inVocabulary
    readonly property bool progressEditable: root.inVocabulary && root.controller !== null
        && typeof root.controller.completeCardProgress === "function"
    readonly property bool canComplete: root.progressEditable && !root.card.completed
    readonly property bool canReset: root.progressEditable && (root.card.box > 0 || root.card.completed)
    readonly property bool canRemove: root.progressEditable && root.removable
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
            form.openForEdit(card.id, card.targetWord, card.translation, card.gender, card.partOfSpeech, card.box,
                             card.language || "")
        }
        else
        {
            form.openWith(card.targetWord, card.translation, card.partOfSpeech, card.gist || "", card.hint || "",
                          card.language || "")
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

    function completeProgress()
    {
        console.info("Card pane marks", root.card.targetWord, "as learned")

        if (root.controller.completeCardProgress(root.card.id))
        {
            root.progressEdited(Object.assign({}, root.card, { completed: true }))
        }
    }

    function resetProgress()
    {
        console.info("Card pane resets the progress of", root.card.targetWord)

        if (root.controller.resetCardProgress(root.card.id))
        {
            root.progressEdited(Object.assign({}, root.card, { box: 0, completed: false }))
        }
    }

    function remove()
    {
        root.removeRequested(root.card.id, root.card.targetWord)
    }

    function openInWords()
    {
        root.dictionaryRequested(root.card.targetWord, root.card.partOfSpeech)
    }

    function searchWeb()
    {
        form.dictionary.openWebSearch(root.card.targetWord)
    }

    function speak()
    {
        if (root.card)
        {
            form.dictionary.speak(root.card.word)
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

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: saveButton.visible ? saveButton.top : parent.bottom
        anchors.bottomMargin: saveButton.visible ? AppMetrics.gapSmall : 0
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
            webSearchShown: !root.compact

            onSpeakRequested: root.speak()
            onWebSearchRequested: root.searchWeb()

            actions: [
                DesktopIconButton
                {
                    objectName: "card_pane_dictionary"

                    visible: !root.compact
                    iconName: "nav_words"
                    text: qsTr("Open in Words")

                    onClicked: root.openInWords()
                },
                DesktopIconButton
                {
                    objectName: "card_pane_remove"

                    visible: !root.compact && root.inVocabulary && root.removable
                    iconName: "trash"
                    iconColour: AppTheme.colourWrong
                    text: qsTr("Remove from My Vocabulary")

                    onClicked: root.remove()
                },
                DesktopIconButton
                {
                    objectName: "card_pane_actions"

                    visible: root.compact && root.actionsButtonShown
                    iconName: "kebab"
                    text: qsTr("More actions")

                    onClicked: root.actionsRequested()
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

                visible: !root.compact && root.canComplete
                tone: "ghost"
                iconName: "check"
                text: qsTr("Mark as learned")

                onClicked: root.completeProgress()
            }

            DesktopButton
            {
                objectName: "card_pane_reset"

                visible: !root.compact && root.canReset
                tone: "ghost"
                iconName: "undo"
                text: qsTr("Reset progress")

                onClicked: root.resetProgress()
            }
        }

        DesktopCardForm
        {
            id: form

            dictionary: root.dictionary

            controller: root.controller
            expanded: true

            onAccepted: root.commit()
            onWordClicked: function(word) { root.dictionaryRequested(word, "") }
        }

        RowLayout
        {
            Layout.alignment: Qt.AlignHCenter
            visible: !root.compact
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
                tone: root.quiet && !root.unsaved ? "secondary" : "primary"
                text: form.editing ? qsTr("Save") : qsTr("Add")
                keyHint: root.quiet && !root.unsaved ? "" : qsTr("Enter")
                enabled: form.canCommit && (form.changed || !form.editing)

                onClicked: root.commit()
            }
        }
    }

    MobileWideButton
    {
        id: saveButton

        objectName: "card_pane_save"

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: AppMetrics.contentPadding
        width: page.columnWidth
        height: AppMetrics.buttonHeightLarge
        visible: root.compact && root.card !== null

        label: form.editing ? qsTr("Save") : qsTr("Add")
        interactive: form.canCommit && (form.changed || !form.editing)

        onClicked: root.commit()
    }
}
