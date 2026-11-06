import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle
{
    id: root

    property string title: ""
    property bool muted: false
    property bool soundAvailable: true
    property bool showOptions: true
    property bool divided: true

    property alias optionsButton: optionsButton
    property alias tools: toolRow.data

    signal exitRequested()
    signal soundToggled()
    signal optionsRequested()

    implicitHeight: AppMetrics.topBarHeight
    color: AppTheme.colourPanel

    DesktopDivider
    {
        visible: root.divided
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    RowLayout
    {
        anchors.fill: parent
        anchors.leftMargin: AppMetrics.gapMedium
        anchors.rightMargin: AppMetrics.gapMedium
        spacing: AppMetrics.gapSmall

        DesktopIconButton
        {
            objectName: "session_exit"

            iconName: "arrow_left"
            text: qsTr("Back (Esc)")

            onClicked: root.exitRequested()
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.title
            font.pixelSize: AppMetrics.fontLabel
            font.weight: Font.DemiBold
            elide: Text.ElideRight

            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        RowLayout
        {
            id: toolRow

            Layout.fillWidth: false
            spacing: AppMetrics.gapSmall
        }

        DesktopIconButton
        {
            objectName: "session_sound"

            enabled: root.soundAvailable
            iconName: root.muted ? "speaker_off" : "speaker_on"
            text: root.muted ? qsTr("Turn sound on") : qsTr("Turn sound off")

            onClicked: root.soundToggled()
        }

        DesktopIconButton
        {
            id: optionsButton

            objectName: "session_options"

            visible: root.showOptions
            iconName: "kebab"
            text: qsTr("Session options")

            onClicked: root.optionsRequested()
        }
    }
}
