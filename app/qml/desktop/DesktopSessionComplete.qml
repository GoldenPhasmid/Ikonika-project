import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope
{
    id: root

    property string headline: qsTr("Session complete")
    property int wordCount: 0
    property int elapsedSeconds: 0
    property string countLabel: qsTr("words trained")

    signal doneClicked()

    readonly property string elapsedText:
    {
        const total = Math.max(0, root.elapsedSeconds)
        const minutes = Math.floor(total / 60)
        const seconds = total % 60

        return qsTr("%1:%2").arg(minutes).arg(seconds < 10 ? "0" + seconds : seconds)
    }

    readonly property var stats: [
        { value: root.wordCount.toString(), label: root.countLabel },
        { value: root.elapsedText, label: qsTr("time spent") }
    ]

    onVisibleChanged:
    {
        if (!root.visible)
        {
            return
        }

        doneButton.forceActiveFocus()
        enterMotion.restart()
    }

    ParallelAnimation
    {
        id: enterMotion

        NumberAnimation { target: column; property: "opacity"; from: 0; to: 1; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: column; property: "scale"; from: 0.96; to: 1; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
    }

    ColumnLayout
    {
        id: column

        anchors.centerIn: parent
        width: Math.min(AppMetrics.dialogWidth, root.width - AppMetrics.contentPadding * 2)
        spacing: AppMetrics.gapLarge

        Rectangle
        {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: AppMetrics.completeBadgeSize
            Layout.preferredHeight: AppMetrics.completeBadgeSize
            radius: width / 2
            color: Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourCorrect, AppTheme.alphaMedium))

            DesktopIcon
            {
                anchors.centerIn: parent
                name: "check"
                width: AppMetrics.completeGlyphSize
                height: width
                strokeWidth: AppMetrics.iconStrokeBold
                color: AppTheme.colourCorrect
            }
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.headline
            font.pixelSize: AppMetrics.fontHeadline
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap

            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapMedium

            Repeater
            {
                model: root.stats

                delegate: Rectangle
                {
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: AppMetrics.statBoxHeight

                    radius: AppMetrics.radiusLarge
                    color: AppTheme.colourPanel
                    border.width: AppMetrics.borderWidth
                    border.color: AppTheme.colourBorder

                    Accessible.role: Accessible.StaticText
                    Accessible.name: qsTr("%1 %2").arg(modelData.value).arg(modelData.label)

                    ColumnLayout
                    {
                        anchors.centerIn: parent
                        spacing: AppMetrics.gapTight

                        DesktopLabel
                        {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.value
                            font.pixelSize: AppMetrics.statValueSize
                            font.weight: Font.DemiBold
                        }

                        DesktopLabel
                        {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.label
                            font.pixelSize: AppMetrics.fontSmall
                            color: AppTheme.colourMuted
                        }
                    }
                }
            }
        }

        DesktopButton
        {
            id: doneButton

            objectName: "session_done"

            Layout.fillWidth: true
            large: true
            tone: "primary"
            text: qsTr("Done")
            keyHint: qsTr("Enter")

            onClicked: root.doneClicked()
        }
    }
}
