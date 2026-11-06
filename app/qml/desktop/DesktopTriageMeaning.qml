import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle
{
    id: root

    required property var meaning
    property var translations: []
    property var picked: []
    property bool expanded: true
    property int highlightedTranslation: -1
    property int stageCount: 1

    signal translationPicked(string text)
    signal expandRequested()
    signal wordClicked(string word, string sentence)

    readonly property bool learned: root.meaning.learnedStage >= 0
    readonly property bool chosen: root.picked.length > 0
    readonly property bool hovered: !root.expanded && hoverHandler.hovered

    readonly property real targetHeight: (root.expanded ? content.implicitHeight : summary.implicitHeight) + AppMetrics.gapMedium * 2

    implicitHeight: root.targetHeight
    clip: true

    radius: AppMetrics.radiusLarge
    color: root.hovered ? AppTheme.colourSunken : AppTheme.colourPanel
    border.width: AppMetrics.borderWidth
    border.color: root.chosen ? AppTheme.colourAccent : (root.hovered ? AppTheme.colourOutline : AppTheme.colourBorder)
    opacity: root.learned ? AppTheme.opacitySecondary : 1.0

    Accessible.role: root.expanded ? Accessible.Grouping : Accessible.Button
    Accessible.name: qsTr("%1: %2").arg(root.meaning.gist).arg(root.meaning.nativeDefinition)
    Accessible.onPressAction: root.expandRequested()

    Behavior on border.color { ColorAnimation { duration: AppMetrics.motionFast } }
    Behavior on implicitHeight { NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing } }

    HoverHandler
    {
        id: hoverHandler

        enabled: !root.expanded
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler
    {
        enabled: !root.expanded

        onTapped: root.expandRequested()
    }

    DesktopMeaningSummary
    {
        id: summary

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: AppMetrics.gapMedium
        visible: opacity > 0
        opacity: root.expanded ? 0 : 1

        meaning: root.meaning

        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }

        picked: root.picked
        learned: root.learned
        stageCount: root.stageCount
    }

    ColumnLayout
    {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: AppMetrics.gapMedium
        visible: opacity > 0
        opacity: root.expanded ? 1 : 0
        spacing: AppMetrics.gapSmall

        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            DesktopGistBadge
            {
                id: gistBadge

                Layout.alignment: Qt.AlignTop
                label: root.meaning.gist
            }

            DesktopLabel
            {
                id: definitionLabel

                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: Math.max(0, (gistBadge.implicitHeight - definitionMetrics.height) / 2)
                visible: text.length > 0
                text: root.meaning.nativeDefinition || ""
                font.pixelSize: AppMetrics.fontLabel
                wrapMode: Text.WordWrap

                FontMetrics
                {
                    id: definitionMetrics

                    font: definitionLabel.font
                }
            }

            DesktopProgressRing
            {
                Layout.alignment: Qt.AlignTop
                visible: root.learned
                stage: root.meaning.learnedStage
                stageCount: root.stageCount
                completed: !!root.meaning.completed
            }
        }

        DesktopMeaningDetails
        {
            Layout.fillWidth: true
            meaning: root.meaning
            showDefinition: false
            showNativeDefinition: false

            onWordClicked: function(word, sentence) { root.wordClicked(word, sentence) }
        }

        Item
        {
            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapSmall
            implicitHeight: chipGrid.implicitHeight

            Grid
            {
                id: chipGrid

                readonly property real cellWidth: (width - columnSpacing * (columns - 1)) / columns

                width: parent.width
                columns: 3
                columnSpacing: AppMetrics.gapSmall
                rowSpacing: AppMetrics.gapSmall

                Repeater
                {
                    model: root.translations

                    delegate: DesktopTranslationChip
                    {
                        required property var modelData
                        required property int index

                        objectName: "triage_chip_" + modelData.text

                        width: chipGrid.cellWidth
                        implicitHeight: AppMetrics.buttonHeight
                        text: modelData.text
                        relative: modelData.relative
                        hasProbability: modelData.hasProbability
                        learnedStage: modelData.learnedStage
                        inUse: root.picked.indexOf(modelData.text.trim().toLowerCase()) >= 0
                        accentInUse: true
                        keyHighlighted: index === root.highlightedTranslation
                        enabled: !root.learned
                        focusPolicy: Qt.NoFocus

                        ToolTip.text: inUse ? "" : qsTr("Use this translation")

                        onClicked: root.translationPicked(modelData.text)
                    }
                }
            }
        }
    }
}
