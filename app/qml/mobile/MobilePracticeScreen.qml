import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Item
{
    id: root

    property var vocabulary: null
    property var daily: null
    property var intensive: null
    property string userDeckId: ""

    signal done()
    signal formsTrainingRequested(string training)
    signal exerciseRequested(string exercise, bool reverse, int cardCount, int repetitionCount, var filterKeys)
    signal tabSelected(string key)

    readonly property var exercises: [
        { key: "flashcards", iconSvg: Icons.Icons.exercise_flashcards.path, label: qsTr("Flashcard"),
          subtitle: qsTr("Swipe through words") },
        { key: "flip", iconSvg: Icons.Icons.exercise_flip.path, label: qsTr("Flip"),
          subtitle: qsTr("Guess then reveal") },
        { key: "choice", iconSvg: Icons.Icons.exercise_choice.path,
          label: qsTr("Multiple choice"), subtitle: qsTr("Pick the right answer") },
        { key: "correctincorrect", iconSvg: Icons.Icons.check.path, label: qsTr("Correct / Incorrect"),
          subtitle: qsTr("Mark yourself honestly") },
        { key: "written", iconSvg: Icons.Icons.exercise_written.path, label: qsTr("Typed translation"),
          subtitle: qsTr("Type the translation") },
        { key: "gap", iconSvg: Icons.Icons.edit.path, label: qsTr("Fill the gap"),
          subtitle: qsTr("Type the missing word") },
        { key: "audio", iconSvg: Icons.Icons.speaker_on.path,
          label: qsTr("Audio transcription"), subtitle: qsTr("Listen and type") },
        { key: "speedmatch", iconSvg: Icons.Icons.exercise_speedmatch.path, label: qsTr("Speed matching"),
          subtitle: qsTr("Match pairs fast") }
    ]

    readonly property var formsTrainingKinds: [
        { key: "conjugation", iconSvg: Icons.Icons.session_conjugation.path, label: qsTr("Conjugations") },
        { key: "forms", iconSvg: Icons.Icons.session_forms.path, label: qsTr("Forms") }
    ]

    property var formsTrainings: []

    function listFormsTrainings()
    {
        if (!root.daily)
        {
            return []
        }

        const offered = []

        for (const kind of root.formsTrainingKinds)
        {
            const trainingId = root.daily.formsTrainingId(kind.key)

            if (trainingId.length === 0)
            {
                continue
            }

            const resumable = root.intensive ? root.intensive.controller.hasSessionFor(trainingId) : false

            offered.push({
                key: kind.key,
                iconSvg: kind.iconSvg,
                label: kind.label,
                resumable: resumable,
                wordCount: resumable ? root.intensive.controller.wordCountFor(trainingId)
                                     : root.daily.formsWordCount(trainingId),
                ready: resumable || root.daily.canStartFormsTraining(trainingId)
            })
        }

        return offered
    }

    onVisibleChanged: if (root.visible) root.formsTrainings = root.listFormsTrainings()
    Component.onCompleted: root.formsTrainings = root.listFormsTrainings()

    Connections
    {
        target: root.daily
        enabled: root.visible

        function onFormsCountsChanged() { root.formsTrainings = root.listFormsTrainings() }
    }

    Connections
    {
        target: root.intensive ? root.intensive.controller : null
        enabled: root.visible

        function onStateChanged() { root.formsTrainings = root.listFormsTrainings() }
    }

    property var selectedExercise: null
    property bool practiceSheetOpen: false

    MobileShell
    {
        anchors.fill: parent

        activeTab: "vocabulary"
        contentMaximumWidth: AppMetrics.shellMaximumWidth
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
                model: root.userDeckId.length > 0 ? [] : root.formsTrainings

                delegate: MobileActionButton
                {
                    required property var modelData

                    objectName: "practice_" + modelData.key

                    interactive: modelData.ready
                    iconSvg: modelData.iconSvg
                    label: modelData.label
                    caption: modelData.resumable ? qsTr("Continue")
                           : modelData.ready ? "" : qsTr("Learn more words to unlock")
                    trailing: modelData.wordCount >= 0 ? modelData.wordCount.toString() : ""

                    onClicked: root.formsTrainingRequested(modelData.key)
                }
            }

            Item
            {
                visible: root.formsTrainings.length > 0 && root.userDeckId.length === 0
                Layout.preferredHeight: AppMetrics.gapSmall
            }

            Repeater
            {
                model: root.exercises

                delegate: MobileExerciseButton
                {
                    required property var modelData

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
                exerciseLabel: root.selectedExercise ? root.selectedExercise.label : ""
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
