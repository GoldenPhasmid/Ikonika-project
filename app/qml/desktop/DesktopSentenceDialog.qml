import QtQuick
import QtQuick.Layouts

DesktopStartDialog
{
    id: root

    property var controller: null

    signal exerciseChosen(string exercise)

    readonly property var saved: root.controller ? root.controller.savedExercises : []

    readonly property var exercises: [
        { key: "training", iconName: "today_learn", label: qsTr("Training") },
        { key: "tiles", iconName: "exercise_choice", label: qsTr("Word tiles") },
        { key: "meaning", iconName: "exercise_choice", label: qsTr("Listen and translate") },
        { key: "gap", iconName: "exercise_written", label: qsTr("Missing word") },
        { key: "dictation", iconName: "speaker_on", label: qsTr("Dictation") }
    ]

    readonly property var difficulties: [
        { key: 1, label: qsTr("Easy") },
        { key: 2, label: qsTr("Normal") },
        { key: 4, label: qsTr("Hard") }
    ]

    function openFor()
    {
        root.chosenKey = "training"

        if (root.controller)
        {
            root.controller.refresh()
        }

        root.open()
    }

    title: qsTr("Sentences")
    choices: root.exercises.map(function(exercise)
    {
        return {
            key: exercise.key,
            iconName: exercise.iconName,
            label: exercise.label,
            caption: root.saved.indexOf(exercise.key) >= 0 ? qsTr("Continue") : ""
        }
    })
    canStart: !!root.controller && (root.saved.indexOf(root.chosenKey) >= 0 || root.controller.canStart)
    startLabel: root.saved.indexOf(root.chosenKey) >= 0 ? qsTr("Continue") : qsTr("Start · %n sentences", "", root.controller ? root.controller.sentenceCount : 0)

    onChoiceActivated: function(key) { root.chosenKey = key }

    onStartRequested:
    {
        const exercise = root.chosenKey

        root.closeThen(function() { root.exerciseChosen(exercise) })
    }

    settings: RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapLarge

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: AppMetrics.gapMedium
            Layout.bottomMargin: AppMetrics.gapMedium
            spacing: AppMetrics.gapSmall

            DesktopLabel
            {
                Layout.fillWidth: true
                text: qsTr("Difficulty")
                font.weight: Font.Medium
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapSmall

                Repeater
                {
                    model: root.difficulties

                    delegate: DesktopChip
                    {
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredWidth: 1

                        text: modelData.label
                        enabled: !!root.controller && (root.controller.unlockedDifficultyMask & modelData.key) !== 0
                        selected: !!root.controller && (root.controller.difficultyMask & modelData.key) !== 0

                        onClicked: root.controller.difficultyMask = root.controller.difficultyMask ^ modelData.key
                    }
                }
            }
        }

        DesktopSettingsSlider
        {
            Layout.preferredWidth: 1
            title: qsTr("Sentences")
            inset: 0
            lastRow: true
            value: root.controller ? root.controller.sentenceCount : 0
            from: root.controller ? root.controller.minimumSentenceCount : 0
            to: root.controller ? root.controller.maximumSentenceCount : 0
            step: root.controller ? root.controller.sentenceCountStep : 1

            onEdited: function(newValue) { root.controller.sentenceCount = newValue }
        }
    }
}
