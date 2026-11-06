import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    required property var controller

    property string exerciseLabel: ""

    heading: root.exerciseLabel

    signal startRequested()

    readonly property var difficulties: [
        { key: 1, label: qsTr("Easy"), signalLevel: 1 },
        { key: 2, label: qsTr("Normal"), signalLevel: 2 },
        { key: 4, label: qsTr("Hard"), signalLevel: 3 }
    ]

    MobileOptionListPanel
    {
        MobileChipSelectRow
        {
            options: root.difficulties.map(function(difficulty)
            {
                return {
                    key: difficulty.key,
                    label: difficulty.label,
                    signalLevel: difficulty.signalLevel,
                    enabled: (root.controller.difficultyMask & difficulty.key) !== 0,
                    available: (root.controller.unlockedDifficultyMask & difficulty.key) !== 0
                }
            })

            onToggled: function(option)
            {
                root.controller.difficultyMask = root.controller.difficultyMask ^ option.key
            }
        }

        MobileSettingsSlider
        {
            title: qsTr("Sentences")
            lastRow: true

            value: root.controller.sentenceCount
            from: root.controller.minimumSentenceCount
            to: root.controller.maximumSentenceCount
            step: root.controller.sentenceCountStep

            onEdited: function(newValue) { root.controller.sentenceCount = newValue }
        }
    }

    MobileWideButton
    {
        label: qsTr("Start")
        interactive: root.controller.canStart

        onClicked: root.startRequested()
    }
}
