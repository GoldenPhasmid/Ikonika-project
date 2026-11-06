import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property var store

    property string chosenCode: ""
    property string chosenNativeCode: ""
    property bool reminderOn: true

    signal continued()

    readonly property var languages: AppCatalog.languages

    readonly property int chosenIndex:
    {
        for (let i = 0; i < root.languages.length; i++)
        {
            if (root.languages[i].code === root.chosenCode)
            {
                return i
            }
        }

        return -1
    }

    readonly property var nativeLanguages:
        root.chosenIndex >= 0 ? (root.languages[root.chosenIndex].natives || []) : []

    readonly property int nativeIndex:
    {
        for (let i = 0; i < root.nativeLanguages.length; i++)
        {
            if (root.nativeLanguages[i].code === root.chosenNativeCode)
            {
                return i
            }
        }

        return root.nativeLanguages.length > 0 ? 0 : -1
    }

    readonly property int totalMegabytes:
    {
        if (root.chosenIndex < 0 || root.languages[root.chosenIndex].installed)
        {
            return 0
        }

        return Math.round(root.languages[root.chosenIndex].bytes / (1024 * 1024))
    }

    readonly property var goalOptions: [
        { words: 10, perSession: 10, sessions: 1 },
        { words: 15, perSession: 15, sessions: 1 },
        { words: 20, perSession: 20, sessions: 1 },
        { words: 25, perSession: 25, sessions: 1 },
        { words: 30, perSession: 15, sessions: 2 },
        { words: 40, perSession: 20, sessions: 2 },
        { words: 50, perSession: 25, sessions: 2 }
    ]

    readonly property int goalIndex:
    {
        for (let i = 0; i < root.goalOptions.length; i++)
        {
            const option = root.goalOptions[i]

            if (option.perSession === root.store.trainingWordCount
                && option.sessions === root.store.sessionsPerDay)
            {
                return i
            }
        }

        return 3
    }

    readonly property int goalWords: root.goalOptions[root.goalIndex].words

    function commit()
    {
        AppOnboarding.choose(root.chosenCode, root.nativeIndex >= 0
            ? root.nativeLanguages[root.nativeIndex].code : "")

        AppReminder.enabled = root.reminderOn

        root.continued()
    }

    color: AppTheme.colourPanel

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        MobileHeader {}

        Flickable
        {
            Layout.fillWidth: true
            Layout.fillHeight: true

            contentWidth: width
            contentHeight: content.implicitHeight + AppMetrics.gapLarge * 2
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout
            {
                id: content

                x: AppMetrics.gapLarge
                y: AppMetrics.gapLarge
                width: parent.width - AppMetrics.gapLarge * 2
                spacing: AppMetrics.gapLarge

                Text
                {
                    Layout.fillWidth: true

                    text: qsTr("Let's set things up")
                    font.pixelSize: AppMetrics.fontHeadline
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourText
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                MobilePicker
                {
                    label: qsTr("I AM LEARNING")
                    title: qsTr("I am learning")
                    sheet: optionSheet

                    model: root.languages
                    textRole: "name"
                    currentIndex: root.chosenIndex

                    onPicked: function(index)
                    {
                        root.chosenCode = root.languages[index].code
                        root.chosenNativeCode = ""
                    }
                }

                Text
                {
                    Layout.fillWidth: true

                    visible: root.languages.length === 0
                    text: AppCatalog.busy
                        ? qsTr("Looking for languages...")
                        : (AppCatalog.lastError.length > 0
                            ? qsTr("Cannot reach the language list")
                            : qsTr("Nothing published yet"))
                    font.pixelSize: AppMetrics.fontSmall
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourMuted
                    horizontalAlignment: Text.AlignHCenter
                }

                MobilePicker
                {
                    label: qsTr("MY LANGUAGE")
                    title: qsTr("My language")
                    sheet: optionSheet

                    enabled: root.nativeLanguages.length > 1
                    model: root.nativeLanguages
                    textRole: "name"
                    currentIndex: root.nativeIndex

                    onPicked: function(index)
                    {
                        root.chosenNativeCode = root.nativeLanguages[index].code
                    }
                }

                MobilePicker
                {
                    label: qsTr("APP LANGUAGE")
                    title: qsTr("App language")
                    sheet: optionSheet

                    model: AppLocalization.languageNames
                    currentIndex: AppLocalization.languageIndex

                    onPicked: function(index) { AppLocalization.languageIndex = index }
                }

                MobileSettingsGroup
                {
                    title: qsTr("EVERY DAY")

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
                        lastRow: true

                        MobileSettingsToggle
                        {
                            checked: root.reminderOn

                            onToggled: root.reminderOn = !root.reminderOn
                        }
                    }
                }
            }
        }

        MobileWideButton
        {
            Layout.fillWidth: true
            Layout.margins: AppMetrics.gapLarge

            label: root.totalMegabytes > 0
                ? qsTr("Continue · %1 MB").arg(root.totalMegabytes)
                : qsTr("Continue")

            interactive: root.chosenCode.length > 0

            onClicked: root.commit()
        }
    }

    MobileSettingsPicker
    {
        id: optionSheet

        anchors.fill: parent
        z: 50
    }
}
