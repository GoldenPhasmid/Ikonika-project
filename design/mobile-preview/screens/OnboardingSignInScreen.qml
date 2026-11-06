import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    headerTitle: "Ikonika"
    showSearch: false
    showTabBar: false

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item { Layout.fillHeight: true }

        Text {
            text: "Ikonika"
            font.pixelSize: 56
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: Theme.ink
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 62
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 66
            Layout.rightMargin: 66
            spacing: 12

            WideButton {
                label: "Sign in with Google"
            }

            WideButton {
                label: "Continue signed out"
                filled: false
            }
        }

        Item { Layout.fillHeight: true }
    }
}
