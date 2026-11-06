import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property alias posText: meta.posText
    property alias posVisible: meta.posVisible
    property alias counterText: meta.counterText

    property alias primaryContent: primaryColumn.data
    property alias secondaryContent: secondaryColumn.data

    property bool showPrimary: true
    property bool reserveActionsSpace: false

    Layout.fillWidth: true
    Layout.fillHeight: true
    radius: Theme.radiusMedium
    color: Theme.panel
    border.color: Theme.line
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 24

        ExerciseMeta {
            id: meta
        }

        Item {
            id: primaryContent
            visible: root.showPrimary
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 1
            Layout.verticalStretchFactor: 40

            ColumnLayout {
                id: primaryColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                spacing: 16
            }
        }

        Rectangle {
            id: separatorBar
            visible: root.showPrimary
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.line
            opacity: 0.35
        }

        Item {
            id: secondaryContent
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 1
            Layout.verticalStretchFactor: root.showPrimary ? 60 : 100

            Flickable {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: root.showPrimary ? 0 : Math.max(0, (parent.height - height) / 2)
                height: Math.min(secondaryColumn.height, parent.height)
                contentWidth: width
                contentHeight: secondaryColumn.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: secondaryColumn
                    width: parent.width
                    spacing: 16
                }
            }
        }

        Item {
            visible: root.reserveActionsSpace
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.actionButtonHeight
        }
    }
}
