import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property int notStartedCount: 0
    property int learningCount: 0
    property int learnedCount: 0
    property string selectedKey: ""

    signal stageOpened(string filterKey)

    readonly property var groups: [
        { key: "notlearned", label: qsTr("Not started"), colour: AppTheme.colourOutline,
          figure: AppTheme.colourText, count: root.notStartedCount },
        { key: "learning", label: qsTr("Learning"), colour: AppTheme.colourAccent,
          figure: AppTheme.colourAccent, count: root.learningCount },
        { key: "completed", label: qsTr("Learned"), colour: AppTheme.colourCorrect,
          figure: AppTheme.colourCorrect, count: root.learnedCount }
    ]

    readonly property int total: root.groups.reduce((Sum, Group) => Sum + Group.count, 0)
    readonly property int firstShown: root.groups.findIndex(Group => Group.count > 0)
    readonly property int lastShown: root.groups.map(Group => Group.count > 0).lastIndexOf(true)

    readonly property var segmentWidths:
    {
        const widths = root.groups.map(Group => root.total > 0 ? Math.floor(track.width * Group.count / root.total) : 0)

        if (root.lastShown >= 0)
        {
            widths[root.lastShown] += track.width - widths.reduce((Sum, Width) => Sum + Width, 0)
        }

        return widths
    }

    Layout.fillWidth: true
    spacing: AppMetrics.gapCompact

    Rectangle
    {
        id: track

        Layout.fillWidth: true
        Layout.preferredHeight: AppMetrics.progressBarHeight * 2
        radius: height / 2
        color: root.total > 0 ? "transparent" : AppTheme.colourFaint

        Repeater
        {
            model: root.groups

            delegate: Rectangle
            {
                required property var modelData
                required property int index

                readonly property bool first: index === root.firstShown
                readonly property bool last: index === root.lastShown

                x: root.segmentWidths.slice(0, index).reduce((Sum, Width) => Sum + Width, 0)
                width: root.segmentWidths[index] > 0 ? root.segmentWidths[index] + (last ? 0 : 1) : 0
                height: track.height
                antialiasing: true
                topLeftRadius: first ? track.radius : 0
                bottomLeftRadius: first ? track.radius : 0
                topRightRadius: last ? track.radius : 0
                bottomRightRadius: last ? track.radius : 0
                color: modelData.colour
            }
        }
    }

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        Repeater
        {
            model: root.groups

            delegate: Rectangle
            {
                id: cell

                required property var modelData

                readonly property bool selected: modelData.key === root.selectedKey
                readonly property bool tappable: modelData.count > 0 || selected

                objectName: "vocabulary_" + modelData.key

                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.preferredHeight: AppMetrics.sessionTileHeightCompact

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("%1, %2").arg(cell.modelData.label).arg(cell.modelData.count)
                Accessible.checkable: true
                Accessible.checked: cell.selected

                radius: AppMetrics.radiusMedium
                color: Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourAccent,
                    cellTap.pressed ? 0.24 : (cell.selected ? 0.10 : (cellHover.hovered ? AppTheme.alphaFaint : 0))))
                border.width: AppMetrics.borderWidth
                border.color: cell.selected ? AppTheme.faded(AppTheme.colourAccent, 0.6) : AppTheme.colourBorder

                ColumnLayout
                {
                    anchors.centerIn: parent
                    spacing: 2

                    Text
                    {
                        Layout.alignment: Qt.AlignHCenter
                        text: cell.modelData.count
                        font.pixelSize: AppMetrics.fontSubtitle
                        font.weight: Font.Bold
                        font.family: AppMetrics.fontFamily
                        color: cell.tappable ? cell.modelData.figure : AppTheme.colourMuted
                    }

                    Text
                    {
                        Layout.alignment: Qt.AlignHCenter
                        text: cell.modelData.label
                        font.pixelSize: AppMetrics.fontMeta
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourMuted
                    }
                }

                TapHandler
                {
                    id: cellTap

                    enabled: cell.tappable

                    onTapped: root.stageOpened(cell.modelData.key)
                }

                HoverHandler
                {
                    id: cellHover

                    enabled: cell.tappable
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
    }
}
