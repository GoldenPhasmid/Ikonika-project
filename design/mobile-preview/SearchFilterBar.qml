import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root

    property alias searchText: searchField.text
    property string searchPlaceholder: "Search..."

    property string filterLabel: "All"
    property string sortLabel: ""

    property string counterText: ""

    signal searchAccepted()
    signal filterRequested()
    signal sortRequested()

    Layout.fillWidth: true
    spacing: 16

    SearchField {
        id: searchField
        placeholder: root.searchPlaceholder

        onAccepted: root.searchAccepted()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        SettingsSelect {
            label: root.filterLabel

            onClicked: root.filterRequested()
        }

        SettingsSelect {
            label: root.sortLabel

            onClicked: root.sortRequested()
        }

        Item { Layout.fillWidth: true }

        Text {
            visible: root.counterText.length > 0
            text: root.counterText
            font.pixelSize: 13
            font.family: Theme.fontFamily
            color: Theme.mid
        }
    }
}
