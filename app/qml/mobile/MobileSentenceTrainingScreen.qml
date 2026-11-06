import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Item
{
    id: root

    required property var controller

    signal done()
    signal exerciseRequested(string exercise)
    signal tabSelected(string key)

    readonly property var exercises: [
        { key: "tiles", iconSvg: Icons.Icons.exercise_choice.path, label: qsTr("Word tiles"),
          subtitle: qsTr("Rebuild the sentence") },
        { key: "meaning", iconSvg: Icons.Icons.exercise_choice.path, label: qsTr("Listen and translate"),
          subtitle: qsTr("Pick the words of the translation") },
        { key: "gap", iconSvg: Icons.Icons.exercise_written.path, label: qsTr("Missing word"),
          subtitle: qsTr("Type the missing word") },
        { key: "dictation", iconSvg: Icons.Icons.speaker_on.path, label: qsTr("Dictation"),
          subtitle: qsTr("Listen and type the sentence") }
    ]

    readonly property string notice: root.controller.available
        ? ""
        : qsTr("This dictionary has no sentences yet. Update it to get them.")

    property string selectedExercise: ""
    property string selectedLabel: ""
    property bool startSheetOpen: false

    function refresh()
    {
        root.controller.refresh()
    }

    function hasSavedSession(exercise)
    {
        return root.controller.savedExercises.indexOf(exercise) >= 0
    }

    function openStartSheet(exercise, label)
    {
        if (!root.controller.available)
        {
            return
        }

        if (root.hasSavedSession(exercise))
        {
            root.exerciseRequested(exercise)

            return
        }

        root.selectedExercise = exercise
        root.selectedLabel = label
        root.startSheetOpen = true
    }

    onVisibleChanged: if (root.visible) root.refresh()
    Component.onCompleted: if (root.visible) root.refresh()

    MobileShell
    {
        anchors.fill: parent

        activeTab: "today"
        headerTitle: qsTr("Sentences")
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

            Text
            {
                visible: root.notice.length > 0
                text: root.notice
                font.pixelSize: AppMetrics.fontBody
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourMuted
                Layout.fillWidth: true
                Layout.bottomMargin: AppMetrics.gapSmall
                wrapMode: Text.WordWrap
            }

            MobileActionButton
            {
                iconSvg: Icons.Icons.today_learn.path
                label: root.hasSavedSession("training") ? qsTr("Continue") : qsTr("Training")
                trailing: root.hasSavedSession("training") ? "" : root.controller.sentenceCount.toString()
                interactive: root.controller.available

                Layout.bottomMargin: AppMetrics.gapMedium

                onClicked: root.openStartSheet("training", qsTr("Training"))
            }

            MobileSectionLabel { text: qsTr("EXERCISES") }

            Repeater
            {
                model: root.exercises

                delegate: MobileExerciseButton
                {
                    required property var modelData

                    iconSvg: modelData.iconSvg
                    label: modelData.label
                    subtitle: root.hasSavedSession(modelData.key) ? qsTr("Continue") : modelData.subtitle
                    opacity: root.controller.available ? 1.0 : 0.5

                    onActivated: root.openStartSheet(modelData.key, modelData.label)
                }
            }
        }

        MobileSheetLoader
        {
            anchors.fill: parent
            z: 10
            open: root.startSheetOpen

            sourceComponent: MobileSentenceStartSheet
            {
                controller: root.controller
                exerciseLabel: root.selectedLabel

                onDismissed: root.startSheetOpen = false

                onStartRequested:
                {
                    root.startSheetOpen = false
                    root.exerciseRequested(root.selectedExercise)
                }
            }
        }
    }
}
