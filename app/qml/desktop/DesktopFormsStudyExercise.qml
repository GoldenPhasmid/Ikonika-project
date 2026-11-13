import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property var controller: null
    property var store: null

    readonly property string kind: "forms"
    readonly property Item surface: card.surface
    readonly property bool editable: false

    readonly property string instruction: qsTr("Study forms")
    readonly property bool voiceSupported: false
    readonly property bool voiceOn: false

    function toggleVoice()
    {
    }

    function playVoice()
    {
        promptLabel.speak()
    }

    function next()
    {
        card.swap(function() { root.controller.next() })
    }

    function previous()
    {
        if (root.controller.canGoPrevious)
        {
            card.swap(function() { root.controller.previous() })
        }
    }

    function handleKey(event)
    {
        if (!root.controller)
        {
            return false
        }

        if (event.key === Qt.Key_Right || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
        {
            root.next()

            return true
        }

        if (event.key === Qt.Key_Left)
        {
            root.previous()

            return true
        }

        return false
    }

    DesktopExerciseCard
    {
        id: card

        anchors.fill: parent

        primarySpeaks: true
        onPrimaryClicked: root.playVoice()
        tags: root.controller ? [root.controller.tense] : []
        counterText: root.controller ? qsTr("%1 / %2").arg(root.controller.currentSessionCount).arg(root.controller.maxSessionCount) : ""

        primary: [
            DesktopPromptLabel
            {
                id: promptLabel

                text: root.controller ? root.controller.prompt : ""
                speakable: true

                onSpeakRequested: root.controller.replay()

                Accessible.role: Accessible.Heading
                Accessible.name: text
            },
            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.controller ? root.controller.answer : ""
                font.pixelSize: AppMetrics.fontLead
                color: AppTheme.colourSubtle
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        ]

        secondary: DesktopFormsGrid
        {
            Layout.fillWidth: true
            forms: root.controller ? root.controller.forms : []
        }

        controls: [
            DesktopButton
            {
                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "secondary"
                iconName: "arrow_left"
                text: qsTr("Previous")
                keyHint: qsTr("Left")
                enabled: !!root.controller && root.controller.canGoPrevious

                onClicked: root.previous()
            },
            DesktopButton
            {
                objectName: "exercise_continue"

                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "primary"
                text: root.controller && root.controller.canGoNext ? qsTr("Next") : qsTr("Finish")
                trailingIconName: root.controller && root.controller.canGoNext ? "arrow_right" : ""
                keyHint: qsTr("Enter")

                onClicked: root.next()
            }
        ]
    }
}
