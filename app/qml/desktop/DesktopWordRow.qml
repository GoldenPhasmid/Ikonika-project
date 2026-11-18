import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ItemDelegate
{
    id: root

    property string word: ""
    property string translation: ""
    property string partOfSpeech: ""
    property int gender: -1
    property string verbGroup: ""
    property string cefrLevel: ""
    property int stage: -1
    property bool completed: false
    property int maxStage: 1
    property bool selected: false
    property bool italicPartOfSpeech: true

    readonly property bool learning: root.stage >= 0 || root.completed

    width: ListView.view ? ListView.view.rowWidth : implicitWidth
    implicitHeight: AppMetrics.listRowHeight

    leftPadding: AppMetrics.gapMedium
    rightPadding: AppMetrics.gapSmall
    topPadding: 0
    bottomPadding: 0

    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    highlighted: root.selected

    text: root.word
    Accessible.role: Accessible.ListItem
    Accessible.name: root.translation.length > 0 ? qsTr("%1, %2").arg(root.word).arg(root.translation) : root.word
    Accessible.selected: root.selected

    contentItem: RowLayout
    {
        spacing: AppMetrics.gapCompact

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: 0

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.word
                color: AppTheme.verbGroupColours[root.verbGroup] ?? AppTheme.genderColours[root.gender] ?? AppTheme.colourText
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: root.translation.length > 0
                text: root.translation
                font.pixelSize: AppMetrics.fontBody
                color: AppTheme.colourText
                elide: Text.ElideRight
            }
        }

        DesktopLabel
        {
            visible: root.partOfSpeech.length > 0
            text: root.partOfSpeech
            font.pixelSize: AppMetrics.fontSmall
            font.italic: root.italicPartOfSpeech
            color: AppTheme.colourSubtle
        }

        Item
        {
            visible: root.learning
            implicitWidth: AppMetrics.progressRingSize
            implicitHeight: AppMetrics.progressRingSize

            DesktopProgressRing
            {
                anchors.centerIn: parent
                visible: root.learning
                stage: root.stage
                stageCount: root.maxStage
                completed: root.completed
            }
        }

        DesktopLevelBadge
        {
            visible: root.cefrLevel.length > 0
            label: root.cefrLevel
            fill: AppTheme.cefrColours[label] || AppTheme.colourBorder
        }
    }

    background: DesktopSelectionBackground
    {
        selected: root.selected
        hovered: root.hovered
        pressed: root.down
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
