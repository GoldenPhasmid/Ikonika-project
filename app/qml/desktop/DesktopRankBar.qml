import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property int rank: 0
    property int maximumRank: 0
    property var levelMarks: []
    property string title: ""
    property bool lastRow: false
    property real inset: AppMetrics.gapLarge

    signal rankChosen(int rank)

    Layout.fillWidth: true
    implicitHeight: column.implicitHeight + AppMetrics.gapMedium * 2

    ColumnLayout
    {
        id: column

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.inset
        anchors.rightMargin: root.inset
        anchors.verticalCenter: parent.verticalCenter
        spacing: AppMetrics.gapSmall

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapMedium

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.title
                font.weight: Font.Medium
                wrapMode: Text.WordWrap
            }

            DesktopLabel
            {
                text: ruler.valueText
                font.weight: Font.DemiBold
                color: AppTheme.colourSubtle
            }
        }

        RankRuler
        {
            id: ruler

            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapSmall
            rank: root.rank
            maximumRank: root.maximumRank
            levelMarks: root.levelMarks

            onRankChosen: function(chosen) { root.rankChosen(chosen) }
        }
    }

    DesktopDivider
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.inset
        anchors.rightMargin: root.inset
        visible: !root.lastRow
    }
}
