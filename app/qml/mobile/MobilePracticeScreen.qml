import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Item
{
    id: root

    property var vocabulary: null

    signal done()
    signal exerciseRequested(string exercise, bool reverse, int cardCount, int repetitionCount, var filterKeys)
    signal tabSelected(string key)

    readonly property var exercises: [
        { key: "flashcards", iconSvg: Icons.Icons.exercise_flashcards.path, label: qsTr("Flashcard"),
          subtitle: qsTr("Swipe through words") },
        { key: "flip", iconSvg: Icons.Icons.exercise_flip.path, label: qsTr("Flip"),
          subtitle: qsTr("Guess then reveal") },
        { key: "choice", iconSvg: Icons.Icons.exercise_choice.path,
          label: qsTr("Multiple choice"), subtitle: qsTr("Pick the right answer") },
        { key: "correctincorrect", icon: "\u{2713}", label: qsTr("Correct / Incorrect"),
          subtitle: qsTr("Mark yourself honestly") },
        { key: "written", iconSvg: Icons.Icons.exercise_written.path, label: qsTr("Typed translation"),
          subtitle: qsTr("Type the translation") },
        { key: "audio", iconSvg: Icons.Icons.speaker_on.path,
          label: qsTr("Audio transcription"), subtitle: qsTr("Listen and type") },
        { key: "speedmatch", iconSvg: Icons.Icons.exercise_speedmatch.path, label: qsTr("Speed matching"),
          subtitle: qsTr("Match pairs fast") }
    ]

    property var selectedExercise: null
    property bool practiceSheetOpen: false

    MobileShell
    {
        anchors.fill: parent

        activeTab: "vocabulary"
        headerTitle: qsTr("Practice")
        showBack: true
        showTabBar: false

        onBackRequested: root.done()
        onTabSelected: function(key) { root.tabSelected(key) }

        ColumnLayout
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppMetrics.contentPadding
            spacing: AppMetrics.gapSmall

            Repeater
            {
                model: root.exercises

                delegate: MobileExerciseButton
                {
                    required property var modelData

                    icon: modelData.icon || ""
                    iconSvg: modelData.iconSvg || ""
                    label: modelData.label
                    subtitle: modelData.subtitle

                    onActivated:
                    {
                        root.selectedExercise = modelData
                        root.practiceSheetOpen = true
                    }
                }
            }
        }

        MobileSheetLoader
        {
            anchors.fill: parent
            z: 10
            open: root.practiceSheetOpen

            sourceComponent: MobileExercisePracticeSheet
            {
                exerciseKey: root.selectedExercise ? root.selectedExercise.key : ""
                exerciseIcon: root.selectedExercise ? (root.selectedExercise.icon || "") : ""
                exerciseIconSvg: root.selectedExercise ? (root.selectedExercise.iconSvg || "") : ""
                exerciseLabel: root.selectedExercise ? root.selectedExercise.label : ""
                exerciseSubtitle: root.selectedExercise ? root.selectedExercise.subtitle : ""
                vocabulary: root.vocabulary

                onDismissed: root.practiceSheetOpen = false

                onStartRequested: function(reverse, cardCount, repetitionCount, filterKeys)
                {
                    root.practiceSheetOpen = false
                    root.exerciseRequested(root.selectedExercise.key, reverse, cardCount, repetitionCount, filterKeys)
                }
            }
        }
    }
}
