import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

DesktopSidePane
{
    id: root

    property var entries: []
    property bool loading: false
    property bool paged: false
    property int positionCount: 0

    signal entryActivated(int position)
    signal closeRequested()

    function positionLabel(position)
    {
        if (root.paged)
        {
            return (position + 1).toString()
        }

        return qsTr("%1%").arg(root.positionCount > 1 ? Math.round(100 * position / (root.positionCount - 1)) : 0)
    }

    widthKey: "readerContents"
    defaultWidth: AppMetrics.readerContentsWidth

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Contents")

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        DesktopLabel
        {
            Layout.fillWidth: true
            text: qsTr("Contents")
            font.pixelSize: AppMetrics.fontSubtitle
            font.weight: Font.DemiBold

            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        DesktopIconButton
        {
            objectName: "reader_contents_close"

            iconName: "close"
            text: qsTr("Close")

            onClicked: root.closeRequested()
        }
    }

    DesktopLabel
    {
        Layout.fillWidth: true
        visible: root.loading || root.entries.length === 0
        text: root.loading ? qsTr("Loading") : qsTr("No table of contents in this book")
        color: AppTheme.colourMuted
        wrapMode: Text.WordWrap
    }

    ListView
    {
        id: list

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: -AppMetrics.gapSmall
        Layout.rightMargin: -AppMetrics.gapSmall

        visible: !root.loading && root.entries.length > 0
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.entries

        Accessible.role: Accessible.List
        Accessible.name: qsTr("Chapters")

        ScrollBar.vertical: DesktopScrollBar { id: scrollBar }

        delegate: ItemDelegate
        {
            id: entry

            required property var modelData

            width: list.width - (scrollBar.visible ? scrollBar.width : 0)
            implicitHeight: AppMetrics.navItemHeight
            leftPadding: AppMetrics.gapSmall + modelData.level * AppMetrics.gapMedium
            rightPadding: AppMetrics.gapSmall
            topPadding: 0
            bottomPadding: 0
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus

            text: modelData.title
            Accessible.role: Accessible.ListItem
            Accessible.name: modelData.title

            onClicked: root.entryActivated(modelData.page)

            contentItem: RowLayout
            {
                spacing: AppMetrics.gapSmall

                DesktopLabel
                {
                    Layout.fillWidth: true
                    text: entry.modelData.title
                    font.weight: entry.modelData.level === 0 ? Font.Medium : Font.Normal
                    elide: Text.ElideRight
                }

                DesktopLabel
                {
                    text: root.positionLabel(entry.modelData.page)
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted
                }
            }

            background: DesktopSelectionBackground
            {
                hovered: entry.hovered
                pressed: entry.down
                focused: entry.visualFocus
            }

            HoverHandler
            {
                cursorShape: Qt.PointingHandCursor
            }
        }
    }

    Item
    {
        visible: root.loading || root.entries.length === 0
        Layout.fillHeight: true
    }
}
