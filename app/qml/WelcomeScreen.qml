import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import "Greeting.js" as Greeting

FocusScope
{
    id: root

    property string name: ""
    property int reviewCount: 0
    property int newCount: 0
    property bool shown: true

    signal finished()

    readonly property int holdDuration: 1500
    readonly property string greeting: Greeting.forHour(new Date().getHours(), root.name)

    function dismiss()
    {
        if (!root.shown || exitAnimation.running)
        {
            return
        }

        holdTimer.stop()
        exitAnimation.start()
    }

    visible: root.shown
    focus: root.shown

    Accessible.role: Accessible.Pane
    Accessible.name: root.greeting

    Keys.onPressed: function(event)
    {
        event.accepted = true
        root.dismiss()
    }

    Rectangle
    {
        id: backdrop

        anchors.fill: parent
        color: AppTheme.colourPanel
    }

    MouseArea
    {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons

        onClicked: root.dismiss()
        onWheel: function(wheel) { wheel.accepted = true }
    }

    ColumnLayout
    {
        id: content

        anchors.centerIn: parent
        width: AppMetrics.contentWidthNarrow > 0 ? Math.min(parent.width - AppMetrics.contentPadding * 2, AppMetrics.contentWidthNarrow)
                                                 : parent.width - AppMetrics.contentPadding * 2
        spacing: AppMetrics.gapLarge
        opacity: 0

        transform: Translate { id: lift; y: AppMetrics.motionTravel }

        Text
        {
            Layout.fillWidth: true
            text: root.greeting
            font.pixelSize: AppMetrics.fontHeadline
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        RowLayout
        {
            Layout.alignment: Qt.AlignHCenter
            spacing: AppMetrics.gapLarge * 2

            Repeater
            {
                model: [
                    { count: root.reviewCount, label: qsTr("to review") },
                    { count: root.newCount, label: qsTr("new words") }
                ]

                delegate: ColumnLayout
                {
                    required property var modelData

                    spacing: AppMetrics.gapTight

                    Text
                    {
                        Layout.alignment: Qt.AlignHCenter
                        text: modelData.count
                        font.pixelSize: AppMetrics.fontTitle
                        font.weight: Font.Bold
                        font.family: AppMetrics.fontFamily
                        color: modelData.count > 0 ? AppTheme.colourText : AppTheme.colourMuted
                    }

                    Text
                    {
                        Layout.alignment: Qt.AlignHCenter
                        text: modelData.label
                        font.pixelSize: AppMetrics.fontBody
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourSubtle
                    }
                }
            }
        }
    }

    ParallelAnimation
    {
        id: entryAnimation

        running: root.shown

        NumberAnimation { target: content; property: "opacity"; to: 1; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: lift; property: "y"; to: 0; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
    }

    Timer
    {
        id: holdTimer

        interval: root.holdDuration
        running: root.shown && root.Window.window !== null && root.Window.window.visible

        onTriggered: root.dismiss()
    }

    SequentialAnimation
    {
        id: exitAnimation

        ParallelAnimation
        {
            NumberAnimation { target: content; property: "opacity"; to: 0; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
            NumberAnimation { target: lift; property: "y"; to: -AppMetrics.motionTravel; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
            NumberAnimation { target: backdrop; property: "opacity"; to: 0; duration: AppMetrics.motionSlow * 2; easing.type: AppMetrics.motionEasing }
        }

        ScriptAction
        {
            script:
            {
                root.shown = false
                root.finished()
            }
        }
    }
}
