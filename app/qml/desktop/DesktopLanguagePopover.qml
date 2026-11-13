import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

DesktopPopover
{
    id: root

    required property var store
    property bool installedOnly: false

    heading: qsTr("I am learning")

    onAboutToShow: AppCatalog.Refresh()

    DesktopLabel
    {
        Layout.fillWidth: true
        Layout.margins: AppMetrics.gapCompact
        visible: !root.installedOnly && AppCatalog.languages.length === 0
        text: AppCatalog.busy ? qsTr("Looking for languages...") : qsTr("Ikonika cannot reach the language list.")
        font.pixelSize: AppMetrics.fontSmall
        color: AppTheme.colourMuted
        wrapMode: Text.WordWrap
    }

    Repeater
    {
        model: root.installedOnly ? AppCatalog.languages.filter(Entry => Entry.installed) : AppCatalog.languages

        delegate: DesktopPopoverRow
        {
            id: languageRow

            required property var modelData

            readonly property DesktopLanguageEntry language: DesktopLanguageEntry
            {
                entry: languageRow.modelData
                store: root.store
            }

            objectName: "language_" + languageRow.modelData.code
            text: languageRow.modelData.name
            flagCode: languageRow.modelData.code
            description: languageRow.language.status
            showsCheck: true
            selected: languageRow.language.active
            enabled: languageRow.modelData.compatible && !languageRow.language.fetching

            onClicked:
            {
                languageRow.language.act()

                if (languageRow.modelData.installed)
                {
                    root.close()
                }
            }
        }
    }
}
