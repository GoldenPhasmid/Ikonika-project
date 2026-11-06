import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    id: root
    activeTab: "Words"
    headerTitle: "Dictionary"
    showBack: true
    trailingIcon: ""
    showSearch: false
    showSearchField: true
    showTabBar: true

    readonly property var entries: [
        { word: "le marché", translation: "market, deal", cefrLevel: "A2", stage: 3, completed: false },
        { word: "marcher", translation: "to walk", cefrLevel: "A1", stage: -1, completed: false },
        { word: "le marchand", translation: "merchant, trader", cefrLevel: "B1", stage: 0, completed: false },
        { word: "marchander", translation: "to haggle", cefrLevel: "B2", stage: -1, completed: false },
        { word: "la marchandise", translation: "merchandise, goods", cefrLevel: "B1", stage: 5, completed: true }
    ]

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentWidth: width
        contentHeight: cardList.implicitHeight
        clip: true

        WordCardList {
            id: cardList
            anchors.left: parent.left
            anchors.right: parent.right

            model: root.entries

            showAddAction: true
            showEditAction: false
            showDeleteAction: false
        }
    }

    FloatingActionButton {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
    }
}
