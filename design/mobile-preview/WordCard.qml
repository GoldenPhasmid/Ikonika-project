import QtQuick
import QtQuick.Shapes
import QtQuick.Layouts

Rectangle {
    id: root

    required property string word
    property string translation: ""
    property string cefrLevel: ""

    property int stage: -1
    property bool completed: false
    property int maxStage: 5

    property bool showVoice: true
    property bool showProgress: true
    property bool showTranslation: true
    property bool showCefr: true

    property bool showAddAction: false
    property bool showEditAction: true
    property bool showDeleteAction: true

    property var coordinator: null

    signal clicked()
    signal voicePlayRequested()
    signal addRequested()
    signal editRequested()
    signal deleteRequested()

    function close()
    {
        front.x = 0

        if (root.coordinator && root.coordinator.openRow === root)
        {
            root.coordinator.openRow = null
        }
    }

    readonly property bool learning: root.stage >= 0 || root.completed

    readonly property real ringFraction: root.completed
        ? 1.0
        : (root.maxStage > 0 ? Math.min(1.0, Math.max(0, root.stage) / root.maxStage) : 0)

    readonly property int actionWidth: 64

    readonly property real actionsWidth: root.actionWidth *
        ((root.showAddAction ? 1 : 0) + (root.showEditAction ? 1 : 0) + (root.showDeleteAction ? 1 : 0))

    implicitHeight: 72
    height: implicitHeight

    radius: Theme.radiusMedium
    color: Theme.panel
    border.color: Theme.line
    border.width: 1
    clip: true

    Item {
        id: revealMask

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: -front.x
        clip: true

        Row {
            id: actionsRow

            anchors.right: parent.right
            height: parent.height
            width: root.actionsWidth

            WordCardAction {
                width: root.actionWidth
                height: actionsRow.height
                visible: root.showAddAction

                icon: "➕"
                tint: Theme.accent
                iconColor: "#ffffff"

                onTriggered: { root.close(); root.addRequested() }
            }

            WordCardAction {
                width: root.actionWidth
                height: actionsRow.height
                visible: root.showEditAction

                icon: "✎"
                tint: Theme.faint
                iconColor: Theme.ink

                onTriggered: { root.close(); root.editRequested() }
            }

            WordCardAction {
                width: root.actionWidth
                height: actionsRow.height
                visible: root.showDeleteAction
                holdToConfirm: true

                icon: "🗑"
                tint: Theme.wrong
                iconColor: "#ffffff"

                onTriggered: { root.close(); root.deleteRequested() }
            }
        }
    }

    Item {
        id: front

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width

        Behavior on x {
            enabled: !dragHandler.active
            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
        }

        DragHandler {
            id: dragHandler

            target: front
            yAxis.enabled: false
            xAxis.enabled: true
            xAxis.minimum: -root.actionsWidth
            xAxis.maximum: 0

            onActiveChanged:
            {
                if (dragHandler.active || root.actionsWidth <= 0)
                {
                    return
                }

                if (front.x >= -root.actionsWidth / 2)
                {
                    root.close()
                    return
                }

                front.x = -root.actionsWidth

                if (!root.coordinator)
                {
                    return
                }

                if (root.coordinator.openRow && root.coordinator.openRow !== root)
                {
                    root.coordinator.openRow.close()
                }

                root.coordinator.openRow = root
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 12
            spacing: 12

            Item {
                id: voiceOverlay

                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                visible: root.showVoice

                Shape {
                    anchors.fill: parent
                    visible: root.showProgress && root.learning
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        strokeColor: Theme.line
                        strokeWidth: 3
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap

                        PathAngleArc {
                            centerX: voiceOverlay.width / 2
                            centerY: voiceOverlay.height / 2
                            radiusX: voiceOverlay.width / 2 - 1.5
                            radiusY: voiceOverlay.height / 2 - 1.5
                            startAngle: 0
                            sweepAngle: 360
                            moveToStart: true
                        }
                    }

                    ShapePath {
                        strokeColor: root.completed ? Theme.correct : Theme.accent
                        strokeWidth: 3
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap

                        PathAngleArc {
                            centerX: voiceOverlay.width / 2
                            centerY: voiceOverlay.height / 2
                            radiusX: voiceOverlay.width / 2 - 1.5
                            radiusY: voiceOverlay.height / 2 - 1.5
                            startAngle: -90
                            sweepAngle: 360 * root.ringFraction
                            moveToStart: true
                        }
                    }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 32
                    height: 32
                    radius: width / 2
                    color: Theme.faint

                    Text {
                        anchors.centerIn: parent
                        text: "\u{1F50A}"
                        font.pixelSize: 14
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.voicePlayRequested()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: root.word
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                    color: Theme.ink
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.showTranslation && root.translation.length > 0
                    text: root.translation
                    font.pixelSize: 13
                    font.family: Theme.fontFamily
                    color: Theme.mid
                    elide: Text.ElideRight
                }
            }

            Item { Layout.fillWidth: true }

            CefrBadge {
                visible: root.showCefr && root.cefrLevel.length > 0
                level: root.cefrLevel
            }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton

            onTapped:
            {
                if (front.x < 0)
                {
                    root.close()
                    return
                }

                root.clicked()
            }
        }
    }
}
