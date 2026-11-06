import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    activeTab: "Today"
    showSearch: true
    showTabBar: true

    readonly property var decks: [
        { name: "Mercado y comida", colourIndex: 0, total: 120, notStarted: 40, learning: 32, completed: 48 },
        { name: "Transporte", colourIndex: 1, total: 96, notStarted: 40, learning: 30, completed: 26 },
        { name: "Verbos irregulares", colourIndex: 2, total: 64, notStarted: 4, learning: 8, completed: 52 }
    ]

    readonly property var reviewTimeline: [
        { label: "Today", count: 12 },
        { label: "Tomorrow", count: 8 },
        { label: "Wed", count: 15 },
        { label: "Thu", count: 3 },
        { label: "Fri", count: 0 }
    ]

    Flickable {
        anchors.fill: parent
        contentHeight: content.implicitHeight
        clip: true

        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            anchors.topMargin: 16
            anchors.bottomMargin: 24
            spacing: 24

            ColumnLayout {
                id: progressSection
                Layout.fillWidth: true
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: "38"
                        font.pixelSize: 40
                        font.weight: Font.Medium
                        font.family: Theme.fontFamily
                        color: Theme.ink
                    }

                    Text {
                        text: "of 92 words today"
                        font.pixelSize: 15
                        font.family: Theme.fontFamily
                        color: Theme.mid
                        Layout.alignment: Qt.AlignBottom
                        Layout.bottomMargin: 8
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: 3
                    color: Theme.faint

                    Rectangle {
                        width: parent.width * 0.41
                        height: parent.height
                        radius: 3
                        color: Theme.ink
                    }
                }
            }

            ColumnLayout {
                id: actionsSection
                Layout.fillWidth: true
                spacing: 8

                ActionButton {
                    icon: "🎓"
                    label: "Learn"
                    trailing: "48"
                }

                ActionButton {
                    icon: "📋"
                    label: "Review"
                    trailing: "61"
                }
            }

            ColumnLayout {
                id: recentDecksSection
                Layout.fillWidth: true
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true

                    SectionLabel {
                        text: "RECENT DECKS"
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "See all"
                        font.pixelSize: 13
                        font.family: Theme.fontFamily
                        color: Theme.mid
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    orientation: ListView.Horizontal
                    spacing: 12
                    clip: true
                    model: decks
                    delegate: Deck {}
                }
            }

            ColumnLayout {
                id: summarySection
                Layout.fillWidth: true
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 49

                    Text {
                        text: "Recently learned"
                        font.pixelSize: 15
                        font.family: Theme.fontFamily
                        color: Theme.ink
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "11"
                        font.pixelSize: 15
                        font.family: Theme.fontFamily
                        color: Theme.mid
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.faint
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 49

                    Text {
                        text: "Search history"
                        font.pixelSize: 15
                        font.family: Theme.fontFamily
                        color: Theme.ink
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "9"
                        font.pixelSize: 15
                        font.family: Theme.fontFamily
                        color: Theme.mid
                    }
                }
            }

            ColumnLayout {
                id: timelineSection
                Layout.fillWidth: true
                spacing: 8

                SectionLabel {
                    text: "COMING UP"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: reviewTimeline

                        delegate: Rectangle {
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredHeight: 64
                            radius: Theme.radiusMedium
                            color: Theme.panel
                            border.color: Theme.line
                            border.width: 1

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2

                                Text {
                                    text: modelData.count
                                    font.pixelSize: 17
                                    font.weight: Font.Bold
                                    font.family: Theme.fontFamily
                                    color: modelData.count > 0 ? Theme.ink : Theme.mid
                                    Layout.alignment: Qt.AlignHCenter
                                }

                                Text {
                                    text: modelData.label
                                    font.pixelSize: 11
                                    font.family: Theme.fontFamily
                                    color: Theme.mid
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
