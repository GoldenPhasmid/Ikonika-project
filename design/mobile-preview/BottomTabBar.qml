import QtQuick
import QtQuick.Layouts

Item {
    id: tabBar

    required property string activeTab

    readonly property var tabs: [
        { name: "Today", icon: "calendar" },
        { name: "Decks", icon: "folder" },
        { name: "Words", icon: "book" },
        { name: "Settings", icon: "settings" }
    ]

    Layout.fillWidth: true
    Layout.preferredHeight: 82

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.line
    }

    RowLayout {
        id: tabRow
        anchors.fill: parent
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 0

        Repeater {
            model: tabBar.tabs
            delegate: ColumnLayout {
                id: tabDelegate
                readonly property bool active: modelData.name === tabBar.activeTab

                Layout.preferredWidth: tabRow.width / tabBar.tabs.length
                Layout.fillWidth: false
                Layout.alignment: Qt.AlignVCenter
                spacing: 6

                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    Layout.alignment: Qt.AlignHCenter
                    radius: 10
                    color: tabDelegate.active ? Theme.ink : "transparent"
                    border.color: Theme.line
                    border.width: tabDelegate.active ? 0 : 1

                    Image {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        source: "icons/" + modelData.icon + ".png"
                        fillMode: Image.PreserveAspectFit
                    }
                }

                Text {
                    text: modelData.name.toUpperCase()
                    font.pixelSize: 10
                    font.weight: tabDelegate.active ? Font.DemiBold : Font.Medium
                    font.family: Theme.fontFamily
                    font.letterSpacing: 0.4
                    color: tabDelegate.active ? Theme.ink : Theme.mid
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }
    }
}
