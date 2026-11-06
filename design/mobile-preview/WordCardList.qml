import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property var model: []

    property int maxStage: 5

    property bool showVoice: true
    property bool showProgress: true
    property bool showTranslation: true
    property bool showCefr: true

    property bool showAddAction: false
    property bool showEditAction: true
    property bool showDeleteAction: true

    signal wordClicked(int index)
    signal voicePlayRequested(int index)
    signal addRequested(int index)
    signal editRequested(int index)
    signal deleteRequested(int index)

    Layout.fillWidth: true
    implicitHeight: list.contentHeight

    ListView {
        id: list

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: contentHeight
        interactive: false
        spacing: 8

        property var openRow: null

        model: root.model

        delegate: WordCard {
            id: cardDelegate

            required property var modelData
            required property int index

            width: list.width
            coordinator: list

            word: cardDelegate.modelData.word
            translation: cardDelegate.modelData.translation
            cefrLevel: cardDelegate.modelData.cefrLevel || ""
            stage: cardDelegate.modelData.stage
            completed: cardDelegate.modelData.completed
            maxStage: root.maxStage

            showVoice: root.showVoice
            showProgress: root.showProgress
            showTranslation: root.showTranslation
            showCefr: root.showCefr

            showAddAction: root.showAddAction
            showEditAction: root.showEditAction
            showDeleteAction: root.showDeleteAction

            onClicked: root.wordClicked(cardDelegate.index)
            onVoicePlayRequested: root.voicePlayRequested(cardDelegate.index)
            onAddRequested: root.addRequested(cardDelegate.index)
            onEditRequested: root.editRequested(cardDelegate.index)
            onDeleteRequested: root.deleteRequested(cardDelegate.index)
        }
    }
}
