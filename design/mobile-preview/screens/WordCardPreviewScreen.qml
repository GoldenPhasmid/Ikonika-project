import QtQuick
import QtQuick.Layouts
import "../"

Rectangle {
    id: root

    color: Theme.panel

    property bool knobVoice: true
    property bool knobProgress: true
    property bool knobTranslation: true
    property bool knobCefr: true

    property bool knobAdd: false
    property bool knobEdit: true
    property bool knobDelete: true

    property int knobStage: 2
    property bool knobCompleted: false

    readonly property var cefrOptions: ["", "A1", "A2", "B1", "B2", "C1", "C2"]
    property int cefrIndex: 3

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight + 32
        clip: true

        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            anchors.topMargin: 24
            spacing: 20

            Text {
                text: "Word card"
                font.pixelSize: 20
                font.weight: Font.DemiBold
                font.family: Theme.fontFamily
                color: Theme.ink
            }

            WordCard {
                id: demoCard
                Layout.fillWidth: true

                word: "le marché"
                translation: "market"
                cefrLevel: root.cefrOptions[root.cefrIndex]

                stage: root.knobStage
                completed: root.knobCompleted

                showVoice: root.knobVoice
                showProgress: root.knobProgress
                showTranslation: root.knobTranslation
                showCefr: root.knobCefr

                showAddAction: root.knobAdd
                showEditAction: root.knobEdit
                showDeleteAction: root.knobDelete
            }

            Text {
                Layout.fillWidth: true
                text: "Drag the card left to reveal actions. Hold delete to confirm."
                font.pixelSize: 12
                font.family: Theme.fontFamily
                color: Theme.mid
                wrapMode: Text.WordWrap
            }

            SettingsGroup {
                title: "ELEMENTS"

                SettingsRow {
                    title: "Voice button"
                    SettingsToggle {
                        checked: root.knobVoice
                        onToggled: root.knobVoice = !root.knobVoice
                    }
                }

                SettingsRow {
                    title: "Progress ring"
                    SettingsToggle {
                        checked: root.knobProgress
                        onToggled: root.knobProgress = !root.knobProgress
                    }
                }

                SettingsRow {
                    title: "Translation"
                    SettingsToggle {
                        checked: root.knobTranslation
                        onToggled: root.knobTranslation = !root.knobTranslation
                    }
                }

                SettingsRow {
                    title: "CEFR badge"
                    lastRow: true
                    SettingsToggle {
                        checked: root.knobCefr
                        onToggled: root.knobCefr = !root.knobCefr
                    }
                }
            }

            SettingsGroup {
                title: "REVEALED ACTIONS"

                SettingsRow {
                    title: "Add"
                    SettingsToggle {
                        checked: root.knobAdd
                        onToggled: root.knobAdd = !root.knobAdd
                    }
                }

                SettingsRow {
                    title: "Edit"
                    SettingsToggle {
                        checked: root.knobEdit
                        onToggled: root.knobEdit = !root.knobEdit
                    }
                }

                SettingsRow {
                    title: "Delete (hold to confirm)"
                    lastRow: true
                    SettingsToggle {
                        checked: root.knobDelete
                        onToggled: root.knobDelete = !root.knobDelete
                    }
                }
            }

            SettingsGroup {
                title: "LEARNING STATE"

                SettingsRow {
                    title: "Stage"
                    description: root.knobCompleted ? "Ignored while completed" : ("Box " + root.knobStage)
                    SettingsStepper {
                        value: root.knobStage
                        from: -1
                        to: 5
                        onEdited: function(newValue) { root.knobStage = newValue }
                    }
                }

                SettingsRow {
                    title: "Completed"
                    SettingsToggle {
                        checked: root.knobCompleted
                        onToggled: root.knobCompleted = !root.knobCompleted
                    }
                }

                SettingsRow {
                    title: "CEFR level"
                    lastRow: true
                    SettingsSelect {
                        label: root.cefrOptions[root.cefrIndex].length > 0
                            ? root.cefrOptions[root.cefrIndex] : "None"
                        onClicked: root.cefrIndex = (root.cefrIndex + 1) % root.cefrOptions.length
                    }
                }
            }
        }
    }
}
