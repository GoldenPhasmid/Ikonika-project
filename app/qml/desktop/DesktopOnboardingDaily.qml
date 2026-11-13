import QtQuick
import QtQuick.Layouts

DesktopFormPage
{
    id: root

    required property var store

    property bool reminderOn: true
    property bool launchOn: true

    readonly property var goalOptions: [
        { words: 10, perSession: 10, sessions: 1 },
        { words: 15, perSession: 15, sessions: 1 },
        { words: 20, perSession: 20, sessions: 1 },
        { words: 25, perSession: 25, sessions: 1 },
        { words: 30, perSession: 15, sessions: 2 },
        { words: 40, perSession: 20, sessions: 2 },
        { words: 50, perSession: 25, sessions: 2 }
    ]

    readonly property int defaultGoalIndex: 3

    readonly property int goalIndex:
    {
        const found = root.goalOptions.findIndex(Option => Option.perSession === root.store.trainingWordCount
                                                           && Option.sessions === root.store.sessionsPerDay)

        return found >= 0 ? found : root.defaultGoalIndex
    }

    readonly property int goalWords: root.goalOptions[root.goalIndex].words

    function commit()
    {
        AppReminder.enabled = root.reminderOn
        AppReminder.launchAtStartup = root.launchOn

        AppOnboarding.next()
    }

    maximumWidth: AppMetrics.dialogWidth

    DesktopLabel
    {
        Layout.fillWidth: true
        text: qsTr("Every day")
        font.pixelSize: AppMetrics.fontHeadline
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap

        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    DesktopSettingsGroup
    {
        DesktopSettingsSlider
        {
            title: qsTr("Words per day")
            description: qsTr("%1 minutes a day, up to %2 with reviews").arg(root.goalWords).arg(root.goalWords * 3)
            value: root.goalIndex
            valueText: root.goalWords.toString()
            from: 0
            to: root.goalOptions.length - 1

            onEdited: function(newValue)
            {
                const option = root.goalOptions[newValue]

                root.store.trainingWordCount = option.perSession
                root.store.sessionsPerDay = option.sessions
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Daily reminder")

            DesktopToggle
            {
                accessibleName: qsTr("Daily reminder")
                on: root.reminderOn

                onClicked: root.reminderOn = !root.reminderOn
            }
        }

        DesktopReminderTimeRow
        {
            visible: root.reminderOn
        }

        DesktopSettingsRow
        {
            text: qsTr("Launch at startup")
            lastRow: true

            DesktopToggle
            {
                accessibleName: qsTr("Launch at startup")
                on: root.launchOn

                onClicked: root.launchOn = !root.launchOn
            }
        }
    }

    footer: [
        DesktopFormButton
        {
            objectName: "onboarding_back"

            text: qsTr("Back")
            keyHint: qsTr("Backspace")
            shortcutKeys: ["Backspace"]

            onClicked: AppOnboarding.back()
        },
        DesktopFormButton
        {
            objectName: "onboarding_continue"

            tone: "primary"
            text: qsTr("Continue")
            keyHint: qsTr("Enter")
            shortcutKeys: ["Return", "Enter"]

            onClicked: root.commit()
        }
    ]
}
