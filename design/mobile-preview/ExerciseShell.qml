import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property string title

    property real contentLeftMargin: Theme.exerciseContentMargin
    property real contentRightMargin: Theme.exerciseContentMargin
    property real contentTopMargin: Theme.exerciseContentMargin
    property real contentBottomMargin: Theme.exerciseContentBottomMargin
    property real contentSpacing: Theme.exerciseContentSpacing

    property bool showMute: true
    property bool showOptions: true

    property bool showProgress: true
    property real progress: 0

    readonly property int actionsHeight: Theme.actionButtonHeight
    property bool hasActions: true

    default property alias content: contentColumn.data
    property alias actions: actionsContent.data

    property bool optionsSheetOpen: false

    signal backRequested()
    signal muteRequested()
    signal restartStageRequested()
    signal completeStageRequested()

    color: Theme.panel

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        AppHeader {
            title: root.title
            showBack: true
            showMute: root.showMute
            showOptions: root.showOptions

            onBackRequested: root.backRequested()
            onMuteRequested: root.muteRequested()
            onOptionsRequested: root.optionsSheetOpen = true
        }

        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: root.contentLeftMargin
            Layout.rightMargin: root.contentRightMargin
            Layout.topMargin: root.contentTopMargin
            spacing: root.contentSpacing

            ExerciseProgress {
                id: exerciseProgress
                opacity: root.showProgress ? 1 : 0
                progress: root.progress
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.hasActions ? root.actionsHeight : 0
            Layout.leftMargin: root.contentLeftMargin
            Layout.rightMargin: root.contentRightMargin
            Layout.topMargin: root.contentSpacing
            Layout.bottomMargin: root.contentBottomMargin

            ColumnLayout {
                id: actionsContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Loader {
        anchors.fill: parent
        active: root.optionsSheetOpen

        sourceComponent: ExerciseOptionsSheet {
            onDismissed: root.optionsSheetOpen = false

            onRestartStageRequested:
            {
                root.optionsSheetOpen = false
                root.restartStageRequested()
            }

            onCompleteStageRequested:
            {
                root.optionsSheetOpen = false
                root.completeStageRequested()
            }
        }
    }
}
