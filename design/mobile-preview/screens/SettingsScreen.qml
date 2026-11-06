import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    id: root

    activeTab: "Settings"
    headerTitle: "Settings"
    trailingIcon: ""
    showSearch: true
    showTabBar: true

    property bool speechOn: true
    property bool remindersOn: false
    property int dailyGoal: 25
    property int sessionLength: 10

    Flickable {
        anchors.fill: parent
        contentHeight: content.implicitHeight + 48
        clip: true

        ColumnLayout {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            anchors.topMargin: 20

            spacing: 24

            SettingsGroup {
                title: "GENERAL"

                SettingsRow {
                    title: "Theme"
                    description: "Light, dark, or follow the system"

                    SettingsSelect { label: "Light" }
                }

                SettingsRow {
                    title: "Learning"

                    SettingsSelect { label: "French" }
                }

                SettingsRow {
                    title: "Account"
                    description: "alex@example.com"
                    lastRow: true

                    SettingsChevron {}
                }
            }

            SettingsGroup {
                title: "ROUTINE"

                SettingsRow {
                    title: "Words a day"
                    description: "What Today counts against"

                    SettingsStepper {
                        value: root.dailyGoal
                        from: 5
                        to: 100
                        step: 5
                        onEdited: function(v) { root.dailyGoal = v }
                    }
                }

                SettingsRow {
                    title: "Session length"

                    SettingsStepper {
                        value: root.sessionLength
                        from: 5
                        to: 50
                        step: 5
                        suffix: "cards"
                        onEdited: function(v) { root.sessionLength = v }
                    }
                }

                SettingsRow {
                    title: "Daily reminder"
                    description: "A notification if you have not studied by evening"
                    lastRow: true

                    Checkbox {
                        checked: root.remindersOn
                        onToggled: root.remindersOn = !root.remindersOn
                    }
                }
            }

            SettingsGroup {
                title: "SOUND"

                SettingsRow {
                    title: "Speech"
                    description: "Read words aloud during a session"
                    lastRow: true

                    Checkbox {
                        checked: root.speechOn
                        onToggled: root.speechOn = !root.speechOn
                    }
                }
            }

            SettingsGroup {
                title: "DATA"

                SettingsRow {
                    title: "Sync now"
                    description: "Last synced 4 minutes ago"

                    SettingsValue { text: "12 decks" }

                    SettingsChevron {}
                }

                SettingsRow {
                    title: "Import a deck"

                    SettingsChevron {}
                }

                SettingsRow {
                    title: "Erase everything"
                    description: "Removes every deck and card from this phone"
                    destructive: true
                    lastRow: true

                    SettingsChevron {}
                }
            }

            SettingsGroup {
                title: "ABOUT"

                SettingsRow {
                    title: "Version"
                    lastRow: true

                    SettingsValue { text: "0.9.2" }
                }
            }
        }
    }
}
