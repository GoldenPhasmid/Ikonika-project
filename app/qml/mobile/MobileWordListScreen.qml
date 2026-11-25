import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property var store
    required property WordListController controller

    property string source: "history"

    signal done()
    signal wordOpened(string headword, string partOfSpeech)
    signal tabSelected(string key)

    function open(newSource)
    {
        root.source = newSource
        root.controller.load(newSource)
    }

    function refresh()
    {
        root.controller.load(root.source)
    }

    readonly property bool dueSource: root.source.indexOf("due:") === 0
    readonly property int dueDaysFromToday: root.dueSource ? parseInt(root.source.substring(4)) : 0

    readonly property string headerTitle:
    {
        if (root.dueSource)
        {
            if (root.dueDaysFromToday <= 0)
            {
                return qsTr("Due Today")
            }

            const date = new Date()
            date.setDate(date.getDate() + root.dueDaysFromToday)

            return qsTr("Due %1").arg(Qt.locale(AppLocalization.localeName).toString(date, "ddd, MMM d"))
        }

        return root.source === "studied" ? qsTr("Recently Studied") : qsTr("Search History")
    }

    readonly property var filteredEntries:
    {
        const term = (shell.searchFieldText + shell.searchFieldPreeditText).trim().toLowerCase()

        if (term.length === 0)
        {
            return root.controller.entries
        }

        return root.controller.entries.filter(function(entry)
        {
            return entry.word.toLowerCase().indexOf(term) >= 0
                || entry.translation.toLowerCase().indexOf(term) >= 0
        })
    }

    MobileShell
    {
        id: shell

        anchors.fill: parent

        activeTab: "today"
        headerTitle: root.headerTitle
        showBack: true
        showSearchField: true

        onBackRequested: root.done()
        onTabSelected: function(key) { root.tabSelected(key) }

        MobileWordCardList
        {
            id: entryList

            anchors.fill: parent
            anchors.margins: AppMetrics.contentPadding

            model: root.filteredEntries
            maxStage: root.store.stageCount

            showAddAction: false
            showEditAction: false
            showDeleteAction: false

            onWordClicked: function(index)
            {
                const entry = root.filteredEntries[index]
                root.wordOpened(entry.targetWord, entry.partOfSpeech)
            }

            onVoicePlayRequested: function(index)
            {
                root.controller.speakWord(root.filteredEntries[index].word)
            }
        }
    }
}
