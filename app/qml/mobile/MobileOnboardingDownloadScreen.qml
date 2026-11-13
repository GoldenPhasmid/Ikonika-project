import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property var store

    signal done()

    readonly property string chosenLanguage: AppOnboarding.chosenLanguage

    readonly property string chosenName:
    {
        for (const entry of AppCatalog.languages)
        {
            if (entry.code === root.chosenLanguage)
            {
                return entry.name
            }
        }

        return root.chosenLanguage
    }

    readonly property bool stalled: !AppPackageDownloader.busy
        && AppPackageDownloader.queue.length === 0
        && !AppOnboarding.dictionaryReady

    readonly property var kindNames: ({
        "dictionary": qsTr("Dictionary"),
        "voice": qsTr("Voice"),
        "translation": qsTr("Sentence translation")
    })

    function kindName(kind)
    {
        return root.kindNames[kind] || kind
    }

    function megabytes(bytes)
    {
        return Math.round(bytes / (1024 * 1024))
    }

    readonly property var items:
    {
        const result = []

        for (const entry of AppCatalog.packages)
        {
            if (!entry.compatible || entry.desktopOnly)
            {
                continue
            }

            if (entry.target.length > 0 && entry.target !== root.chosenLanguage)
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

            result.push({
                kind: entry.kind,
                bytes: entry.bytes,
                installed: entry.installed,
                active: active
            })
        }

        return result
    }

    function statusOf(item)
    {
        if (item.active)
        {
            return qsTr("%1 of %2 MB")
                .arg(root.megabytes(AppPackageDownloader.receivedBytes))
                .arg(root.megabytes(AppPackageDownloader.totalBytes))
        }

        return item.installed ? qsTr("ready") : qsTr("waiting")
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

    color: AppTheme.colourPanel

    Timer
    {
        interval: 15000
        repeat: true
        running: root.visible && root.stalled

        onTriggered: AppOnboarding.retry()
    }

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        MobileHeader {}

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: AppMetrics.gapLarge
            spacing: AppMetrics.gapLarge

            Text
            {
                Layout.fillWidth: true

                text: qsTr("Downloading %1").arg(root.chosenName)
                font.pixelSize: AppMetrics.fontHeadline
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Rectangle
            {
                Layout.fillWidth: true

                visible: root.items.length > 0
                implicitHeight: itemsColumn.implicitHeight

                radius: AppMetrics.radiusMedium
                color: AppTheme.colourPanel
                border.color: AppTheme.colourBorder
                border.width: AppMetrics.borderWidth
                clip: true

                ColumnLayout
                {
                    id: itemsColumn

                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 0

                    Repeater
                    {
                        model: root.items

                        delegate: ColumnLayout
                        {
                            id: itemDelegate

                            required property var modelData
                            required property int index

                            readonly property bool isLast:
                                itemDelegate.index === root.items.length - 1

                            Layout.fillWidth: true
                            spacing: 0

                            ColumnLayout
                            {
                                Layout.fillWidth: true
                                Layout.margins: AppMetrics.contentPadding
                                spacing: 10

                                RowLayout
                                {
                                    Layout.fillWidth: true

                                    Text
                                    {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0

                                        text: root.kindName(itemDelegate.modelData.kind)
                                        font.pixelSize: AppMetrics.fontLabel
                                        font.weight: Font.DemiBold
                                        font.family: AppMetrics.fontFamily
                                        color: AppTheme.colourText
                                        elide: Text.ElideRight
                                    }

                                    Text
                                    {
                                        text: root.statusOf(itemDelegate.modelData)
                                        font.pixelSize: AppMetrics.fontSmall
                                        font.family: AppMetrics.fontFamily
                                        color: itemDelegate.modelData.active
                                            || itemDelegate.modelData.installed
                                            ? AppTheme.colourText
                                            : AppTheme.colourMuted
                                    }
                                }

                                Rectangle
                                {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 6

                                    radius: 3
                                    color: AppTheme.colourFaint

                                    Rectangle
                                    {
                                        width: parent.width * root.progressOf(itemDelegate.modelData)
                                        height: parent.height
                                        radius: 3
                                        color: AppTheme.colourText
                                    }
                                }
                            }

                            Rectangle
                            {
                                Layout.fillWidth: true
                                Layout.preferredHeight: AppMetrics.borderWidth

                                color: AppTheme.colourBorder
                                visible: !itemDelegate.isLast
                            }
                        }
                    }
                }
            }

            Text
            {
                Layout.fillWidth: true

                visible: AppPackageDownloader.lastError.length > 0
                text: AppPackageDownloader.lastError
                font.pixelSize: AppMetrics.fontSmall
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourWrong
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }

            MobileWideButton
            {
                label: qsTr("Try again")
                filled: false
                visible: root.stalled

                onClicked: AppOnboarding.retry()
            }

            MobileWideButton
            {
                label: qsTr("Start learning")
                interactive: AppOnboarding.dictionaryReady

                onClicked:
                {
                    root.store.selectLanguagePair(AppOnboarding.chosenLanguage, AppOnboarding.chosenNative)
                    root.done()
                }
            }
        }
    }
}
