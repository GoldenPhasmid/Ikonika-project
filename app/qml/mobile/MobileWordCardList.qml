import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property var model: []

    required property int maxStage

    property bool showVoice: true
    property bool showProgress: true
    property bool showTranslation: true
    property bool showCefr: true
    property bool showPartOfSpeech: false

    property bool showAddAction: false
    property bool showLookAction: false
    property bool showOpenAction: false
    property bool showEditAction: true
    property bool showDeleteAction: true
    property bool showCompleteAction: false
    property bool showResetAction: false

    property real bottomInset: 0

    property alias contentY: list.contentY

    signal wordClicked(int index)
    signal voicePlayRequested(int index)
    signal addRequested(int index)
    signal lookRequested(int index)
    signal openRequested(int index)
    signal editRequested(int index)
    signal deleteRequested(int index)
    signal completeRequested(int index)
    signal resetRequested(int index)

    Layout.fillWidth: true
    Layout.fillHeight: true
    clip: true

    ListView
    {
        id: list

        anchors.fill: parent
        clip: true
        spacing: AppMetrics.gapCompact
        bottomMargin: root.bottomInset

        property var openRow: null

        model: root.model

        delegate: MobileWordCard
        {
            id: cardDelegate

            required property var modelData
            required property int index

            width: list.width
            coordinator: list

            word: cardDelegate.modelData.displayWord || cardDelegate.modelData.word
            translation: cardDelegate.modelData.translation
            cefrLevel: cardDelegate.modelData.cefrLevel || ""
            partOfSpeech: cardDelegate.modelData.partOfSpeech || ""
            gender: cardDelegate.modelData.gender ?? -1
            verbGroup: cardDelegate.modelData.verbGroup || ""
            stage: cardDelegate.modelData.stage
            completed: cardDelegate.modelData.completed
            maxStage: root.maxStage

            showVoice: root.showVoice
            showProgress: root.showProgress
            showTranslation: root.showTranslation
            showCefr: root.showCefr
            showPartOfSpeech: root.showPartOfSpeech

            showAddAction: cardDelegate.modelData.showAddAction !== undefined
                ? cardDelegate.modelData.showAddAction : root.showAddAction
            showLookAction: cardDelegate.modelData.showLookAction !== undefined
                ? cardDelegate.modelData.showLookAction : root.showLookAction
            showOpenAction: root.showOpenAction
            showEditAction: cardDelegate.modelData.showEditAction !== undefined
                ? cardDelegate.modelData.showEditAction : root.showEditAction
            showDeleteAction: cardDelegate.modelData.showDeleteAction !== undefined
                ? cardDelegate.modelData.showDeleteAction : root.showDeleteAction
            showCompleteAction: cardDelegate.modelData.showCompleteAction !== undefined
                ? cardDelegate.modelData.showCompleteAction : root.showCompleteAction
            showResetAction: cardDelegate.modelData.showResetAction !== undefined
                ? cardDelegate.modelData.showResetAction : root.showResetAction

            onClicked: root.wordClicked(cardDelegate.index)
            onVoicePlayRequested: root.voicePlayRequested(cardDelegate.index)
            onAddRequested: root.addRequested(cardDelegate.index)
            onLookRequested: root.lookRequested(cardDelegate.index)
            onOpenRequested: root.openRequested(cardDelegate.index)
            onEditRequested: root.editRequested(cardDelegate.index)
            onDeleteRequested: root.deleteRequested(cardDelegate.index)
            onCompleteRequested: root.completeRequested(cardDelegate.index)
            onResetRequested: root.resetRequested(cardDelegate.index)
        }
    }
}
