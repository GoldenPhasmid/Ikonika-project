import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    required property var store

    readonly property int rowHeight: 60

    heading: qsTr("Languages")

    visible: false

    onDismissed: root.close()

    function open()
    {
        AppCatalog.Refresh()
        root.present()
    }

    function close()
    {
        root.hide()
    }

    function megabytes(bytes)
    {
        return Math.round(bytes / (1024 * 1024))
    }

    function act(entry)
    {
        if (!entry.compatible)
        {
            return
        }

        if (entry.installed)
        {
            root.store.selectTargetLanguage(entry.code)
            root.close()

            return
        }

        if (entry.outdated)
        {
            AppPackageDownloader.download(entry.id)

            return
        }

        AppOnboarding.choose(entry.code)
    }

    Flickable
    {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(panel.implicitHeight, 420)

        contentWidth: width
        contentHeight: panel.implicitHeight
        clip: true

        MobileOptionListPanel
        {
            id: panel

            width: parent.width

            Text
            {
                Layout.fillWidth: true
                Layout.preferredHeight: root.rowHeight

                visible: AppCatalog.languages.length === 0

                text: AppCatalog.busy
                    ? qsTr("Looking for languages...")
                    : qsTr("Ikonika cannot reach the language list.")
                color: AppTheme.colourMuted
                font.pixelSize: AppMetrics.fontSmall
                font.family: AppMetrics.fontFamily
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            Repeater
            {
                model: AppCatalog.languages

                delegate: MobileSettingsRow
                {
                            id: languageRow

                            required property var modelData
                            required property int index

                            readonly property bool active:
                                languageRow.modelData.code === root.store.targetLanguageCode

                            readonly property bool downloading: languageRow.modelData.id.length > 0
                                && (AppPackageDownloader.activeId === languageRow.modelData.id
                                    || AppPackageDownloader.queue.indexOf(languageRow.modelData.id) >= 0)

                            readonly property bool fetching:
                                (languageRow.downloading && !languageRow.modelData.installed)
                                || (AppOnboarding.chosenLanguage === languageRow.modelData.code
                                    && !(AppOnboarding.dictionaryReady && AppOnboarding.voiceReady))

                            readonly property real fetchProgress:
                                AppPackageDownloader.totalBytes > 0
                                    ? AppPackageDownloader.receivedBytes / AppPackageDownloader.totalBytes
                                    : 0

                            title: languageRow.modelData.name
                            description:
                            {
                                if (!languageRow.modelData.compatible)
                                {
                                    return languageRow.modelData.outdated
                                        ? qsTr("update not available yet")
                                        : qsTr("needs a newer app version")
                                }

                                return languageRow.modelData.outdated ? qsTr("update") : ""
                            }
                            lastRow: languageRow.index === AppCatalog.languages.length - 1

                            onClicked: root.act(languageRow.modelData)

                            Text
                            {
                                visible: languageRow.active

                                text: "✓"
                                font.pixelSize: AppMetrics.fontLabel
                                font.family: AppMetrics.fontFamily
                                color: AppTheme.colourText
                            }

                            Text
                            {
                                visible: !languageRow.active && !languageRow.fetching
                                    && languageRow.modelData.compatible
                                    && !languageRow.modelData.installed

                                text: qsTr("%1 MB").arg(root.megabytes(languageRow.modelData.bytes))
                                font.pixelSize: AppMetrics.fontMeta
                                font.family: AppMetrics.fontFamily
                                color: AppTheme.colourSubtle
                            }

                            ColumnLayout
                            {
                                visible: languageRow.fetching

                                width: 96
                                spacing: AppMetrics.gapTight

                                Rectangle
                                {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 6

                                    radius: 3
                                    color: AppTheme.colourFaint

                                    Rectangle
                                    {
                                        width: parent.width * languageRow.fetchProgress
                                        height: parent.height

                                        radius: 3
                                        color: AppTheme.colourAccent
                                    }
                                }

                                Text
                                {
                                    Layout.fillWidth: true

                                    text: Math.round(languageRow.fetchProgress * 100) + "%"
                                    font.pixelSize: AppMetrics.fontMicro
                                    font.family: AppMetrics.fontFamily
                                    color: AppTheme.colourMuted
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }
                }
            }
        }

