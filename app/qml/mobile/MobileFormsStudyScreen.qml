import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property var controller: FlashcardController {}

    property var store: null

    property bool showComplete: true
    property bool inSession: false

    signal done()
    signal endSessionRequested()
    signal restartRequested()
    signal completeRequested()

    MobileExerciseShell
    {
        id: shell

        anchors.fill: parent

        exerciseName: qsTr("Forms")
        store: root.store

        progress: root.controller.exerciseProgress

        inSession: root.inSession

        onExitRequested: root.done()
        onEndSessionRequested: root.endSessionRequested()
        onRestartRequested: root.restartRequested()
        onCompleteRequested: root.completeRequested()

        MobileContentArea
        {
            posText: root.controller.tense
            posVisible: root.controller.tense.length > 0
            counterText: qsTr("%1 / %2")
                .arg(root.controller.currentSessionCount)
                .arg(root.controller.maxSessionCount)

            primaryContent: [
                Text
                {
                    text: root.controller.prompt
                    font.pixelSize: AppMetrics.sessionAnswerSize
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourText
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                },
                Text
                {
                    text: root.controller.answer
                    font.pixelSize: AppMetrics.sessionTranslationSize
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourMuted
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
            ]

            secondaryContent: MobileFormsTable
            {
                Layout.fillWidth: true
                forms: root.controller.forms
            }
        }

        actions: MobileButtonPair
        {
            leftLabel: qsTr("Previous")
            rightLabel: root.controller.canGoNext ? qsTr("Next") : qsTr("Finish")

            leftArrow: "left"
            rightArrow: root.controller.canGoNext ? "right" : ""

            leftEnabled: root.controller.canGoPrevious
            rightEnabled: true

            onLeftClicked: shell.swapBack(function() { root.controller.previous() })
            onRightClicked: shell.swap(function() { root.controller.next() })
        }
    }
}
