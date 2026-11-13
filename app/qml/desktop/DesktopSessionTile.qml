import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property string iconName: ""
    property string caption: ""
    property string count: ""
    property bool completed: false
    property bool primary: false
    property bool compact: false
    property bool blocked: false
    property color dotColour: "transparent"

    signal activated()
    signal blockedClicked()

    readonly property bool filled: root.primary && !root.blocked
    readonly property bool locked: root.blocked && !root.primary
    readonly property bool revealed: root.hovered && !root.blocked
    readonly property color contentColour: root.filled ? AppTheme.colourPanel
                                                       : (root.primary || root.locked || root.completed ? AppTheme.colourMuted
                                                                                                        : AppTheme.colourText)

    Layout.fillWidth: true
    implicitHeight: root.compact ? AppMetrics.sessionTileHeightCompact : AppMetrics.sessionTileHeight

    leftPadding: root.compact ? AppMetrics.gapCompact : AppMetrics.gapWide
    rightPadding: root.compact ? AppMetrics.gapCompact : AppMetrics.gapWide
    topPadding: root.compact ? 0 : AppMetrics.gapWide
    bottomPadding: root.compact ? 0 : AppMetrics.gapWide

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    z: root.hovered ? 1 : 0
    scale: root.revealed ? AppMetrics.coverHoverScale : 1.0

    Behavior on scale { NumberAnimation { duration: AppMetrics.motionBase; easing.type: Easing.OutCubic } }

    Accessible.name:
    {
        if (root.completed)
        {
            return qsTr("%1, done for today").arg(root.text)
        }

        return root.count.length > 0 ? qsTr("%1, %2").arg(root.text).arg(root.count) : root.text
    }
    Accessible.description: root.caption

    onClicked: root.blocked ? root.blockedClicked() : root.activated()

    contentItem: Item
    {
        Item
        {
            anchors.fill: parent
            visible: !root.compact

            DesktopIcon
            {
                anchors.left: parent.left
                anchors.top: parent.top
                width: AppMetrics.iconSize + AppMetrics.gapTight
                height: width
                name: root.locked ? "lock" : root.iconName
                color: root.contentColour
            }

            ColumnLayout
            {
                anchors.left: parent.left
                anchors.right: countSlot.left
                anchors.rightMargin: AppMetrics.gapMedium
                anchors.bottom: parent.bottom
                spacing: AppMetrics.gapTight

                RowLayout
                {
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapSmall

                    Rectangle
                    {
                        visible: root.dotColour.a > 0
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: AppMetrics.masteryDotSize
                        Layout.preferredHeight: AppMetrics.masteryDotSize
                        radius: width / 2
                        color: root.dotColour
                    }

                    DesktopLabel
                    {
                        Layout.fillWidth: true
                        text: root.text
                        font.pixelSize: AppMetrics.fontTitle
                        font.weight: Font.Bold
                        color: root.contentColour
                        elide: Text.ElideRight
                    }
                }

                DesktopLabel
                {
                    Layout.fillWidth: true
                    visible: root.caption.length > 0
                    text: root.caption
                    font.pixelSize: AppMetrics.fontSmall
                    color: root.contentColour
                    opacity: AppTheme.opacitySecondary
                    elide: Text.ElideRight
                }
            }

            Item
            {
                id: countSlot

                readonly property real shift: arrow.width + (figure.width > 0 ? AppMetrics.gapSmall : 0)

                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: figure.width + countSlot.shift
                height: Math.max(figure.height, arrow.height)
                clip: true

                Item
                {
                    id: figure

                    anchors.bottom: parent.bottom
                    x: root.revealed ? 0 : countSlot.shift
                    width: root.completed ? check.width : heroCount.implicitWidth
                    height: root.completed ? check.height : heroCount.implicitHeight

                    Behavior on x { NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing } }

                    DesktopLabel
                    {
                        id: heroCount

                        anchors.bottom: parent.bottom
                        visible: !root.completed
                        text: root.count
                        font.pixelSize: AppMetrics.heroFigureSize
                        font.weight: Font.DemiBold
                        font.features: { "tnum": 1 }
                        color: root.contentColour
                    }

                    DesktopIcon
                    {
                        id: check

                        anchors.bottom: parent.bottom
                        visible: root.completed
                        width: AppMetrics.iconSize + AppMetrics.gapSmall
                        height: width
                        name: "check"
                        color: root.contentColour
                    }
                }

                TextMetrics
                {
                    id: countMetrics

                    font: heroCount.font
                    text: root.count
                }

                DesktopIcon
                {
                    id: arrow

                    readonly property real centre: root.completed
                        ? figure.y + figure.height / 2
                        : figure.y + heroCount.y + heroCount.baselineOffset
                          + countMetrics.tightBoundingRect.y + countMetrics.tightBoundingRect.height / 2

                    x: root.revealed ? countSlot.width - width : countSlot.width
                    y: arrow.centre - height / 2
                    width: AppMetrics.iconSize + AppMetrics.gapTight
                    height: width
                    name: "arrow_right"
                    color: root.contentColour
                    opacity: root.revealed ? 1.0 : 0.0

                    Behavior on x { NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing } }
                    Behavior on opacity { NumberAnimation { duration: AppMetrics.motionBase } }
                }
            }
        }

        RowLayout
        {
            anchors.fill: parent
            visible: root.compact
            spacing: AppMetrics.gapSmall

            DesktopIcon
            {
                Layout.alignment: Qt.AlignVCenter
                name: root.locked ? "lock" : root.iconName
                color: root.contentColour
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: root.text
                font.pixelSize: AppMetrics.fontLabel
                font.weight: Font.DemiBold
                color: root.contentColour
                elide: Text.ElideRight
            }

            DesktopLabel
            {
                Layout.alignment: Qt.AlignVCenter
                visible: !root.revealed && !root.completed && root.count.length > 0
                text: root.count
                font.pixelSize: AppMetrics.fontSmall
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
                color: root.contentColour
                opacity: AppTheme.opacitySecondary
            }

            DesktopIcon
            {
                Layout.alignment: Qt.AlignVCenter
                visible: root.revealed || root.completed
                name: root.revealed ? "arrow_right" : "check"
                color: root.contentColour
            }
        }
    }

    background: Rectangle
    {
        radius: root.compact ? AppMetrics.radiusMedium : AppMetrics.radiusLarge

        color:
        {
            if (root.primary)
            {
                if (root.blocked)
                {
                    return AppTheme.faded(AppTheme.colourText, 0)
                }

                const veil = root.down ? AppTheme.alphaStrong : (root.hovered ? AppTheme.alphaSoft : 0)

                return Qt.tint(AppTheme.colourText, AppTheme.faded(AppTheme.colourPanel, veil))
            }

            if (root.locked)
            {
                return "transparent"
            }

            return root.down ? AppTheme.colourFaint : (root.hovered ? AppTheme.colourSunken : AppTheme.colourPanel)
        }

        border.width: root.locked ? 0 : AppMetrics.borderWidth
        border.color: root.revealed ? AppTheme.colourOutline : AppTheme.colourBorder

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopDashedBox
        {
            anchors.fill: parent
            visible: root.locked
            radius: parent.radius
        }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: parent.radius
        }
    }

    HoverHandler
    {
        cursorShape: root.blocked ? Qt.ArrowCursor : Qt.PointingHandCursor
    }
}
