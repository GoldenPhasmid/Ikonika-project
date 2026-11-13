import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store
    property var triage: null

    signal placementRequested()

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    property var wordCounts: ({})

    function refresh()
    {
        const counts = {}

        for (const entry of AppCatalog.languages)
        {
            counts[entry.code] = AppSyncController.countLanguageWords(entry.code)
        }

        root.wordCounts = counts
    }

    function wordCount(code)
    {
        return root.wordCounts[code] || 0
    }

    function askToDelete(entry)
    {
        const count = root.wordCount(entry.code)

        deleteDialog.ask(qsTr("Delete your %1 words?").arg(entry.name),
                         qsTr("%n words and their progress are deleted here and on every device that syncs with your Google account. This cannot be undone.", "", count),
                         qsTr("Delete"),
                         () => AppSyncController.deleteLanguage(entry.code))
    }

    Connections
    {
        target: AppCatalog

        function onCatalogChanged()
        {
            root.refresh()
        }
    }

    Connections
    {
        target: AppSyncController

        function onLocalDataChanged()
        {
            root.refresh()
        }
    }

    DesktopAccountProfile
    {
        onSignOutRequested: deleteDialog.ask(qsTr("Sign out?"),
                                             qsTr("Your words and progress stay in your Google account. Sign in again to pick up where you left off."),
                                             qsTr("Sign out"),
                                             () => AppSyncController.signOut())

        onEraseRequested: eraseDialog.openFresh()
    }

    DesktopLanguageDownloads
    {
        store: root.store
        words: root.wordCount(root.store.targetLanguageCode)
        triage: root.triage
        onPlacementRequested: root.placementRequested()
        onDeleteRequested: root.askToDelete({code: root.store.targetLanguageCode, name: root.store.targetLanguageName})
    }

    ColumnLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        DesktopSectionLabel
        {
            text: qsTr("Other languages")
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            visible: AppCatalog.languages.length === 0
            text: AppCatalog.busy ? qsTr("Looking for languages...") : qsTr("Ikonika cannot reach the language list.")
            color: AppTheme.colourMuted
            wrapMode: Text.WordWrap
        }

        GridLayout
        {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: AppMetrics.gapMedium
            rowSpacing: AppMetrics.gapMedium

            Repeater
            {
                model: AppCatalog.languages.filter(entry => entry.code !== root.store.targetLanguageCode)

                delegate: DesktopLanguageTile
                {
                    required property var modelData

                    objectName: "account_language_" + modelData.code

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    entry: modelData
                    store: root.store
                    words: root.wordCount(modelData.code)

                }
            }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Your data")

        DesktopSettingsRow
        {
            text: qsTr("Export database")
            clickable: true

            onClicked:
            {
                exportDialog.selectedFile = "file:///ikonika-backup.db"
                exportDialog.open()
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Import database")
            clickable: true
            lastRow: true

            onClicked: importDialog.open()
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Remove")

        DesktopSettingsRow
        {
            objectName: "account_uninstall"

            visible: AppUpdater.canUninstall
            text: qsTr("Uninstall Ikonika")

            DesktopButton
            {
                tone: "dangerOutline"
                text: qsTr("Uninstall")

                onClicked: AppUpdater.Uninstall()
            }
        }

        DesktopSettingsRow
        {
            objectName: "account_delete_everything"

            text: qsTr("Delete everything")
            description: qsTr("Every language, here and in your Google account")
            lastRow: true

            DesktopButton
            {
                tone: "dangerOutline"
                text: qsTr("Delete")

                onClicked: eraseDialog.openFresh()
            }
        }
    }

    FileDialog
    {
        id: exportDialog

        title: qsTr("Save the Ikonika database")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Ikonika database (*.db)")]
        defaultSuffix: "db"

        onAccepted: AppSyncController.exportDatabase(exportDialog.selectedFile)
    }

    FileDialog
    {
        id: importDialog

        title: qsTr("Choose an Ikonika database")
        nameFilters: [qsTr("Ikonika database (*.db)")]

        onAccepted: AppSyncController.importDatabase(importDialog.selectedFile)
    }

    DesktopConfirmDialog
    {
        id: deleteDialog

        objectName: "account_delete_dialog"
    }

    DesktopDeleteDataDialog
    {
        id: eraseDialog
    }
}
