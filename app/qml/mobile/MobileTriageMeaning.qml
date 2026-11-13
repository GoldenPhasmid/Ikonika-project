import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property var meaning
    property var translations: []
    property var picked: []
    property bool expanded: false
    property int stageCount: 1
    property int chipRows: 0
    property bool animated: true

    signal translationPicked(string text)
    signal expandRequested()
    signal wordTapped(string word, string sentence)

    readonly property bool learned: root.meaning.learnedStage >= 0
    readonly property bool chosen: root.picked.length > 0

    readonly property real targetHeight: (root.expanded ? content.implicitHeight : summary.implicitHeight) + AppMetrics.gapMedium * 2

    Layout.fillWidth: true
    implicitHeight: root.targetHeight
    clip: true

    radius: AppMetrics.radiusMedium
    color: AppTheme.colourPanel
    border.width: AppMetrics.borderWidth
    border.color: root.chosen ? AppTheme.colourAccent : AppTheme.colourBorder
    opacity: root.learned ? AppTheme.opacitySecondary : 1.0

    Behavior on border.color { ColorAnimation { duration: AppMetrics.motionFast } }
    Behavior on implicitHeight
    {
        enabled: root.animated

        NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
    }

    TapHandler
    {
        enabled: !root.expanded

        onTapped: root.expandRequested()
    }

    MobileMeaningSummary
    {
        id: summary

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: AppMetrics.gapMedium
        visible: opacity > 0
        opacity: root.expanded ? 0 : 1

        meaning: root.meaning
        picked: root.picked
        stageCount: root.stageCount

        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
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

            MobileGistTag
            {
                id: openGistTag

                Layout.alignment: Qt.AlignTop
                text: root.meaning.gist
            }

            Text
            {
                id: openDefinition

                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: Math.max(0, (openGistTag.implicitHeight - openDefinitionMetrics.height) / 2)
                text: root.meaning.nativeDefinition || ""
                font.pixelSize: AppMetrics.fontBody
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
                wrapMode: Text.WordWrap

                FontMetrics
                {
                    id: openDefinitionMetrics

                    font: openDefinition.font
                }
            }

            MobileProgressRing
            {
                Layout.alignment: Qt.AlignTop
                visible: root.learned
                stage: root.meaning.learnedStage
                stageCount: root.stageCount
                completed: !!root.meaning.completed
            }
        }

        MobileMeaningDetails
        {
            Layout.fillWidth: true
            meaning: root.meaning
            showDefinition: false
            showNativeDefinition: false

            onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
        }

        Flickable
        {
            id: chipArea

            readonly property real rowsHeight: translationGrid.chipHeight * root.chipRows
                                               + translationGrid.rowSpacing * (root.chipRows - 1)

            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapTight
            Layout.preferredHeight: root.chipRows > 0 && translationGrid.chipHeight > 0
                                    ? Math.min(translationGrid.implicitHeight, chipArea.rowsHeight)
                                    : translationGrid.implicitHeight
            contentWidth: width
            contentHeight: translationGrid.implicitHeight
            interactive: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds
            clip: root.chipRows > 0

            GridLayout
            {
                id: translationGrid

                readonly property real halfWidth: (translationGrid.width - translationGrid.columnSpacing) / 2
                property real chipHeight: 0

                function arrange()
                {
                    let nextRow = 0
                    let tallest = 0
                    let openRow = -1
                    let openChip = null

                    for (let index = 0; index < translationRepeater.count; ++index)
                    {
                        const chip = translationRepeater.itemAt(index)

                        if (!chip)
                        {
                            continue
                        }

                        tallest = Math.max(tallest, chip.implicitHeight, chip.Layout.preferredHeight)

                        if (chip.naturalWidth > translationGrid.halfWidth)
                        {
                            chip.Layout.row = nextRow
                            chip.Layout.column = 0
                            chip.Layout.columnSpan = 2
                            nextRow += 1
                        }
                        else if (openRow >= 0)
                        {
                            chip.Layout.row = openRow
                            chip.Layout.column = 1
                            chip.Layout.columnSpan = 1
                            openChip.Layout.columnSpan = 1
                            openRow = -1
                            openChip = null
                        }
                        else
                        {
                            chip.Layout.row = nextRow
                            chip.Layout.column = 0
                            chip.Layout.columnSpan = 2
                            openRow = nextRow
                            openChip = chip
                            nextRow += 1
                        }
                    }

                    translationGrid.chipHeight = tallest
                }

                width: chipArea.width
                columns: 2
                columnSpacing: AppMetrics.chipListSpacing
                rowSpacing: AppMetrics.chipListSpacing

                onHalfWidthChanged: translationGrid.arrange()

                Repeater
                {
                    id: translationRepeater

                    model: root.translations

                    onItemAdded: Qt.callLater(translationGrid.arrange)

                    delegate: MobileTranslationChip
                    {
                        required property var modelData

                        Layout.preferredWidth: 1
                        text: modelData.text
                        relative: modelData.relative
                        hasProbability: modelData.hasProbability
                        learnedStage: modelData.learnedStage
                        inUse: root.picked.indexOf(modelData.text.trim().toLowerCase()) >= 0
                        enabled: !root.learned

                        onNaturalWidthChanged: Qt.callLater(translationGrid.arrange)
                        onPicked: root.translationPicked(modelData.text)
                    }
                }
            }
        }
    }
}
