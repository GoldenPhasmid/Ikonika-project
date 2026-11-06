import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store
    required property var optionPicker
    required property var languageSheet
    required property var updateSheet

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    readonly property var dictionaryUpdate: AppCatalog.availableDictionaryUpdate

    Rectangle
    {
        Layout.fillWidth: true
        implicitHeight: updateColumn.implicitHeight + (AppMetrics.contentPadding * 2)

        visible: Object.keys(root.dictionaryUpdate).length > 0

        radius: AppMetrics.radiusMedium
        color: AppTheme.colourPanel
        border.color: AppTheme.colourBorder
        border.width: AppMetrics.borderWidth

        ColumnLayout
        {
            id: updateColumn

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppMetrics.contentPadding

            spacing: AppMetrics.gapSmall

            Text
            {
                Layout.fillWidth: true

                text: qsTr("New %1 dictionary").arg(root.dictionaryUpdate.name !== undefined
                    ? root.dictionaryUpdate.name
                    : "")
                font.pixelSize: AppMetrics.fontLabel
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
            }

            Text
            {
                Layout.fillWidth: true

                text: qsTr("%1 MB. Your cards and progress are not affected.").arg(
                    Math.round((root.dictionaryUpdate.bytes !== undefined
                        ? root.dictionaryUpdate.bytes
                        : 0) / (1024 * 1024)))
                font.pixelSize: AppMetrics.fontMeta
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourSubtle
                wrapMode: Text.WordWrap
            }

            MobileWideButton
            {
                Layout.topMargin: AppMetrics.gapSmall

                label: qsTr("Update")

                onClicked: root.updateSheet.offer()
            }
        }
    }

    MobileSettingsGroup
    {
        title: qsTr("GENERAL")

        MobileSettingsRow
        {
            title: qsTr("Theme")

            MobileSettingsSelect
            {
                label: AppTheme.themeNames[AppTheme.themeIndex]

                onClicked: root.optionPicker.open(qsTr("Theme"), AppTheme.themeNames,
                                                  AppTheme.themeIndex,
                                                  function(index) { AppTheme.themeIndex = index })
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Interface language")

            MobileSettingsSelect
            {
                label: AppLocalization.languageNames[AppLocalization.languageIndex]

                onClicked: root.optionPicker.open(qsTr("Interface language"),
                                                  AppLocalization.languageNames,
                                                  AppLocalization.languageIndex,
                                                  function(index)
                                                  {
                                                      AppLocalization.languageIndex = index
                                                  })
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Foreign language")
            description: qsTr("%n word(s) available", "", root.store.dictionaryWordCount)

            MobileSettingsSelect
            {
                label: root.store.targetLanguageName

                onClicked: root.languageSheet.open()
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Native language")
            lastRow: true

            MobileSettingsSelect
            {
                label: root.store.nativeLanguageName
                interactive: root.store.nativeLanguageNames.length > 1

                onClicked: root.optionPicker.open(qsTr("Native language"),
                                                  root.store.nativeLanguageNames,
                                                  root.store.nativeLanguageIndex,
                                                  function(index)
                                                  {
                                                      root.store.nativeLanguageIndex = index
                                                  })
            }
        }
    }

    MobileSettingsGroup
    {
        title: qsTr("EXERCISES")

        MobileSettingsRow
        {
            title: qsTr("Show articles")
            description: qsTr("A hidden article is still checked in typed answers when the article is required")

            MobileSettingsToggle
            {
                checked: root.store.showArticle

                onToggled: root.store.showArticle = !root.store.showArticle
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Speak articles")
            description: qsTr("Turn off to voice only the word, without its article")

            MobileSettingsToggle
            {
                checked: root.store.speakArticle
                interactive: root.store.audioAvailable

                onToggled: root.store.speakArticle = !root.store.speakArticle
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Require correct articles")
            description: qsTr("Requires the article before the word")

            MobileSettingsToggle
            {
                checked: !root.store.allowMissingArticle

                onToggled: root.store.allowMissingArticle = !root.store.allowMissingArticle
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Learn %1 forms").arg(root.store.optionalPronoun)
            description: qsTr("Turn off to leave %1 out of conjugation trainings").arg(root.store.optionalPronoun)
            visible: root.store.optionalPronoun.length > 0

            MobileSettingsToggle
            {
                checked: root.store.learnOptionalPronoun

                onToggled: root.store.learnOptionalPronoun = !root.store.learnOptionalPronoun
            }
        }

        MobileSettingsSlider
        {
            title: qsTr("Cards per exercise")

            value: root.store.cardsPerExercise
            from: 5
            to: 20
            step: 5
            lastRow: true

            onEdited: function(newValue)
            {
                root.store.cardsPerExercise = newValue
            }
        }
    }

    MobileSettingsGroup
    {
        title: qsTr("DICTIONARY")

        MobileSettingsSlider
        {
            title: qsTr("Examples per meaning")

            value: root.store.examplesPerSense
            from: 0
            to: 5
            lastRow: true

            onEdited: function(newValue)
            {
                root.store.examplesPerSense = newValue
            }
        }
    }

    MobileSettingsGroup
    {
        title: qsTr("APPEARANCE")

        MobileSettingsRow
        {
            title: qsTr("Colour nouns by gender")

            MobileSettingsToggle
            {
                objectName: "settings_colour_by_gender"

                checked: AppTheme.colourByGender

                onToggled: AppTheme.colourByGender = !AppTheme.colourByGender
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Colour verbs by conjugation group")

            MobileSettingsToggle
            {
                objectName: "settings_colour_by_verb_group"

                checked: AppTheme.colourByVerbGroup

                onToggled: AppTheme.colourByVerbGroup = !AppTheme.colourByVerbGroup
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Animated transitions")
            lastRow: true

            MobileSettingsToggle
            {
                checked: AppMetrics.transitionsEnabled

                onToggled: AppMetrics.transitionsEnabled = !AppMetrics.transitionsEnabled
            }
        }
    }
}
