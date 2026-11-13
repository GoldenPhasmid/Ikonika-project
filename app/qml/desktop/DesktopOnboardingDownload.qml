import QtQuick
import QtQuick.Layouts

DesktopFormPage
{
    id: root

    required property var store

    signal done()

    readonly property string chosenLanguage: AppOnboarding.chosenLanguage

    readonly property string chosenName:
    {
        const found = AppCatalog.languages.find(Language => Language.code === root.chosenLanguage)

        return found ? found.name : root.chosenLanguage
    }

    readonly property bool stalled: !AppPackageDownloader.busy
        && AppPackageDownloader.queue.length === 0
        && !AppOnboarding.dictionaryReady

    readonly property var kindNames: ({
        "dictionary": qsTr("Dictionary"),
        "voice": qsTr("Voice"),
        "translation": qsTr("Sentence translation")
    })

    readonly property int retryInterval: 15000

    readonly property var items:
    {
        const result = []

        for (const entry of AppCatalog.packages)
        {
            if (!entry.compatible || (entry.target.length > 0 && entry.target !== root.chosenLanguage))
            {
                continue
            }

            if (entry.natives.length > 0 && entry.natives.indexOf(AppOnboarding.chosenNative) < 0)
            {
                continue
            }

            const active = AppPackageDownloader.activeId === entry.id
            const queued = AppPackageDownloader.queue.indexOf(entry.id) >= 0

            if (!entry.installed && !active && !queued)
            {
                continue
            }

            result.push({ kind: entry.kind, installed: entry.installed, active: active })
        }

        return result
    }

    function megabytes(bytes)
    {
        return Math.round(bytes / (1024 * 1024))
    }

    function statusOf(item)
    {
        if (item.active)
        {
            return qsTr("%1 of %2 MB").arg(root.megabytes(AppPackageDownloader.receivedBytes))
                                      .arg(root.megabytes(AppPackageDownloader.totalBytes))
        }

        return item.installed ? qsTr("Ready") : qsTr("Waiting")
    }

    function progressOf(item)
    {
        if (item.installed)
        {
            return 1.0
        }

        if (!item.active || AppPackageDownloader.totalBytes <= 0)
        {
            return 0.0
        }

        return AppPackageDownloader.receivedBytes / AppPackageDownloader.totalBytes
    }

    maximumWidth: AppMetrics.dialogWidth

    Timer
    {
        interval: root.retryInterval
        repeat: true
        running: root.visible && root.stalled

        onTriggered: AppOnboarding.retry()
    }

    DesktopLabel
    {
        Layout.fillWidth: true
        text: qsTr("Downloading %1").arg(root.chosenName)
        font.pixelSize: AppMetrics.fontHeadline
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap

        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    DesktopSettingsGroup
    {
        visible: root.items.length > 0

        Repeater
        {
            model: root.items

            delegate: Item
            {
                id: row

                required property var modelData
                required property int index

                Layout.fillWidth: true
                implicitHeight: rowColumn.implicitHeight + AppMetrics.gapMedium * 2

                Accessible.role: Accessible.ProgressBar
                Accessible.name: qsTr("%1, %2").arg(root.kindNames[row.modelData.kind] || row.modelData.kind).arg(root.statusOf(row.modelData))

                ColumnLayout
                {
                    id: rowColumn

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: AppMetrics.gapLarge
                    anchors.rightMargin: AppMetrics.gapLarge
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: AppMetrics.gapSmall

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: AppMetrics.gapMedium

                        DesktopLabel
                        {
                            Layout.fillWidth: true
                            text: root.kindNames[row.modelData.kind] || row.modelData.kind
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }

                        DesktopLabel
                        {
                            text: root.statusOf(row.modelData)
                            font.pixelSize: AppMetrics.fontSmall
                            color: row.modelData.active || row.modelData.installed ? AppTheme.colourText : AppTheme.colourMuted
                        }
                    }

                    DesktopProgressBar
                    {
                        Layout.fillWidth: true
                        progress: root.progressOf(row.modelData)
                        fill: row.modelData.installed ? AppTheme.colourCorrect : AppTheme.colourText
                    }
                }

                DesktopDivider
                {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    visible: row.index < root.items.length - 1
                }
            }
        }
    }

    DesktopLabel
    {
        Layout.fillWidth: true
        visible: AppPackageDownloader.lastError.length > 0
        text: AppPackageDownloader.lastError
        font.pixelSize: AppMetrics.fontSmall
        color: AppTheme.colourWrong
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    footer: [
        DesktopFormButton
        {
            visible: root.stalled
            text: qsTr("Try again")

            onClicked: AppOnboarding.retry()
        },
        DesktopFormButton
        {
            objectName: "onboarding_start"

            tone: "primary"
            text: qsTr("Start learning")
            keyHint: qsTr("Enter")
            shortcutKeys: ["Return", "Enter"]
            enabled: AppOnboarding.dictionaryReady

            onClicked:
            {
                root.store.selectLanguagePair(AppOnboarding.chosenLanguage, AppOnboarding.chosenNative)
                root.done()
            }
        }
    ]
}
