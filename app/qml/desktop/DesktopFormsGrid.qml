import QtQuick
import QtQuick.Layouts
import "../FormsMarkup.js" as FormsMarkup

GridLayout
{
    id: root

    property var forms: []
    property bool speakable: false

    signal formClicked(string form)

    property var titles: [qsTr("Singular"), qsTr("Plural")]

    readonly property var cells:
    {
        const listed = []
        const placed = root.titles.map(() => 0)

        for (const row of root.forms)
        {
            row.cells.forEach(function(cell, column)
            {
                if (cell.form.length === 0)
                {
                    return
                }

                listed.push({ label: cell.cue, form: cell.form, matched: !!cell.matched, column: column, row: placed[column] + 1 })
                ++placed[column]
            })
        }

        return listed
    }

    readonly property string widestLabel: root.cells.reduce(function(widest, cell)
    {
        return cell.label.length > widest.length ? cell.label : widest
    }, "")

    columns: root.titles.length
    rowSpacing: AppMetrics.gapSmall
    columnSpacing: AppMetrics.gapMedium

    TextMetrics
    {
        id: labelMetrics

        text: root.widestLabel
        font.pixelSize: AppMetrics.fontSmall
        font.family: AppMetrics.fontFamily
    }

    Repeater
    {
        model: root.titles

        delegate: DesktopSectionLabel
        {
            required property string modelData
            required property int index

            visible: root.cells.some(cell => cell.column === index)
            Layout.row: 0
            Layout.column: index
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.leftMargin: AppMetrics.gapMedium
            text: modelData
        }
    }

    Repeater
    {
        model: root.cells

        delegate: Rectangle
        {
            id: formCell

            required property var modelData

            readonly property bool lifted: root.speakable && cellArea.containsMouse

            Layout.row: modelData.row
            Layout.column: modelData.column
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.preferredHeight: AppMetrics.formCellHeight

            radius: AppMetrics.radiusMedium
            color: modelData.matched ? Qt.tint(AppTheme.colourWell, AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaSoft))
                : (formCell.lifted ? AppTheme.colourHover : AppTheme.colourWell)
            border.width: AppMetrics.borderWidth
            border.color: modelData.matched ? AppTheme.colourAccent : (formCell.lifted ? AppTheme.colourOutline : AppTheme.colourBorder)

            Accessible.role: root.speakable ? Accessible.Button : Accessible.StaticText
            Accessible.name: (formCell.modelData.label + " " + formCell.modelData.form.replace(/<[^>]+>/g, "")).trim()

            MouseArea
            {
                id: cellArea

                z: 1
                anchors.fill: parent
                hoverEnabled: true
                enabled: root.speakable
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

                onClicked: root.formClicked(formCell.modelData.form)
            }

            RowLayout
            {
                anchors.fill: parent
                anchors.leftMargin: AppMetrics.gapMedium
                anchors.rightMargin: AppMetrics.gapMedium
                spacing: AppMetrics.gapCompact

                DesktopLabel
                {
                    visible: root.widestLabel.length > 0
                    Layout.preferredWidth: labelMetrics.advanceWidth
                    text: formCell.modelData.label
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted

                    Accessible.ignored: true
                }

                DesktopLabel
                {
                    Layout.fillWidth: true
                    text: FormsMarkup.colourEndings(formCell.modelData.form, AppTheme.colourAccent)
                    textFormat: Text.StyledText
                    color: AppTheme.colourText
                    font.pixelSize: AppMetrics.fontLead
                    font.weight: Font.Medium
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: AppMetrics.fontSmall
                    elide: Text.ElideRight

                    Accessible.ignored: true
                }
            }
        }
    }
}
