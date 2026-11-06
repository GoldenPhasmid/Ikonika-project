import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes

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
    readonly property color contentColour: root.filled ? AppTheme.colourPanel
                                                       : (root.primary || root.locked || root.completed ? AppTheme.colourMuted
                                                                                                        : AppTheme.colourText)

    Layout.fillWidth: true
    implicitHeight: root.compact ? AppMetrics.sessionTileHeightCompact : AppMetrics.sessionTileHeight

    leftPadding: root.compact ? AppMetrics.gapCompact : AppMetrics.gapLarge
    rightPadding: root.compact ? AppMetrics.gapSmall : AppMetrics.gapLarge
    topPadding: root.compact ? 0 : AppMetrics.gapLarge
    bottomPadding: root.compact ? 0 : AppMetrics.gapLarge

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    z: root.hovered ? 1 : 0
    scale: root.hovered && !root.blocked ? AppMetrics.coverHoverScale : 1.0

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

    contentItem: RowLayout
    {
        spacing: root.compact ? AppMetrics.gapSmall : AppMetrics.gapMedium

        Rectangle
        {
            visible: !root.compact
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: AppMetrics.buttonHeightLarge
            Layout.preferredHeight: AppMetrics.buttonHeightLarge
            radius: AppMetrics.radiusMedium
            color: root.filled ? AppTheme.faded(AppTheme.colourPanel, AppTheme.alphaSoft)
                               : AppTheme.colourFaint

            DesktopIcon
            {
                anchors.centerIn: parent
                name: root.iconName
                width: AppMetrics.iconSize + AppMetrics.gapTight
                height: width
                color: root.contentColour
            }
        }

        DesktopIcon
        {
            visible: root.compact
            name: root.locked ? "lock" : root.iconName
            color: root.contentColour
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
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
                    font.pixelSize: root.compact ? AppMetrics.fontLabel : AppMetrics.fontTitle
                    font.weight: root.compact ? Font.DemiBold : Font.Bold
                    color: root.contentColour
                    elide: Text.ElideRight
                }
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: root.caption.length > 0
                text: root.caption
                font.pixelSize: root.compact ? AppMetrics.fontMeta : AppMetrics.fontSmall
                color: root.contentColour
                opacity: AppTheme.opacitySecondary
                wrapMode: root.compact ? Text.NoWrap : Text.WordWrap
                maximumLineCount: root.compact ? 1 : 2
                elide: Text.ElideRight
            }
        }

        Item
        {
            id: countSlot

            readonly property bool revealed: root.hovered && !root.blocked
            readonly property real shift: arrow.width + (figure.width > 0 ? AppMetrics.gapSmall : 0)

            visible: !root.compact || root.count.length > 0 || root.completed
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: figure.width + countSlot.shift
            Layout.preferredHeight: Math.max(figure.height, arrow.height)
            clip: true

            Item
            {
                id: figure

                anchors.verticalCenter: parent.verticalCenter
                x: countSlot.revealed ? 0 : countSlot.shift
                width: root.completed ? check.width : heroCount.implicitWidth
                height: root.completed ? check.height : heroCount.implicitHeight

                Behavior on x { NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing } }

                DesktopLabel
                {
                    id: heroCount

                    anchors.verticalCenter: parent.verticalCenter
                    visible: !root.completed
                    text: root.count
                    font.pixelSize: root.compact ? AppMetrics.fontSubtitle : AppMetrics.heroFigureSize
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                    color: root.contentColour
                }

                DesktopIcon
                {
                    id: check

                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.completed
                    width: root.compact ? AppMetrics.iconSize : AppMetrics.heroFigureSize
                    height: width
                    name: "check"
                    color: root.contentColour
                }
            }

            DesktopIcon
            {
                id: arrow

                anchors.verticalCenter: parent.verticalCenter
                x: countSlot.revealed ? countSlot.width - width : countSlot.width
                width: root.compact ? AppMetrics.iconSize : AppMetrics.iconSize + AppMetrics.gapTight
                height: width
                name: "arrow_right"
                color: root.contentColour
                opacity: countSlot.revealed ? 1.0 : 0.0

                Behavior on x { NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing } }
                Behavior on opacity { NumberAnimation { duration: AppMetrics.motionBase } }
            }
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusLarge

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

        border.width: root.filled || root.locked ? 0 : AppMetrics.borderWidth
        border.color: root.hovered && !root.blocked ? AppTheme.colourOutline : AppTheme.colourBorder
        clip: true

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        Shape
        {
            id: band

            readonly property real cornerRadius: AppMetrics.radiusLarge - AppMetrics.borderWidth

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: AppMetrics.borderWidth
            width: parent.width * AppMetrics.sessionTileBandWidth
            visible: !root.blocked

            preferredRendererType: Shape.CurveRenderer

            ShapePath
            {
                strokeWidth: -1
                fillColor: AppTheme.colourCoverStripe

                startX: 0
                startY: band.height

                PathLine { x: band.width; y: band.height * AppMetrics.sessionTileBandTop }
                PathLine { x: band.width; y: band.height - band.cornerRadius }
                PathArc { x: band.width - band.cornerRadius; y: band.height; radiusX: band.cornerRadius; radiusY: band.cornerRadius }
                PathLine { x: 0; y: band.height }
            }
        }

        Shape
        {
            id: lockOutline

            anchors.fill: parent
            visible: root.locked
            preferredRendererType: Shape.CurveRenderer

            ShapePath
            {
                strokeColor: AppTheme.colourBorder
                strokeWidth: AppMetrics.borderWidth
                strokeStyle: ShapePath.DashLine
                dashPattern: [4, 3]
                fillColor: "transparent"

                PathRectangle
                {
                    x: AppMetrics.borderWidth / 2
                    y: AppMetrics.borderWidth / 2
                    width: lockOutline.width - AppMetrics.borderWidth
                    height: lockOutline.height - AppMetrics.borderWidth
                    radius: AppMetrics.radiusLarge
                }
            }
        }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: AppMetrics.radiusLarge
        }
    }

    HoverHandler
    {
        cursorShape: root.blocked ? Qt.ArrowCursor : Qt.PointingHandCursor
    }
}
