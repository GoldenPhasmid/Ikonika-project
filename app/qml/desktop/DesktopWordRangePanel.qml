import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property var triage: null

    signal placementRequested()

    spacing: AppMetrics.gapMedium

    onVisibleChanged: if (root.visible && root.triage) root.triage.refreshStartRank()

    DesktopSectionLabel
    {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("New words")
    }

    DesktopRankSlider
    {
        objectName: "settings_word_range"

        Layout.alignment: Qt.AlignHCenter
        Layout.fillHeight: true

        rank: root.triage ? root.triage.startRank : 0
        maximumRank: root.triage ? root.triage.maximumRank : 0
        levelMarks: root.triage ? root.triage.levelMarks : []

        onRankChosen: function(rank) { root.triage.startRank = rank }
    }

    DesktopButton
    {
        objectName: "settings_placement_test"

        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Placement test")

        onClicked: root.placementRequested()
    }
}
