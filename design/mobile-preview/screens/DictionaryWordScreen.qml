import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    activeTab: "Words"
    headerTitle: "marché"
    showBack: true
    trailingIcon: "⋮"
    showSearch: true

    readonly property var chips: [
        { label: "market", level: 4, selected: true },
        { label: "deal", level: 3, selected: false },
        { label: "marketplace", level: 2, selected: false },
        { label: "agreement", level: 1, selected: false },
        { label: "bargain", level: 1, selected: false }
    ]

    readonly property var meanings: [
        { def: "lieu public où l'on vend des marchandises, généralement en plein air ou dans un bâtiment couvert", subChips: "market, marketplace" },
        { def: "accord commercial, transaction entre deux parties", subChips: "deal, agreement, bargain" },
        { def: "ensemble des acheteurs et des vendeurs d'un bien", subChips: "" }
    ]

    readonly property string cefrLevel: "B1"

    readonly property var examples: [
        { fr: "Je vais au marché le samedi matin.", en: "I go to the market on Saturday mornings." },
        { fr: "Ils ont conclu un marché avantageux.", en: "They struck a good deal." }
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
            anchors.margins: 24
            anchors.topMargin: 15
            anchors.bottomMargin: 32
            spacing: 24

            Card {
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "marché"
                        font.pixelSize: Theme.fontSizeHeadword
                        font.weight: Font.DemiBold
                        font.family: Theme.fontFamily
                        color: Theme.ink
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        spacing: 8

                        ActionIconButton { icon: "🔊" }
                        ActionIconButton { icon: "➕" }
                        ActionIconButton { icon: "🌐" }
                    }
                }

                RowLayout {
                    spacing: 8

                    Text {
                        text: "noun"
                        font.pixelSize: 13
                        font.family: Theme.fontFamily
                        color: Theme.subtle
                    }

                    Text {
                        text: "/maʁ.ʃe/"
                        font.pixelSize: 13
                        font.family: Theme.fontFamily
                        color: Theme.mid
                    }

                    FrequencyBadge { bucket: "TOP 500" }
                    CefrBadge { level: cefrLevel }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                SectionLabel {
                    text: "TRANSLATIONS"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: chips
                        delegate: TranslationChip {}
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12

                Repeater {
                    model: meanings
                    delegate: Card {
                        required property var modelData
                        required property int index

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text {
                                text: (index + 1) + ""
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                font.family: Theme.fontFamily
                                color: Theme.mid
                                Layout.alignment: Qt.AlignTop
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    textFormat: Text.RichText
                                    text: "<b style='color:" + Theme.subtle + "'>m. </b>" + modelData.def
                                    font.pixelSize: 14
                                    font.family: Theme.fontFamily
                                    color: Theme.ink
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    visible: modelData.subChips !== ""
                                    text: modelData.subChips
                                    font.pixelSize: 12
                                    font.family: Theme.fontFamily
                                    color: Theme.mid
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                SectionLabel {
                    text: "FORMS"
                }

                Card {
                    Text {
                        textFormat: Text.RichText
                        text: "marchés <span style='color:" + Theme.mid + "'>(plural)</span>"
                        font.pixelSize: 14
                        font.family: Theme.fontFamily
                        color: Theme.ink
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                SectionLabel {
                    text: "EXAMPLES"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Repeater {
                        model: examples
                        delegate: Card {
                            required property var modelData

                            Text {
                                text: modelData.fr
                                font.pixelSize: 14
                                font.italic: true
                                font.family: Theme.fontFamily
                                color: Theme.ink
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                text: modelData.en
                                font.pixelSize: 13
                                font.family: Theme.fontFamily
                                color: Theme.subtle
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }

    FloatingActionButton {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
    }
}
