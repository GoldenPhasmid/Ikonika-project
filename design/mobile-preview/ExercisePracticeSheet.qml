import QtQuick
import QtQuick.Layouts

BottomSheet {
    id: root

    property string exerciseIcon: ""
    property string exerciseLabel: ""
    property string exerciseSubtitle: ""
    property int wordCount: 0

    property bool reverse: false
    property int count: root.wordCount

    heading: root.exerciseLabel

    signal startRequested(bool reverse, int count)

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            radius: Theme.radiusMedium
            color: Theme.faint

            Text {
                anchors.centerIn: parent
                text: root.exerciseIcon
                font.pixelSize: 18
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.exerciseSubtitle
            font.pixelSize: 13
            font.family: Theme.fontFamily
            color: Theme.mid
            wrapMode: Text.WordWrap
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: optionRows.implicitHeight

        radius: Theme.radiusMedium
        color: Theme.panel
        border.color: Theme.line
        border.width: 1
        clip: true

        ColumnLayout {
            id: optionRows

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0

            SettingsRow {
                title: "Direction"
                description: root.reverse ? "Translation → target word" : "Target word → translation"

                SettingsToggle {
                    checked: root.reverse
                    onToggled: root.reverse = !root.reverse
                }
            }

            SettingsRow {
                title: "Word count"
                lastRow: true

                SettingsStepper {
                    value: root.count
                    from: 5
                    to: Math.max(root.wordCount, 5)
                    step: 5

                    onEdited: function(newValue) { root.count = newValue }
                }
            }
        }
    }

    WideButton {
        label: "Start · " + root.count + " words"

        onClicked: root.startRequested(root.reverse, root.count)
    }
}
