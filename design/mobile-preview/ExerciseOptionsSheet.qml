import QtQuick
import QtQuick.Layouts

BottomSheet {
    id: root

    heading: "Options"

    signal restartStageRequested()
    signal completeStageRequested()
    signal endSessionRequested()

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: rows.implicitHeight

        radius: Theme.radiusMedium
        color: Theme.panel
        border.color: Theme.line
        border.width: 1
        clip: true

        ColumnLayout {
            id: rows

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0

            SettingsRow {
                title: "Restart stage"

                onClicked: root.restartStageRequested()

                SettingsChevron {}
            }

            SettingsRow {
                title: "Complete stage"

                onClicked: root.completeStageRequested()

                SettingsChevron {}
            }

            SettingsRow {
                title: "End this session"
                destructive: true
                lastRow: true

                onClicked: root.endSessionRequested()
            }
        }
    }
}
