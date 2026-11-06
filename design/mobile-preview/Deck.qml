import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property string name
    required property int colourIndex
    required property int total
    required property int notStarted
    required property int learning
    required property int completed

    signal activated()

    readonly property real totalSafe: Math.max(root.total, 1)
    readonly property string monogram: root.name.length > 0 ? root.name.substring(0, 2).toUpperCase() : "??"
    readonly property color headerColour: Theme.deckColours[root.colourIndex % Theme.deckColours.length]

    width: 200
    height: 143
    radius: Theme.radiusMedium
    color: Theme.panel
    border.color: Theme.line
    border.width: 1
    clip: true

    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 52
        color: root.headerColour
        topLeftRadius: Theme.radiusMedium
        topRightRadius: Theme.radiusMedium

        Text {
            anchors.centerIn: parent
            text: root.monogram
            font.pixelSize: 22
            font.weight: Font.Bold
            font.family: Theme.fontFamily
            color: "#ffffff"
        }
    }

    Item {
        id: body
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 8
            anchors.bottomMargin: 10
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 6

            Rectangle {
                id: progressTrack
                Layout.fillWidth: true
                Layout.leftMargin: -6
                Layout.rightMargin: -6
                height: 5
                radius: height / 2
                color: Theme.faint

                Rectangle {
                    width: progressTrack.width * (root.learning + root.completed) / root.totalSafe
                    height: parent.height
                    topLeftRadius: parent.radius
                    bottomLeftRadius: parent.radius
                    topRightRadius: width >= progressTrack.width ? parent.radius : 0
                    bottomRightRadius: width >= progressTrack.width ? parent.radius : 0
                    color: Theme.accent
                }

                Rectangle {
                    width: progressTrack.width * root.completed / root.totalSafe
                    height: parent.height
                    topLeftRadius: parent.radius
                    bottomLeftRadius: parent.radius
                    topRightRadius: width >= progressTrack.width ? parent.radius : 0
                    bottomRightRadius: width >= progressTrack.width ? parent.radius : 0
                    color: Theme.correct
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.name
                font.pixelSize: 14
                font.weight: Font.DemiBold
                font.family: Theme.fontFamily
                color: Theme.ink
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Item {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.preferredHeight: statsColumnLeft.implicitHeight

                ColumnLayout {
                    id: statsColumnLeft
                    anchors.centerIn: parent
                    spacing: 3

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.total
                        font.pixelSize: 17
                        font.weight: Font.Bold
                        font.family: Theme.fontFamily
                        color: Theme.ink
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "WORDS"
                        font.pixelSize: 9
                        font.weight: Font.DemiBold
                        font.family: Theme.fontFamily
                        font.letterSpacing: 0.5
                        color: Theme.mid
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 30
                Layout.alignment: Qt.AlignVCenter
                color: Theme.line
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.preferredHeight: statsColumnRight.implicitHeight

                ColumnLayout {
                    id: statsColumnRight
                    anchors.centerIn: parent
                    spacing: 3

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.learning
                        font.pixelSize: 17
                        font.weight: Font.Bold
                        font.family: Theme.fontFamily
                        color: Theme.accent
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "LEARNING"
                        font.pixelSize: 9
                        font.weight: Font.DemiBold
                        font.family: Theme.fontFamily
                        font.letterSpacing: 0.5
                        color: Theme.mid
                    }
                }
            }
        }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.activated()
    }
}
