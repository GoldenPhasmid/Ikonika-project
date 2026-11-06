import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property int count: 0
    property string caption: ""

    readonly property bool hasWords: root.count > 0

    implicitHeight: AppMetrics.dayTileHeight
    topPadding: 0
    bottomPadding: 0

    enabled: root.hasWords
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.name: qsTr("%1, %n words due", "", root.count).arg(root.text)

    contentItem: Item
    {
        ColumnLayout
        {
            anchors.centerIn: parent
            width: parent.width
            spacing: 0

            DesktopLabel
            {
                Layout.alignment: Qt.AlignHCenter
                text: root.count
                font.pixelSize: AppMetrics.fontSubtitle
                font.weight: Font.Bold
                color: root.hasWords ? AppTheme.colourText : AppTheme.colourMuted
            }

            DesktopLabel
            {
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: root.width - AppMetrics.gapSmall
                text: root.caption
                font.pixelSize: AppMetrics.fontMeta
                color: AppTheme.colourMuted
                elide: Text.ElideRight
            }
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: root.hasWords
            ? Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourAccent,
                                                           root.down ? AppTheme.alphaStrong
                                                                     : (root.hovered ? AppTheme.alphaMedium : AppTheme.alphaFaint)))
            : "transparent"
        border.width: AppMetrics.borderWidth
        border.color: root.hasWords ? AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaHalf) : AppTheme.colourBorder

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopFocusRing
        {
            visible: root.visualFocus
        }
    }

    HoverHandler
    {
        cursorShape: root.hasWords ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
