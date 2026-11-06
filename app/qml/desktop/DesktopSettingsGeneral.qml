import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    DesktopSettingsGroup
    {
        title: qsTr("General")

        DesktopSettingsRow
        {
            text: qsTr("Theme")

            DesktopSelect
            {
                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                heading: qsTr("Theme")
                options: AppTheme.themeNames
                currentIndex: AppTheme.themeIndex

                onChosen: function(index) { AppTheme.themeIndex = index }
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Launch at startup")
            lastRow: true

            DesktopToggle
            {
                objectName: "settings_launch_at_startup"

                accessibleName: qsTr("Launch at startup")
                on: AppReminder.launchAtStartup

                onClicked: AppReminder.launchAtStartup = !AppReminder.launchAtStartup
            }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Languages")

        DesktopSettingsRow
        {
            text: qsTr("Interface language")

            DesktopSelect
            {
                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                heading: qsTr("Interface language")
                options: AppLocalization.languageNames
                currentIndex: AppLocalization.languageIndex

                onChosen: function(index) { AppLocalization.languageIndex = index }
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Language you learn")
            description: qsTr("%1 words in the dictionary").arg(Number(root.store.dictionaryWordCount).toLocaleString(Qt.locale(), "f", 0))

            DesktopDropdownButton
            {
                id: languageButton

                objectName: "settings_target_language"

                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                popover: languagePopover
                text: root.store.targetLanguageName

                Accessible.name: qsTr("Language you learn: %1").arg(text)

                DesktopLanguagePopover
                {
                    id: languagePopover

                    store: root.store
                    y: languageButton.height + AppMetrics.gapTight
                    width: languageButton.width
                }
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Your language")
            lastRow: true

            DesktopSelect
            {
                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                heading: qsTr("Your language")
                options: root.store.nativeLanguageNames
                currentIndex: root.store.nativeLanguageIndex
                enabled: root.store.nativeLanguageNames.length > 1

                onChosen: function(index) { root.store.nativeLanguageIndex = index }
            }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Exercises")

        DesktopSettingsRow
        {
            text: qsTr("Show articles")
            description: qsTr("A hidden article is still checked in typed answers when the article is required")

            DesktopToggle
            {
                accessibleName: qsTr("Show articles")
                on: root.store.showArticle

                onClicked: root.store.showArticle = !root.store.showArticle
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Speak articles")

            DesktopToggle
            {
                accessibleName: qsTr("Speak articles")
                on: root.store.speakArticle
                enabled: root.store.audioAvailable

                onClicked: root.store.speakArticle = !root.store.speakArticle
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Require the article")

            DesktopToggle
            {
                accessibleName: qsTr("Require the article")
                on: !root.store.allowMissingArticle

                onClicked: root.store.allowMissingArticle = !root.store.allowMissingArticle
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Learn %1 forms").arg(root.store.optionalPronoun)
            description: qsTr("Turn off to leave %1 out of conjugation trainings").arg(root.store.optionalPronoun)
            visible: root.store.optionalPronoun.length > 0

            DesktopToggle
            {
                accessibleName: qsTr("Learn %1 forms").arg(root.store.optionalPronoun)
                on: root.store.learnOptionalPronoun

                onClicked: root.store.learnOptionalPronoun = !root.store.learnOptionalPronoun
            }
        }

        DesktopSettingsSlider
        {
            title: qsTr("Cards per exercise")
            value: root.store.cardsPerExercise
            from: 5
            to: 20
            step: 5
            lastRow: true

            onEdited: function(newValue) { root.store.cardsPerExercise = newValue }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Dictionary")

        DesktopSettingsRow
        {
            text: qsTr("Search the web with")

            DesktopSelect
            {
                objectName: "settings_web_search_source"

                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                heading: qsTr("Search the web with")
                options: root.store.webSearchSourceNames
                currentIndex: root.store.webSearchSourceIndex
                enabled: root.store.webSearchSourceNames.length > 1

                onChosen: function(index) { root.store.webSearchSourceIndex = index }
            }
        }

        DesktopSettingsSlider
        {
            title: qsTr("Examples per meaning")
            value: root.store.examplesPerSense
            from: 0
            to: 5
            lastRow: true

            onEdited: function(newValue) { root.store.examplesPerSense = newValue }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Appearance")

        DesktopSettingsRow
        {
            text: qsTr("Colour nouns by gender")

            DesktopToggle
            {
                objectName: "settings_colour_by_gender"

                accessibleName: qsTr("Colour nouns by gender")
                on: AppTheme.colourByGender

                onClicked: AppTheme.colourByGender = !AppTheme.colourByGender
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Colour verbs by conjugation group")

            DesktopToggle
            {
                objectName: "settings_colour_by_verb_group"

                accessibleName: qsTr("Colour verbs by conjugation group")
                on: AppTheme.colourByVerbGroup

                onClicked: AppTheme.colourByVerbGroup = !AppTheme.colourByVerbGroup
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Animated transitions")
            lastRow: true

            DesktopToggle
            {
                accessibleName: qsTr("Animated transitions")
                on: AppMetrics.transitionsEnabled

                onClicked: AppMetrics.transitionsEnabled = !AppMetrics.transitionsEnabled
            }
        }
    }
}
