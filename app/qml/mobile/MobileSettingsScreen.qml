import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Item
{
    id: root

    required property var store

    signal done()
    signal tabSelected(string key)
    signal placementRequested()

    readonly property string accountInitials:
    {
        const name = AppSyncController.accountName

        if (name.length > 0)
        {
            const words = name.split(" ").filter(function(word) { return word.length > 0 })

            if (words.length >= 2)
            {
                return (words[0][0] + words[1][0]).toUpperCase()
            }

            return words[0].substring(0, 2).toUpperCase()
        }

        const address = AppSyncController.accountEmail

        if (address.length === 0)
        {
            return "–"
        }

        const parts = address.split("@")[0].split(/[._\-]/).filter(function(part)
        {
            return part.length > 0
        })

        if (parts.length >= 2)
        {
            return (parts[0][0] + parts[1][0]).toUpperCase()
        }

        return parts[0].substring(0, 2).toUpperCase()
    }

    readonly property string accountDisplayName:
    {
        if (!AppSyncController.signedIn)
        {
            return qsTr("Not signed in")
        }

        if (AppSyncController.accountName.length > 0)
        {
            return AppSyncController.accountName
        }

        return AppSyncController.accountEmail.length > 0
            ? AppSyncController.accountEmail
            : qsTr("Signed in")
    }

    readonly property string accountEmailLine:
    {
        if (!AppSyncController.signedIn || AppSyncController.accountName.length === 0)
        {
            return ""
        }

        return AppSyncController.accountEmail
    }

    readonly property string accountSubtitle:
    {
        if (AppSyncController.signedIn)
        {
            return AppSyncController.statusText
        }

        return AppSyncController.configured
            ? qsTr("Sign in to keep your cards on every device")
            : qsTr("Sync is not set up on this build")
    }

    property string category: ""

    readonly property var categories: [
        { key: "general", title: qsTr("General"),
          description: qsTr("Theme, languages, exercises, dictionary") },
        { key: "training", title: qsTr("Training"),
          description: qsTr("Daily goal and stages") },
        { key: "review", title: qsTr("Review"),
          description: qsTr("Missed reviews and stages") },
        { key: "sound", title: qsTr("Sound"),
          description: qsTr("Speech and voice") },
        { key: "reader", title: qsTr("Reader"),
          description: qsTr("Page colour, text size, page turning") },
        { key: "reminders", title: qsTr("Reminders"),
          description: qsTr("Daily notification") },
        { key: "about", title: qsTr("About"),
          description: qsTr("Version and credits") }
    ].filter(Category => Qt.platform.os !== "ios" || (Category.key !== "reader" && Category.key !== "reminders"))

    property string languageCode: ""

    readonly property string categoryTitle:
    {
        if (root.languageCode.length > 0)
        {
            const language = AppCatalog.languages.find(function(entry) { return entry.code === root.languageCode })

            return language ? language.name : ""
        }

        if (root.category === "account")
        {
            return qsTr("Account")
        }

        const found = root.categories.find(function(entry) { return entry.key === root.category })

        return found ? found.title : qsTr("Settings")
    }

    property string shownCategory: ""
    property string shownLanguageCode: ""

    QtObject
    {
        id: pageRouter

        property string screen: "list"
        property int transitionDirection: 0
    }

    function openCategory(key)
    {
        root.shownCategory = key
        categoryScroller.contentY = 0

        pageRouter.transitionDirection = 1
        root.category = key
        pageRouter.screen = "category"
    }

    function openLanguage(code)
    {
        root.shownLanguageCode = code
        languageScroller.contentY = 0

        pageRouter.transitionDirection = 1
        root.languageCode = code
        pageRouter.screen = "language"
    }

    function closeCategory()
    {
        if (root.languageCode.length > 0)
        {
            pageRouter.transitionDirection = -1
            root.languageCode = ""
            pageRouter.screen = "category"

            return true
        }

        if (root.category.length === 0)
        {
            return false
        }

        pageRouter.transitionDirection = -1
        root.category = ""
        pageRouter.screen = "list"

        return true
    }

    function toggleAccountSignIn()
    {
        if (AppSyncController.busy || !AppSyncController.configured)
        {
            return
        }

        if (AppSyncController.signedIn)
        {
            signOutSheet.open()
        }
        else
        {
            AppSyncController.signIn()
        }
    }

    MobileShell
    {
        anchors.fill: parent

        activeTab: "settings"
        headerTitle: root.categoryTitle
        showBack: true
        showTabBar: false

        onTabSelected: function(key) { root.tabSelected(key) }
        onBackRequested: if (!root.closeCategory()) root.done()

        Item
        {
            anchors.fill: parent
            clip: true

            Flickable
            {
                id: listScroller

                anchors.fill: parent

                contentWidth: width
                contentHeight: listBody.implicitHeight + (AppMetrics.gapLarge * 2)
                clip: true

                ColumnLayout
                {
                    id: listBody

                    x: AppMetrics.contentPadding
                    y: AppMetrics.contentPadding
                    width: parent.width - (AppMetrics.contentPadding * 2)

                    spacing: AppMetrics.gapLarge

                    MobileSettingsGroup
                    {
                        title: qsTr("ACCOUNT")

                        MobileSettingsRow
                        {
                            title: root.accountDisplayName
                            description: root.accountEmailLine.length > 0 ? root.accountEmailLine : root.accountSubtitle
                            lastRow: true

                            onClicked: root.openCategory("account")

                            MobileSettingsChevron {}
                        }
                    }

                    MobileSettingsGroup
                    {
                        title: qsTr("PREFERENCES")

                        Repeater
                        {
                            model: root.categories

                            delegate: MobileSettingsRow
                            {
                                required property var modelData
                                required property int index

                                title: modelData.title
                                description: modelData.description
                                lastRow: index === root.categories.length - 1

                                onClicked: root.openCategory(modelData.key)

                                MobileSettingsChevron {}
                            }
                        }
                    }
                }
            }

            Flickable
            {
                id: categoryScroller

                anchors.fill: parent

                contentWidth: width
                contentHeight: categoryBody.implicitHeight + (AppMetrics.gapLarge * 2)
                clip: true

                ColumnLayout
                {
                    id: categoryBody

                    x: AppMetrics.contentPadding
                    y: AppMetrics.contentPadding
                    width: parent.width - (AppMetrics.contentPadding * 2)

                    spacing: AppMetrics.gapLarge

                    ColumnLayout
                    {
                        visible: root.shownCategory === "account"

                        Layout.fillWidth: true
                        spacing: AppMetrics.gapSmall


                        MobileCard
                        {
                            spacing: AppMetrics.gapMedium

                            RowLayout
                            {
                                Layout.fillWidth: true
                                spacing: AppMetrics.gapMedium

                                Rectangle
                                {
                                    Layout.preferredWidth: 48
                                    Layout.preferredHeight: 48

                                    radius: width / 2
                                    color: AppSyncController.signedIn
                                        ? AppTheme.colourAccent
                                        : AppTheme.colourFaint
                                    border.width: AppSyncController.signedIn ? 0 : AppMetrics.borderWidth
                                    border.color: AppTheme.colourBorder

                                    Text
                                    {
                                        anchors.centerIn: parent

                                        text: root.accountInitials
                                        color: AppSyncController.signedIn
                                            ? AppTheme.colourAccentText
                                            : AppTheme.colourMuted
                                        font.pixelSize: AppMetrics.fontLabel
                                        font.weight: Font.Bold
                                        font.family: AppMetrics.fontFamily
                                    }
                                }

                                ColumnLayout
                                {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text
                                    {
                                        Layout.fillWidth: true

                                        text: root.accountDisplayName
                                        font.pixelSize: AppMetrics.fontLabel
                                        font.weight: Font.Medium
                                        font.family: AppMetrics.fontFamily
                                        color: AppTheme.colourText
                                        elide: Text.ElideRight
                                    }

                                    Text
                                    {
                                        Layout.fillWidth: true
                                        visible: root.accountEmailLine.length > 0

                                        text: root.accountEmailLine
                                        font.pixelSize: AppMetrics.fontMeta
                                        font.family: AppMetrics.fontFamily
                                        color: AppTheme.colourMuted
                                        elide: Text.ElideRight
                                    }

                                    Text
                                    {
                                        Layout.fillWidth: true

                                        text: root.accountSubtitle
                                        font.pixelSize: AppMetrics.fontMeta
                                        font.family: AppMetrics.fontFamily
                                        color: AppTheme.colourSubtle
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }

                            MobileWideButton
                            {
                                visible: AppSyncController.needsReconnect
                                label: qsTr("Reconnect")
                                filled: true
                                interactive: !AppSyncController.busy

                                onClicked: AppSyncController.signIn()
                            }

                            MobileWideButton
                            {
                                visible: AppSyncController.signedIn
                                label: qsTr("Switch account")
                                filled: false
                                interactive: !AppSyncController.busy && AppSyncController.configured

                                onClicked: AppSyncController.switchAccount()
                            }

                            MobileWideButton
                            {
                                visible: AppSyncController.signedIn && !AppSyncController.needsReconnect
                                label: AppSyncController.syncing ? qsTr("Syncing…") : qsTr("Sync now")
                                filled: false
                                interactive: !AppSyncController.syncing

                                onClicked: AppSyncController.syncNow()
                            }

                            MobileWideButton
                            {
                                label: AppSyncController.signedIn
                                    ? qsTr("Sign out")
                                    : qsTr("Sign in with Google")
                                filled: !AppSyncController.signedIn
                                destructive: AppSyncController.signedIn
                                interactive: !AppSyncController.busy && AppSyncController.configured

                                onClicked: root.toggleAccountSignIn()
                            }
                        }
                    }

                    MobileSettingsLanguages
                    {
                        id: accountLanguages

                        visible: root.shownCategory === "account"

                        store: root.store

                        onLanguageOpened: function(code) { root.openLanguage(code) }
                        onPlacementRequested: root.placementRequested()
                    }

                    MobileSettingsData
                    {
                        visible: root.shownCategory === "account"

                        exportFile: exportDialog
                        importFile: importDialog
                        eraseSheet: eraseAccountDialog
                    }

                    MobileSettingsGeneral
                    {
                        visible: root.shownCategory === "general"

                        store: root.store
                        optionPicker: picker
                        languageSheet: languageDialog
                        updateSheet: dictionaryUpdateSheet
                    }

                    MobileSettingsReminders
                    {
                        visible: root.shownCategory === "reminders"

                        timeSheet: timePicker
                    }

                    MobileSettingsSessionStages
                    {
                        visible: root.shownCategory === "training"

                        store: root.store
                        session: root.store.training
                        optionPicker: picker
                    }

                    MobileSettingsSessionStages
                    {
                        visible: root.shownCategory === "review"

                        store: root.store
                        session: root.store.review
                        optionPicker: picker
                    }

                    MobileSettingsSound
                    {
                        visible: root.shownCategory === "sound"

                        store: root.store
                    }

                    MobileSettingsReader
                    {
                        visible: root.shownCategory === "reader"

                        optionPicker: picker
                    }

                    MobileSettingsAbout
                    {
                        visible: root.shownCategory === "about"
                    }
                }
            }

            Flickable
            {
                id: languageScroller

                anchors.fill: parent

                contentWidth: width
                contentHeight: languageBody.implicitHeight + (AppMetrics.gapLarge * 2)
                clip: true

                MobileSettingsLanguage
                {
                    id: languageBody

                    x: AppMetrics.contentPadding
                    y: AppMetrics.contentPadding
                    width: parent.width - (AppMetrics.contentPadding * 2)

                    store: root.store
                    languages: accountLanguages
                    languageSheet: languageDialog
                    deleteLanguageSheet: deleteLanguageDialog
                    languageCode: root.shownLanguageCode

                    onPlacementRequested: root.placementRequested()
                }
            }

            MobileScreenMotion { target: listScroller; screenName: "list"; router: pageRouter }
            MobileScreenMotion { target: categoryScroller; screenName: "category"; router: pageRouter }
            MobileScreenMotion { target: languageScroller; screenName: "language"; router: pageRouter }
        }
    }


    MobileSettingsPicker
    {
        id: picker

        anchors.fill: parent
        z: 50
    }

    TimePickerSheet
    {
        id: timePicker

        anchors.fill: parent
        z: 70
    }

    MobileDeleteDataSheet
    {
        id: eraseAccountDialog

        anchors.fill: parent
        z: 70
    }

    MobileSignOutSheet
    {
        id: signOutSheet

        anchors.fill: parent
        z: 70
    }

    MobileDeleteLanguageSheet
    {
        id: deleteLanguageDialog

        anchors.fill: parent
        z: 70
    }

    MobileSyncResultSheet
    {
        id: syncResultSheet

        anchors.fill: parent
        z: 70
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

        onAccepted: AppSyncController.importDatabase(importDialog.selectedFile)
    }

    Connections
    {
        target: AppSyncController

        function onExportFinished(success, message)
        {
            if (message.length > 0)
            {
                syncResultSheet.show(message)
            }
        }

        function onImportFinished(success, message)
        {
            syncResultSheet.show(message)
        }

        function onLanguageDeleted(success, message)
        {
            if (!success)
            {
                syncResultSheet.show(message)
            }
        }
    }

    MobileDictionaryUpdateSheet
    {
        id: dictionaryUpdateSheet

        anchors.fill: parent
        z: 70
    }

    MobileLanguageSheet
    {
        id: languageDialog

        anchors.fill: parent
        z: 60

        store: root.store
    }
}
