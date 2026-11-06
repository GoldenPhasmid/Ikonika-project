import QtQuick

Column
{
    id: root

    property var forms: []
    property int fontSize: AppMetrics.sessionTranslationSize

    readonly property var titles: [qsTr("Singular"), qsTr("Plural")]

    readonly property var cells:
    {
        const listed = []

        for (const row of root.forms)
        {
            for (const cell of row.cells)
            {
                listed.push(cell)
            }
        }

        return listed
    }

    readonly property string widestCue: root.cells.reduce(function(widest, cell)
    {
        return cell.cue.length > widest.length ? cell.cue : widest
    }, "")

    readonly property string widestForm: root.cells.reduce(function(widest, cell)
    {
        const plain = cell.form.replace(/<[^>]+>/g, "")

        return plain.length > widest.length ? plain : widest
    }, "")

    readonly property real cueWidth: root.widestCue.length > 0 ? cueMetrics.advanceWidth + AppMetrics.gapSmall : 0

    readonly property real cellWidth: Math.max((root.width - AppMetrics.gapMedium) / 2, 0)
    readonly property real formWidth: Math.max(root.cellWidth - root.cueWidth, 0)

    readonly property int fittedFontSize: root.formWidth > 0 && formMetrics.advanceWidth > root.formWidth
        ? Math.floor(root.fontSize * root.formWidth / formMetrics.advanceWidth * 0.96)
        : root.fontSize

    function cellLeft(index)
    {
        return index * (root.cellWidth + AppMetrics.gapMedium)
    }

    spacing: 0

    TextMetrics
    {
        id: cueMetrics

        text: root.widestCue
        font.pixelSize: AppMetrics.fontLabel
        font.family: AppMetrics.fontFamily
    }

    TextMetrics
    {
        id: formMetrics

        text: root.widestForm
        font.pixelSize: root.fontSize
        font.weight: Font.Medium
        font.family: AppMetrics.fontFamily
    }

    Item
    {
        width: root.width
        height: childrenRect.height + AppMetrics.gapSmall

        Repeater
        {
            model: root.titles

            delegate: MobileSectionLabel
            {
                required property string modelData
                required property int index

                x: root.cellLeft(index)
                text: modelData.toUpperCase()
            }
        }
    }

    Repeater
    {
        model: root.forms

        delegate: Item
        {
            id: tableRow

            required property var modelData

            width: root.width
            height: formMetrics.height * root.fittedFontSize / root.fontSize + AppMetrics.gapSmall

            Repeater
            {
                model: tableRow.modelData.cells

                delegate: Item
                {
                    required property var modelData
                    required property int index

                    x: root.cellLeft(index)
                    width: root.cellWidth
                    height: tableRow.height

                    Text
                    {
                        anchors.baseline: formText.baseline
                        text: modelData.cue
                        font.pixelSize: AppMetrics.fontLabel
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourMuted
                    }

                    Text
                    {
                        id: formText

                        x: root.cueWidth
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.form
                        textFormat: Text.StyledText
                        linkColor: AppTheme.colourAccent
                        font.pixelSize: root.fittedFontSize
                        font.weight: Font.Medium
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourText
                    }
                }
            }
        }
    }
}
