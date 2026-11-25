import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Item
{
    id: root

    property var book: null
    property string level: ""
    property bool needsDownload: false
    property real downloadProgress: -1

    signal clicked()

    readonly property bool opener: root.book === null
    readonly property bool starting: !root.opener && root.level.length > 0
    readonly property bool reading: !root.opener && !root.starting
    readonly property bool downloading: root.downloadProgress >= 0
    readonly property real barProgress: root.downloading ? root.downloadProgress : (root.book ? root.book.progress : 0)
    readonly property int percent: Math.ceil(Math.max(0, root.barProgress) * 100)
    readonly property string title: root.opener ? qsTr("Open a book") : root.book.title
    readonly property string author: root.opener ? qsTr("PDF, EPUB or FB2 from your phone") : root.book.author || ""

    Layout.fillWidth: true
    Layout.preferredHeight: 134

    scale: tap.pressed ? 0.98 : 1

    Behavior on scale
    {
        NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
    }

    Accessible.role: Accessible.Button
    Accessible.name: root.opener ? root.title
                                 : root.starting ? qsTr("Start %1, %2").arg(root.title).arg(root.level)
                                                 : qsTr("Continue %1, %2% read").arg(root.title).arg(root.percent)
    Accessible.description: root.author
    Accessible.onPressAction: root.clicked()

    Rectangle
    {
        anchors.fill: parent
        radius: 18
        color: AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder
    }

    RowLayout
    {
        anchors.fill: parent
        anchors.leftMargin: AppMetrics.gapCompact
        anchors.rightMargin: AppMetrics.gapMedium
        anchors.topMargin: AppMetrics.gapCompact
        anchors.bottomMargin: AppMetrics.gapCompact
        spacing: 14

        Rectangle
        {
            visible: root.opener
            Layout.fillHeight: true
            Layout.preferredWidth: Math.round(height * 2 / 3)
            radius: AppMetrics.bookRadius
            color: AppTheme.colourFaint

            MobileVectorIcon
            {
                anchors.centerIn: parent
                width: 22
                height: 22
                svgPath: Icons.Icons.folder_plus.path
                color: AppTheme.colourSubtle
            }
        }

        DesktopBookCover
        {
            visible: !root.opener
            Layout.fillHeight: true
            Layout.preferredWidth: Math.round(height * 2 / 3)

            title: root.opener ? "" : root.book.title
            author: root.opener ? "" : root.book.author || ""
            imageSource: root.opener ? "" : root.book.cover || ""
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 3

            Text
            {
                Layout.fillWidth: true

                text: root.title
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                lineHeight: 1.15
                font.family: AppMetrics.fontFamily
                font.pixelSize: 16
                font.weight: Font.DemiBold
                color: AppTheme.colourText
            }

            Text
            {
                Layout.fillWidth: true
                visible: text.length > 0

                text: root.author
                elide: Text.ElideRight
                maximumLineCount: 1
                font.family: AppMetrics.fontFamily
                font.pixelSize: 13
                color: AppTheme.colourMuted
            }

            Item
            {
                Layout.fillHeight: true
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: 10

                Rectangle
                {
                    visible: root.reading || root.downloading
                    Layout.fillWidth: true
                    Layout.preferredHeight: 5
                    radius: 3
                    color: AppTheme.colourFaint

                    Rectangle
                    {
                        width: Math.max(parent.height, parent.width * Math.min(Math.max(root.barProgress, 0), 1))
                        height: parent.height
                        radius: parent.radius
                        color: AppTheme.colourAccent
                    }
                }

                Text
                {
                    visible: root.starting && !root.downloading
                    Layout.fillWidth: true

                    text: root.level
                    elide: Text.ElideRight
                    font.family: AppMetrics.fontFamily
                    font.pixelSize: 13
                    color: AppTheme.colourMuted
                }

                Item
                {
                    visible: root.opener
                    Layout.fillWidth: true
                }

                Text
                {
                    visible: root.reading || root.downloading

                    text: qsTr("%1%").arg(root.percent)
                    font.family: AppMetrics.fontFamily
                    font.pixelSize: 13
                    font.features: { "tnum": 1 }
                    color: AppTheme.colourMuted
                }

                MobileVectorIcon
                {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18

                    svgPath: root.needsDownload && !root.downloading ? Icons.Icons.download.path : Icons.Icons.arrow_right.path
                    color: AppTheme.colourMuted
                }
            }
        }
    }

    MouseArea
    {
        id: tap

        anchors.fill: parent

        onClicked: root.clicked()
    }
}
