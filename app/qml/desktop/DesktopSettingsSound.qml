import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    DesktopSettingsGroup
    {
        title: qsTr("Voice")

        DesktopSettingsRow
        {
            text: qsTr("Speech")
            description: root.store.audioAvailable ? qsTr("Words are read aloud") : qsTr("No voice is installed for this language")

            DesktopToggle
            {
                accessibleName: qsTr("Speech")
                on: root.store.audioEnabled
                enabled: root.store.audioAvailable

                onClicked: root.store.audioEnabled = !root.store.audioEnabled
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Speak automatically")

            DesktopToggle
            {
                accessibleName: qsTr("Speak automatically")
                on: root.store.autoSpeak
                enabled: root.store.audioAvailable && root.store.audioEnabled

                onClicked: root.store.autoSpeak = !root.store.autoSpeak
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Sound effects")

            DesktopToggle
            {
                accessibleName: qsTr("Sound effects")
                on: AppSoundEffects.enabled

                onClicked: AppSoundEffects.enabled = !AppSoundEffects.enabled
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Voice")

            DesktopSelect
            {
                objectName: "settings_speaker"

                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                enabled: root.store.supertonicActive && root.store.audioAvailable
                heading: qsTr("Voice")
                options: ["Sarah", "Lily", "Jessica", "Olivia", "Emily",
                          "Alex", "James", "Robert", "Sam", "Daniel"]
                currentIndex: root.store.speakerId

                onChosen: function(index)
                {
                    root.store.speakerId = index
                    root.store.playVoiceSample()
                }
            }
        }

        DesktopSettingsSlider
        {
            title: qsTr("Speech rate")
            value: Math.round(root.store.speechSpeed * 100)
            from: Math.round(root.store.minimumSpeechSpeed * 100)
            to: Math.round(root.store.maximumSpeechSpeed * 100)
            step: 5
            suffix: "%"
            interactive: root.store.audioAvailable

            onEdited: function(newValue)
            {
                root.store.speechSpeed = newValue / 100
                root.store.playVoiceSample()
            }
        }

        DesktopSettingsSlider
        {
            title: qsTr("Sentence speech rate")
            value: Math.round(root.store.sentenceSpeechSpeed * 100)
            from: Math.round(root.store.minimumSpeechSpeed * 100)
            to: Math.round(root.store.maximumSpeechSpeed * 100)
            step: 5
            suffix: "%"
            interactive: root.store.audioAvailable
            lastRow: true

            onEdited: function(newValue)
            {
                root.store.sentenceSpeechSpeed = newValue / 100
                root.store.playSentenceVoiceSample()
            }
        }
    }
}
