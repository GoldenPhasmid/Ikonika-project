import QtQuick
import QtQuick.Layouts

MobileExerciseShell
{
    id: root

    required property int wordCount
    required property int elapsedSeconds

    property string headline: qsTr("Training complete")
    property string countLabel: qsTr("words trained")

    signal doneClicked()

    readonly property string elapsedText:
    {
        const total = Math.max(0, root.elapsedSeconds)
        const minutes = Math.floor(total / 60)
        const seconds = total % 60

        return qsTr("%1:%2").arg(minutes).arg(seconds < 10 ? "0" + seconds : seconds)
    }

    onVisibleChanged: if (root.visible) root.enter()

    showMute: false
    showOptions: false
    showProgress: false

    readonly property var stats: [
        { value: root.wordCount.toString(), label: root.countLabel },
        { value: root.elapsedText, label: qsTr("elapsed") }
    ]

    MobileContentArea
    {
        posVisible: false

        primaryContent: Text
        {
            text: root.headline
            font.pixelSize: AppMetrics.fontHeadline
            font.weight: Font.Medium
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        secondaryContent: GridLayout
        {
            Layout.fillWidth: true

            columns: 2
            columnSpacing: AppMetrics.gapMedium
            rowSpacing: AppMetrics.gapMedium

            Repeater
            {
                model: root.stats

                delegate: Rectangle
                {
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredHeight: AppMetrics.statBoxHeight

                    radius: AppMetrics.radiusMedium
                    color: AppTheme.colourSunken
                    border.color: AppTheme.colourBorder
                    border.width: AppMetrics.borderWidth

                    ColumnLayout
                    {
                        anchors.centerIn: parent
                        spacing: AppMetrics.gapTight

                        Text
                        {
                            Layout.alignment: Qt.AlignHCenter

                            text: modelData.value
                            font.pixelSize: AppMetrics.statValueSize
                            font.weight: Font.DemiBold
                            font.family: AppMetrics.fontFamily
                            color: AppTheme.colourText
                        }

                        Text
                        {
                            Layout.alignment: Qt.AlignHCenter

                            text: modelData.label
                            font.pixelSize: AppMetrics.fontMeta
                            font.family: AppMetrics.fontFamily
                            color: AppTheme.colourMuted
                        }
                    }
                }
            }
        }
    }

    actions: MobileWideButton
    {
        label: qsTr("Done")

        onClicked: root.doneClicked()
    }
}
