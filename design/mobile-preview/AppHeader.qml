import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string title: "Ikonika"
    property bool showBack: false
    property string trailingIcon: ""
    property bool showSearch: false
    property bool showSearchField: false
    property bool showMute: false
    property bool showOptions: false

    property alias searchFieldText: searchField.text

    signal backRequested()
    signal trailingActivated()
    signal searchRequested()
    signal searchFieldAccepted()
    signal muteRequested()
    signal optionsRequested()

    Layout.fillWidth: true
    spacing: 0

    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: 56
        Layout.leftMargin: 16
        Layout.rightMargin: 16

        Text {
            text: "‹"
            font.pixelSize: 22
            font.family: Theme.fontFamily
            color: Theme.ink
            visible: root.showBack
            Layout.preferredWidth: visible ? 44 : 0
            horizontalAlignment: Text.AlignHCenter

            TapHandler { onTapped: root.backRequested() }
        }

        Text {
            text: root.title
            font.pixelSize: 17
            font.weight: Font.DemiBold
            font.family: Theme.fontFamily
            color: Theme.ink
            visible: !root.showSearchField
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        HeaderSearchField {
            id: searchField
            visible: root.showSearchField
            Layout.fillWidth: true

            onAccepted: root.searchFieldAccepted()
        }

        Text {
            text: "🔇"
            font.pixelSize: 16
            color: Theme.ink
            visible: root.showMute
            Layout.preferredWidth: visible ? 40 : 0
            horizontalAlignment: Text.AlignHCenter

            TapHandler { onTapped: root.muteRequested() }
        }

        Text {
            text: "⋮"
            font.pixelSize: 18
            color: Theme.ink
            visible: root.showOptions
            Layout.preferredWidth: visible ? 40 : 0
            horizontalAlignment: Text.AlignHCenter

            TapHandler { onTapped: root.optionsRequested() }
        }

        Text {
            text: "🔍"
            font.pixelSize: 16
            color: Theme.ink
            visible: root.showSearch
            Layout.preferredWidth: visible ? 44 : 0
            horizontalAlignment: Text.AlignHCenter

            TapHandler { onTapped: root.searchRequested() }
        }

        Text {
            text: root.trailingIcon
            font.pixelSize: 18
            color: Theme.ink
            visible: root.trailingIcon !== ""
            Layout.preferredWidth: visible ? 44 : 0
            horizontalAlignment: Text.AlignHCenter

            TapHandler { onTapped: root.trailingActivated() }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Theme.line
    }
}
