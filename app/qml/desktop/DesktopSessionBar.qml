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
    property bool showSlowSpeech: false
    property bool divided: true

    property alias optionsButton: optionsButton
    property alias tools: toolRow.data

    signal exitRequested()
    signal soundToggled()
    signal slowSpeechRequested()
    signal optionsRequested()

    implicitHeight: AppMetrics.topBarHeight

    onTitleChanged: titleSwap.restart()

    SequentialAnimation
    {
        id: titleSwap

        NumberAnimation { target: titleLabel; property: "opacity"; to: 0; duration: AppMetrics.motionFast }
        ScriptAction { script: titleLabel.shownTitle = root.title }
        PropertyAction { target: titleShift; property: "y"; value: AppMetrics.gapSmall }

        ParallelAnimation
        {
            NumberAnimation { target: titleLabel; property: "opacity"; to: 1; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
            NumberAnimation { target: titleShift; property: "y"; to: 0; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
        }
    }
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
            id: titleLabel

            property string shownTitle: root.title

            objectName: "session_title"

            Layout.fillWidth: true
            text: titleLabel.shownTitle
            font.pixelSize: AppMetrics.fontLabel
            font.weight: Font.DemiBold
            elide: Text.ElideRight

            transform: Translate { id: titleShift }

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
            objectName: "session_slow_speech"

            visible: root.showSlowSpeech
            enabled: root.soundAvailable
            iconName: "turtle"
            text: qsTr("Say it slowly")

            onClicked: root.slowSpeechRequested()
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
