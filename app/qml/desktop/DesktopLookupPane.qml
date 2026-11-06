import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle
{
    id: root

    required property var lookup
    property int maxStage: 1

    property string sourceSentence: ""

    signal addWordRequested(string word, string translation, string partOfSpeech, string hint)

    readonly property string term: field.term
    readonly property bool typing: field.activeFocus
    readonly property var results: root.lookup.results
    readonly property bool found: root.term.length > 0 && root.results.length > 0
    readonly property bool missing: root.term.length > 0 && root.results.length === 0

    function search()
    {
        const term = field.term

        if (term.length > 0)
        {
            root.lookup.search(term)

            return
        }

        root.lookup.clear()
    }

    function refresh()
    {
        if (!root.found)
        {
            return
        }

        root.lookup.search(root.term)
    }

    function show(term, focusField, sentence)
    {
        root.sourceSentence = sentence || ""
        field.text = term
        root.search()

        if (focusField)
        {
            field.forceActiveFocus()
            field.selectAll()
        }
    }

    Connections
    {
        target: root.lookup

        function onLanguagesChanged()
        {
            root.search()
        }
    }

    width: grip.paneWidth
    color: AppTheme.colourPanel

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Dictionary")

    MouseArea
    {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: function(wheel) { wheel.accepted = true }
    }

    DesktopDivider
    {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        vertical: true
    }

    DesktopPaneGrip
    {
        id: grip

        widthKey: "lookup"
        defaultWidth: AppMetrics.explainPaneWidth
        onLeft: true
    }

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: AppMetrics.gapMedium
        anchors.leftMargin: AppMetrics.gapMedium + AppMetrics.borderWidth
        spacing: AppMetrics.gapCompact

        DesktopSearchArea
        {
            id: field

            objectName: "reader_lookup_field"

            Layout.fillWidth: true

            onTermChanged: root.search()
        }

        DesktopMachineTranslation
        {
            visible: AppMachineTranslation.enabled && root.missing
            lookup: root.lookup
            term: root.term

            onAddRequested: function(word, translation) { root.addWordRequested(word, translation, "", root.sourceSentence) }
        }

        DesktopButton
        {
            objectName: "reader_add_by_hand"

            Layout.fillWidth: true
            visible: root.missing && !root.lookup.translating && root.lookup.machineTranslation.length === 0
            iconName: "plus"
            text: qsTr("Add as a word")

            onClicked: root.addWordRequested(root.term, "", "", root.sourceSentence)
        }

        DesktopWordList
        {
            objectName: "reader_lookup_list"

            visible: root.found

            Accessible.name: qsTr("Dictionary results")

            model: root.results

            delegate: DesktopWordRow
            {
                required property var modelData

                word: modelData.displayWord || modelData.word
                translation: modelData.translation || ""
                cefrLevel: modelData.cefrLevel || ""
                stage: modelData.stage !== undefined ? modelData.stage : -1
                completed: !!modelData.completed
                maxStage: root.maxStage

                onClicked: root.addWordRequested(modelData.word,
                                                 (modelData.translation || "").split(", ")[0] || "",
                                                 modelData.partOfSpeech || "", root.sourceSentence)
            }
        }

        Item
        {
            visible: !root.found
            Layout.fillHeight: true
        }
    }
}
