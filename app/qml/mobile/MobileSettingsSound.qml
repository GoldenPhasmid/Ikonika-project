import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    MobileSettingsGroup
    {
        title: qsTr("SOUND")

        MobileSettingsRow
        {
            title: qsTr("Speech")

            MobileSettingsToggle
            {
                checked: root.store.audioEnabled
                interactive: root.store.audioAvailable

                onToggled: root.store.audioEnabled = !root.store.audioEnabled
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Speak automatically")

            MobileSettingsToggle
            {
                checked: root.store.autoSpeak
                interactive: root.store.audioAvailable && root.store.audioEnabled

                onToggled: root.store.autoSpeak = !root.store.autoSpeak
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Sound effects")

            MobileSettingsToggle
            {
                checked: AppSoundEffects.enabled

                onToggled: AppSoundEffects.enabled = !AppSoundEffects.enabled
            }
        }

        MobileSettingsSlider
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

        MobileSettingsSlider
        {
            title: qsTr("Sentence speech rate")
            lastRow: true

            value: Math.round(root.store.sentenceSpeechSpeed * 100)
            from: Math.round(root.store.minimumSpeechSpeed * 100)
            to: Math.round(root.store.maximumSpeechSpeed * 100)
            step: 5
            suffix: "%"
            interactive: root.store.audioAvailable

            onEdited: function(newValue)
            {
                root.store.sentenceSpeechSpeed = newValue / 100
                root.store.playSentenceVoiceSample()
            }
        }
    }
}
