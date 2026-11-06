import QtQuick
import QtQuick.Layouts
import "../FormsMarkup.js" as FormsMarkup

GridLayout
{
    id: root

    property var forms: []

    readonly property var titles: [qsTr("Singular"), qsTr("Plural")]

    readonly property var cells:
    {
        const listed = []

        for (const row of root.forms)
        {
            for (const cell of row.cells)
            {
                listed.push({ label: cell.cue, form: cell.form, matched: !!cell.matched })
            }
        }

        return listed
    }

    readonly property string widestLabel: root.cells.reduce(function(widest, cell)
    {
        return cell.label.length > widest.length ? cell.label : widest
    }, "")

    columns: 2
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

            readonly property bool missing: formCell.modelData.form.length === 0

            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.preferredHeight: AppMetrics.formCellHeight

            radius: AppMetrics.radiusMedium
            color: modelData.matched ? Qt.tint(AppTheme.colourWell, AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaSoft)) : AppTheme.colourWell
            border.width: AppMetrics.borderWidth
            border.color: modelData.matched ? AppTheme.colourAccent : AppTheme.colourBorder

            Accessible.role: Accessible.StaticText
            Accessible.name: (formCell.modelData.label + " " + formCell.modelData.form.replace(/<[^>]+>/g, "")).trim()

            RowLayout
            {
                anchors.fill: parent
                anchors.leftMargin: AppMetrics.gapMedium
                anchors.rightMargin: AppMetrics.gapMedium
                spacing: AppMetrics.gapCompact

                DesktopLabel
                {
                    visible: root.widestLabel.length > 0 && !formCell.missing
                    Layout.preferredWidth: labelMetrics.advanceWidth
                    text: formCell.modelData.label
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted

                    Accessible.ignored: true
                }

                DesktopLabel
                {
                    Layout.fillWidth: true
                    text: formCell.missing ? "—" : FormsMarkup.colourEndings(formCell.modelData.form, AppTheme.colourAccent)
                    textFormat: Text.StyledText
                    horizontalAlignment: formCell.missing ? Text.AlignHCenter : Text.AlignLeft
                    color: formCell.missing ? AppTheme.colourFaint : AppTheme.colourText
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
