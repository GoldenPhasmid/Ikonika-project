import QtQuick

Item
{
    id: root

    required property var store

    signal done()
    signal addWordRequested(string word, string translation, string partOfSpeech)
    signal entryOpened(string headword, string partOfSpeech, string searchText)
    signal tabSelected(string key)

    onVisibleChanged: if (root.visible) shell.focusSearchField()

    function search()
    {
        controller.search(shell.searchFieldText + shell.searchFieldPreeditText)
    }

    function searchTerm(word)
    {
        shell.searchFieldText = word
        controller.search(word)
    }

    function refreshLearnedMarks()
    {
        if (shell.searchFieldText.length > 0 || shell.searchFieldPreeditText.length > 0)
        {
            root.search()
        }
    }

    function reset()
    {
        controller.clear()
        shell.searchFieldText = ""
    }

    DictionaryController
    {
        id: controller
    }

    MobileShell
    {
        id: shell

        anchors.fill: parent

        activeTab: "dictionary"
        headerTitle: qsTr("Dictionary")
        showBack: true
        showSearchField: true

        onBackRequested: root.done()
        onTabSelected: function(key) { root.tabSelected(key) }
        onSearchFieldAccepted: root.search()
        onSearchFieldTextChanged: root.search()
        onSearchFieldPreeditTextChanged: root.search()

        MobileWordCardList
        {
            id: entryList

            anchors.fill: parent
            anchors.margins: AppMetrics.contentPadding

            model: controller.results
            maxStage: root.store.stageCount

            showPartOfSpeech: false
            showAddAction: true
            showEditAction: false
            showDeleteAction: false

            onWordClicked: function(index)
            {
                const row = controller.results[index]
                root.entryOpened(row.word, "", shell.searchFieldText)
            }

            onVoicePlayRequested: function(index)
            {
                controller.speak(controller.results[index].word)
            }

            onAddRequested: function(index)
            {
                const row = controller.results[index]
                root.addWordRequested(row.word, row.translation.split(", ")[0] || "",
                                       row.partOfSpeech)
            }
        }

        MobileMachineTranslation
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppMetrics.contentPadding

            visible: AppMachineTranslation.enabled
                && controller.results.length === 0
                && (shell.searchFieldText + shell.searchFieldPreeditText).trim().length > 0
            lookup: controller
            term: (shell.searchFieldText + shell.searchFieldPreeditText).trim()

            onAddRequested: function(word, translation) { root.addWordRequested(word, translation, "") }
        }

        MobileFloatingButton
        {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: AppMetrics.contentPadding

            onClicked: root.addWordRequested(shell.searchFieldText, "", "")
        }
    }
}
