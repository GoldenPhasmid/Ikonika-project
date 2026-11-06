import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    required property var entry
    required property var store
    property int words: 0

    readonly property DesktopLanguageEntry language: DesktopLanguageEntry
    {
        entry: root.entry
        store: root.store
    }

    readonly property string detail: root.language.status.length > 0 ? root.language.status
        : qsTr("Switch · %n words", "", root.words)

    enabled: root.language.actionable
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    padding: AppMetrics.gapMedium
    implicitHeight: contentItem.implicitHeight + padding * 2
    text: root.entry.name
    Accessible.name: root.entry.installed ? qsTr("Switch to %1").arg(root.entry.name)
        : qsTr("Download %1 dictionary").arg(root.entry.name)
    Accessible.description: root.detail

    onClicked: root.language.act()

    contentItem: RowLayout
    {
        spacing: AppMetrics.gapSmall

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: AppMetrics.gapTight

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.entry.name
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.detail
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
                elide: Text.ElideRight
            }
        }

        DesktopIcon
        {
            name: root.entry.installed ? "chevron_right" : "arrow_down"
            Layout.preferredWidth: AppMetrics.iconSizeSmall
            Layout.preferredHeight: AppMetrics.iconSizeSmall
            color: AppTheme.colourMuted
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: root.down ? AppTheme.colourSunken : root.hovered ? AppTheme.colourFaint : AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: AppMetrics.radiusMedium
        }
    }
}
