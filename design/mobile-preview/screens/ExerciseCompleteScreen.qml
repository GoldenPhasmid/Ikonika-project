import QtQuick
import QtQuick.Layouts
import "../"

ExerciseShell {
    title: "Match Pair"
    showMute: false
    showOptions: false
    showProgress: false

    property string headline: "Training complete"

    readonly property var stats: [
        { value: "25", label: "words trained" },
        { value: "3:07", label: "elapsed" }
    ]

    ContentArea {
        posVisible: false

        primaryContent: Text {
            text: headline
            font.pixelSize: 28
            font.weight: Font.Medium
            font.family: Theme.fontFamily
            color: Theme.ink
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        secondaryContent: GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 16
            rowSpacing: 16

            Repeater {
                model: stats

                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 81
                    radius: Theme.radiusMedium
                    color: "#f9f9f9"
                    border.color: Theme.line
                    border.width: 1

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 4

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.value
                            font.pixelSize: 24
                            font.weight: Font.DemiBold
                            font.family: Theme.fontFamily
                            color: Theme.ink
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.label
                            font.pixelSize: 12
                            font.family: Theme.fontFamily
                            color: Theme.mid
                        }
                    }
                }
            }
        }
    }

    actions: WideButton {
        label: "Done"
    }
}
