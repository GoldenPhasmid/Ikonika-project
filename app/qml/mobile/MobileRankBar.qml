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

    signal rankChosen(int rank)

    Layout.fillWidth: true
    implicitHeight: column.implicitHeight + AppMetrics.gapMedium * 2

    ColumnLayout
    {
        id: column

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: AppMetrics.contentPadding
        anchors.rightMargin: AppMetrics.contentPadding
        anchors.verticalCenter: parent.verticalCenter
        spacing: AppMetrics.gapSmall

        Text
        {
            Layout.fillWidth: true
            text: root.title
            font.pixelSize: AppMetrics.fontLabel
            font.weight: Font.Medium
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
            wrapMode: Text.WordWrap
        }

        Text
        {
            Layout.fillWidth: true
            text: ruler.valueText
            font.pixelSize: AppMetrics.fontMeta
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourSubtle
        }

        RankRuler
        {
            id: ruler

            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapSmall
            rank: root.rank
            maximumRank: root.maximumRank
            levelMarks: root.levelMarks
            tickFontSize: AppMetrics.fontMeta

            onRankChosen: function(chosen) { root.rankChosen(chosen) }
        }
    }

    Rectangle
    {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.contentPadding
        width: parent.width - AppMetrics.contentPadding
        height: AppMetrics.borderWidth
        visible: !root.lastRow
        color: AppTheme.colourBorder
    }
}
