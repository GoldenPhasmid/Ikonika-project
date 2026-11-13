import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property var store

    property bool reminderOn: true

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

    MobileOnboardingPage
    {
        anchors.fill: parent

        title: qsTr("Every day")

        onContinued:
        {
            AppReminder.enabled = root.reminderOn
            AppOnboarding.next()
        }

        MobileSettingsGroup
        {
            title: ""

            MobileSettingsSlider
            {
                title: qsTr("Words per day")
                description: qsTr("%1 minutes per day, up to %2 with reviews")
                    .arg(root.goalWords)
                    .arg(root.goalWords * 3)

                value: root.goalIndex
                valueText: root.goalWords
                from: 0
                to: root.goalOptions.length - 1
                step: 1

                onEdited: function(newValue)
                {
                    const option = root.goalOptions[newValue]

                    root.store.trainingWordCount = option.perSession
                    root.store.sessionsPerDay = option.sessions
                }
            }

            MobileSettingsRow
            {
                title: qsTr("Daily reminder")
                description: qsTr("A nudge on days you have not studied yet")
                lastRow: !root.reminderOn

                MobileSettingsToggle
                {
                    checked: root.reminderOn

                    onToggled: root.reminderOn = !root.reminderOn
                }
            }

            MobileReminderTimeRow
            {
                visible: root.reminderOn
                lastRow: true
                timeSheet: timePicker
            }
        }
    }

    TimePickerSheet
    {
        id: timePicker

        anchors.fill: parent
        z: 70
    }
}
