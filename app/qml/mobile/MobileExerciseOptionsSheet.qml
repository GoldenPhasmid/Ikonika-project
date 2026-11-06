import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    property var questionSideNames: []
    property int questionSideIndex: 0

    property bool showStageActions: true
    property bool showEditCard: false

    property bool showVoiceOption: false
    property bool voiceOn: false
    property bool voiceAvailable: true

    property bool showTypingOption: false
    property bool typingOn: false

    heading: qsTr("Options")

    signal questionSideChosen(int index)
    signal voiceToggled()
    signal typingToggled()
    signal editCardRequested()
    signal restartStageRequested()
    signal completeStageRequested()
    signal endSessionRequested()

    MobileOptionListPanel
    {
        MobileSettingsRow
        {
            visible: root.showVoiceOption
            title: qsTr("Voice")
            description: root.voiceAvailable ? qsTr("Play the word instead of showing it")
                                             : qsTr("Turn the sound on to use voice")

            MobileSettingsToggle
            {
                checked: root.voiceOn && root.voiceAvailable
                interactive: root.voiceAvailable

                onToggled: if (root.voiceAvailable) root.voiceToggled()
            }
        }

        MobileSettingsRow
        {
            visible: root.showTypingOption
            title: qsTr("Hard mode")
            description: qsTr("Write each sentence instead of building it from tiles")

            MobileSettingsToggle
            {
                checked: root.typingOn

                onToggled: root.typingToggled()
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Ask with")
            visible: !root.showStageActions
            lastRow: !root.showStageActions

            MobileSettingsSelect
            {
                label: root.questionSideNames.length > 0
                    ? root.questionSideNames[root.questionSideIndex] : ""

                onClicked: picker.open(qsTr("Ask with"),
                    root.questionSideNames,
                    root.questionSideIndex,
                        function(index) { root.questionSideChosen(index) })
            }
        }

        MobileSettingsRow
        {
            visible: root.showEditCard
            title: qsTr("Edit card")

            onClicked: root.editCardRequested()

            MobileSettingsChevron {}
        }

        MobileSettingsRow
        {
            visible: root.showStageActions
            title: qsTr("Restart stage")

            onClicked: root.restartStageRequested()

            MobileSettingsChevron {}
        }

        MobileSettingsRow
        {
            visible: root.showStageActions
            title: qsTr("Complete stage")

            onClicked: root.completeStageRequested()

            MobileSettingsChevron {}
        }

        MobileSettingsRow
        {
            visible: root.showStageActions
            title: qsTr("End this session")
            destructive: true
            lastRow: true
            holdToConfirm: true

            onClicked: root.endSessionRequested()
        }
    }

    overlays: MobileSettingsPicker
    {
        id: picker
    }
}
