import QtQuick

Item
{
    id: root

    required property DictionaryController controller
    required property var store

    signal done()
    signal addWordRequested(string word, string translation, string partOfSpeech)
    signal entryOpened(string headword, string partOfSpeech, string searchText)
    signal tabSelected(string key)

    onVisibleChanged: if (root.visible) shell.focusSearchField()

    Component.onCompleted:
    {
        if (root.controller.query.length > 0)
        {
            shell.searchFieldText = root.controller.query
        }
    }

    function search()
    {
        root.controller.search(shell.searchFieldText + shell.searchFieldPreeditText)
    }

    function searchTerm(word)
    {
        shell.searchFieldText = word
        root.controller.search(word)
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
        root.controller.clear()
        shell.searchFieldText = ""
    }

    MobileShell
    {
        id: shell

        anchors.fill: parent

        activeTab: "dictionary"
        contentMaximumWidth: 632
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
            anchors.topMargin: AppMetrics.contentPadding
                + (formTranslation.visible && root.controller.results.length > 0 ? formTranslation.height + AppMetrics.gapSmall : 0)

            model: root.controller.results
            maxStage: root.store.stageCount

            showPartOfSpeech: false
            showAddAction: true
            showEditAction: false
            showDeleteAction: false

            onWordClicked: function(index)
            {
                const row = root.controller.results[index]
                root.entryOpened(row.word, "", shell.searchFieldText)
            }

            onVoicePlayRequested: function(index)
            {
                root.controller.speak(root.controller.results[index].word)
            }

            onAddRequested: function(index)
            {
                const row = root.controller.results[index]
                root.addWordRequested(row.word, row.translation.split(", ")[0] || "",
                                       row.partOfSpeech)
            }
        }

        MobileMachineTranslation
        {
            id: formTranslation

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppMetrics.contentPadding

            visible: AppMachineTranslation.enabled
                && (root.controller.results.length === 0 || root.controller.formTerm.length > 0)
                && (shell.searchFieldText + shell.searchFieldPreeditText).trim().length > 0
            lookup: root.controller
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
