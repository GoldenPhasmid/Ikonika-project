import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property string author: ""
    property string coverSource: ""
    property real progress: 0
    property string level: ""
    property bool needsDownload: false
    property real downloadProgress: -1
    property string iconName: ""

    readonly property bool opener: root.iconName.length > 0
    readonly property bool starting: root.level.length > 0
    readonly property bool reading: !root.starting && !root.opener
    readonly property bool downloading: root.downloadProgress >= 0
    readonly property real barProgress: root.downloading ? root.downloadProgress : root.progress
    readonly property int percent: Math.ceil(Math.max(0, root.barProgress) * 100)

    padding: AppMetrics.gapCompact

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    z: root.hovered ? 1 : 0
    scale: root.hovered ? AppMetrics.coverHoverScale : 1.0

    Behavior on scale { NumberAnimation { duration: AppMetrics.motionBase; easing.type: Easing.OutCubic } }

    Accessible.name:
    {
        if (root.opener)
        {
            return root.text
        }

        return root.starting ? qsTr("Start %1, %2").arg(root.text).arg(root.level)
                             : qsTr("Continue %1, %2% read").arg(root.text).arg(root.percent)
    }
    Accessible.description: root.author

    contentItem: RowLayout
    {
        spacing: AppMetrics.gapMedium

        Rectangle
        {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.round(height / AppMetrics.bookCoverAspect)
            visible: root.opener
            radius: AppMetrics.coverRadius
            color: AppTheme.colourFaint

            DesktopIcon
            {
                anchors.centerIn: parent
                name: root.iconName
                color: AppTheme.colourSubtle
            }
        }

        DesktopBookCover
        {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.round(height / AppMetrics.bookCoverAspect)
            visible: !root.opener
            title: root.text
            author: root.author
            imageSource: root.coverSource
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: AppMetrics.gapTight

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.text
                font.pixelSize: AppMetrics.fontLabel
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.author
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Item
            {
                Layout.fillHeight: true
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapSmall

                DesktopProgressBar
                {
                    Layout.fillWidth: true
                    visible: root.reading || root.downloading
                    progress: root.barProgress
                    fill: AppTheme.colourAccent
                }

                DesktopLabel
                {
                    Layout.fillWidth: true
                    visible: root.starting && !root.downloading
                    text: root.level
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted
                }

                Item
                {
                    Layout.fillWidth: true
                    visible: root.opener
                }

                DesktopLabel
                {
                    visible: root.reading || root.downloading
                    text: qsTr("%1%").arg(root.percent)
                    font.pixelSize: AppMetrics.fontSmall
                    font.features: { "tnum": 1 }
                    color: AppTheme.colourMuted
                }

                DesktopIcon
                {
                    name: root.needsDownload && !root.downloading ? "download" : "arrow_right"
                    color: root.hovered ? AppTheme.colourText : AppTheme.colourSubtle
                    width: AppMetrics.iconSizeSmall
                    height: AppMetrics.iconSizeSmall
                }
            }
        }
    }

    background: Rectangle
    {
        id: plate

        radius: AppMetrics.radiusLarge
        color: root.down ? AppTheme.colourFaint : (root.hovered ? AppTheme.colourSunken : AppTheme.colourPanel)
        border.width: AppMetrics.borderWidth
        border.color: root.hovered ? AppTheme.colourOutline : AppTheme.colourBorder

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopTileShadow
        {
            plateColour: plate.color
            radius: plate.radius
        }

        DesktopTileSheen
        {
            colour: plate.color
            radius: plate.radius
        }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: AppMetrics.radiusLarge
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
