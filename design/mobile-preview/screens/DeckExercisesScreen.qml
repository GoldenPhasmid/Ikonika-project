import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    id: root

    readonly property var exercises: [
        { icon: "🃏", label: "Flashcard", subtitle: "Swipe through words", count: 48 },
        { icon: "🔄", label: "Flip", subtitle: "Guess then reveal", count: 31 },
        { icon: "◉", label: "Multiple choice", subtitle: "Pick the right answer", count: 52 },
        { icon: "⌨", label: "Typed", subtitle: "Type the translation", count: 24 },
        { icon: "🔊", label: "Audio", subtitle: "Listen and type", count: 17 },
        { icon: "⚡", label: "Speed matching", subtitle: "Match pairs fast", count: 39 }
    ]

    property var selectedExercise: null
    property bool practiceSheetOpen: false

    headerTitle: "Mercado y comida · Exercises"
    showBack: true
    showSearch: true

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 12

        Repeater {
            model: root.exercises
            delegate: ExerciseButton {
                required property var modelData

                icon: modelData.icon
                label: modelData.label
                subtitle: modelData.subtitle
                count: modelData.count

                onClicked:
                {
                    root.selectedExercise = modelData
                    root.practiceSheetOpen = true
                }
            }
        }
    }

    Loader {
        anchors.fill: parent
        z: 10
        active: root.practiceSheetOpen

        sourceComponent: ExercisePracticeSheet {
            exerciseIcon: root.selectedExercise ? root.selectedExercise.icon : ""
            exerciseLabel: root.selectedExercise ? root.selectedExercise.label : ""
            exerciseSubtitle: root.selectedExercise ? root.selectedExercise.subtitle : ""
            wordCount: root.selectedExercise ? root.selectedExercise.count : 0

            onDismissed: root.practiceSheetOpen = false
            onStartRequested: root.practiceSheetOpen = false
        }
    }
}
