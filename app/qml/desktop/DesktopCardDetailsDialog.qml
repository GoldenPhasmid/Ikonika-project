import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

DesktopDialog
{
    id: root

    property var store: null
    property alias controller: form.controller

    signal added()
    signal dictionaryRequested(string word, string partOfSpeech)

    title: form.editing ? qsTr("Edit card") : qsTr("Create card")
    width: Math.min(AppMetrics.contentWidthNarrow, parent ? parent.width - AppMetrics.contentPadding * 2 : AppMetrics.contentWidthNarrow)

    function openWith(word, translation, partOfSpeech, gist, hint)
    {
        form.openWith(word, translation, partOfSpeech, gist, hint)
        root.open()

        Qt.callLater(form.focusField)
    }

    function openForEdit(cardId, word, translation, gender, partOfSpeech, box)
    {
        root.openWith(word, translation, partOfSpeech)
        form.openForEdit(cardId, word, translation, gender, partOfSpeech, box)
    }

    function commit()
    {
        if (form.commit())
        {
            root.added()
            root.close()
        }
    }

    Shortcut
    {
        sequence: "Ctrl+Space"
        enabled: root.opened && form.enteredWord.length > 0
        context: Qt.WindowShortcut

        onActivated: form.dictionary.speak(form.enteredWord)
    }

    Shortcut
    {
        sequence: "Backspace"
        enabled: root.opened && !form.typing
        context: Qt.WindowShortcut

        onActivated: root.close()
    }

    DesktopCardForm
    {
        id: form

        onAccepted: root.commit()

        onWordClicked: function(word)
        {
            root.close()
            root.dictionaryRequested(word, "")
        }
    }

    status: [
        DesktopProgressRing
        {
            visible: form.editing
            stage: form.editingCard ? form.editingCard.stage : form.editingStage
            stageCount: root.store ? root.store.stageCount : 1
            completed: form.editingCard ? form.editingCard.completed : false
        },
        DesktopIconButton
        {
            objectName: "word_dialog_speak"

            focusPolicy: Qt.NoFocus
            enabled: form.enteredWord.length > 0
            iconName: "speaker_on"
            text: qsTr("Pronounce (Ctrl+Space)")

            onClicked: form.dictionary.speak(form.enteredWord)
        },
        DesktopIconButton
        {
            objectName: "word_dialog_web"

            focusPolicy: Qt.NoFocus
            enabled: form.enteredWord.length > 0
            iconName: "external_link"
            text: qsTr("Search the web")

            onClicked: form.dictionary.openWebSearch(form.enteredWord)
        },
        DesktopIconButton
        {
            objectName: "word_dialog_dictionary"

            focusPolicy: Qt.NoFocus
            enabled: form.enteredWord.length > 0
            iconName: "nav_words"
            text: qsTr("Open in the dictionary")

            onClicked:
            {
                const word = form.enteredWord
                const partOfSpeech = form.editingPartOfSpeech

                root.close()
                root.dictionaryRequested(word, partOfSpeech)
            }
        },
        DesktopIconButton
        {
            objectName: "word_dialog_close"

            focusPolicy: Qt.NoFocus
            iconName: "close"
            text: qsTr("Close (Esc)")

            onClicked: root.close()
        }
    ]

    footer: [
        DesktopButton
        {
            large: true
            Layout.preferredWidth: AppMetrics.actionButtonWidth
            text: qsTr("Cancel")
            keyHint: qsTr("Backspace")

            onClicked: root.close()
        },
        DesktopButton
        {
            objectName: "word_dialog_commit"

            large: true
            Layout.preferredWidth: AppMetrics.actionButtonWidth
            tone: "primary"
            text: form.editing ? qsTr("Save") : qsTr("Add")
            keyHint: qsTr("Enter")
            enabled: form.canCommit

            onClicked: root.commit()
        }
    ]
}
