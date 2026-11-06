import QtQuick
import QtQuick.Layouts

DesktopFormPage
{
    id: root

    required property var store

    property string chosenCode: ""
    property string chosenNativeCode: ""
    property bool reminderOn: true
    property bool launchOn: true

    signal continued()

    readonly property var languages: AppCatalog.languages

    readonly property int chosenIndex: root.languages.findIndex(Language => Language.code === root.chosenCode)

    readonly property var nativeLanguages: root.chosenIndex >= 0 ? (root.languages[root.chosenIndex].natives || []) : []

    readonly property int nativeIndex:
    {
        const found = root.nativeLanguages.findIndex(Language => Language.code === root.chosenNativeCode)

        return found >= 0 ? found : (root.nativeLanguages.length > 0 ? 0 : -1)
    }

    readonly property int totalMegabytes: root.chosenIndex < 0 || root.languages[root.chosenIndex].installed
        ? 0 : Math.round(root.languages[root.chosenIndex].bytes / (1024 * 1024))

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
        AppOnboarding.choose(root.chosenCode, root.nativeIndex >= 0 ? root.nativeLanguages[root.nativeIndex].code : "")
        AppReminder.enabled = root.reminderOn
        AppReminder.launchAtStartup = root.launchOn

        root.continued()
    }

    maximumWidth: AppMetrics.dialogWidth

    DesktopLabel
    {
        Layout.fillWidth: true
        text: qsTr("Let's set things up")
        font.pixelSize: AppMetrics.fontHeadline
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap

        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    DesktopSettingsGroup
    {
        title: qsTr("Languages")

        DesktopSettingsRow
        {
            text: qsTr("I am learning")
            description: root.languages.length > 0 ? ""
                : (AppCatalog.busy ? qsTr("Looking for languages")
                                   : (AppCatalog.lastError.length > 0 ? qsTr("Cannot reach the language list")
                                                                      : qsTr("Nothing published yet")))

            DesktopSelect
            {
                objectName: "onboarding_language"

                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                heading: qsTr("I am learning")
                options: root.languages.map(Language => Language.name)
                currentIndex: root.chosenIndex
                enabled: root.languages.length > 0
                text: root.chosenIndex >= 0 ? root.languages[root.chosenIndex].name : qsTr("Choose")

                onChosen: function(index)
                {
                    root.chosenCode = root.languages[index].code
                    root.chosenNativeCode = ""
                }
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("My language")

            DesktopSelect
            {
                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                heading: qsTr("My language")
                options: root.nativeLanguages.map(Language => Language.name)
                currentIndex: root.nativeIndex
                enabled: root.nativeLanguages.length > 1

                onChosen: function(index) { root.chosenNativeCode = root.nativeLanguages[index].code }
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("App language")
            lastRow: true

            DesktopSelect
            {
                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                heading: qsTr("App language")
                options: AppLocalization.languageNames
                currentIndex: AppLocalization.languageIndex

                onChosen: function(index) { AppLocalization.languageIndex = index }
            }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Every day")

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
        DesktopButton
        {
            objectName: "onboarding_continue"

            large: true
            tone: "primary"
            text: root.totalMegabytes > 0 ? qsTr("Continue · %1 MB").arg(root.totalMegabytes) : qsTr("Continue")
            trailingIconName: "arrow_right"
            enabled: root.chosenCode.length > 0

            onClicked: root.commit()
        }
    ]
}
