import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons
import "../Decks.js" as Decks

Item
{
    id: root

    property string title: ""
    property string iconName: ""
    property color tint: AppTheme.colourCoverMuted
    property int wordCount: 0
    property int learningCount: 0
    property int learnedCount: 0
    property bool compact: false
    property bool pickable: false
    property bool picked: false

    signal openRequested()
    signal pickRequested()
    signal held()

    readonly property bool pressed: deckTap.pressed
    readonly property bool lifted: root.enabled && !root.compact && (root.pressed || root.picked)
    readonly property real wordCountSafe: Math.max(root.wordCount, 1)
    readonly property string detail: Decks.detail(root)

    function wiggle()
    {
        refusal.restart()
    }

    function activate()
    {
        if (root.pickable)
        {
            root.pickRequested()

            return
        }

        root.openRequested()
    }

    implicitHeight: root.compact ? AppMetrics.readerRowHeight : Math.round(root.width / AppMetrics.readerTileAspect)
    opacity: root.enabled ? 1 : AppTheme.opacityDisabled
    z: root.lifted ? 1 : 0
    scale: root.lifted ? AppMetrics.coverHoverScale : 1.0

    Behavior on scale { NumberAnimation { duration: AppMetrics.motionBase; easing.type: Easing.OutCubic } }

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Open %1, %2").arg(root.title).arg(root.detail)
    Accessible.onPressAction: if (root.enabled) root.activate()

    TapHandler
    {
        id: deckTap

        onTapped: root.activate()
        onLongPressed: root.held()
    }

    component Progress: Rectangle
    {
        radius: height / 2
        color: AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaStrong)
        clip: true

        Row
        {
            anchors.fill: parent

            Rectangle
            {
                width: parent.width * root.learnedCount / root.wordCountSafe
                height: parent.height
                color: AppTheme.colourCoverInk
            }

            Rectangle
            {
                width: parent.width * root.learningCount / root.wordCountSafe
                height: parent.height
                color: AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaHalf)
            }
        }
    }

    DesktopWiggleAnimation
    {
        id: refusal

        target: root
    }

    Rectangle
    {
        anchors.fill: parent
        visible: !root.compact
        radius: AppMetrics.coverRadius
        color: !root.enabled ? AppTheme.colourCoverMuted
             : root.pressed ? Qt.tint(root.tint, AppTheme.faded(AppTheme.colourText, AppTheme.alphaFaint))
             : root.tint

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        CoverStripe {}

        DesktopIcon
        {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: AppMetrics.gapCompact

            name: root.iconName
            color: AppTheme.colourCoverInk
        }

        MobileVectorIcon
        {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: AppMetrics.gapCompact
            width: AppMetrics.iconSize
            height: width
            visible: root.picked

            svgPath: Icons.Icons.check.path
            color: AppTheme.colourCoverInk
        }

        Rectangle
        {
            anchors.fill: parent
            visible: root.picked
            radius: AppMetrics.coverRadius
            color: "transparent"
            border.color: AppTheme.colourCoverInk
            border.width: AppMetrics.focusRingWidth
        }

        ColumnLayout
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: AppMetrics.gapCompact
            anchors.bottomMargin: AppMetrics.gapSmall
            spacing: AppMetrics.gapSmall

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapTight

                Text
                {
                    Layout.fillWidth: true

                    text: root.title
                    color: AppTheme.colourCoverInk
                    font.pixelSize: AppMetrics.fontBody
                    font.weight: Font.DemiBold
                    font.family: AppMetrics.fontFamily
                    elide: Text.ElideRight
                }

                Text
                {
                    text: root.wordCount
                    color: AppTheme.colourCoverInk
                    opacity: AppTheme.opacitySecondary
                    font.pixelSize: AppMetrics.fontMeta
                    font.family: AppMetrics.fontFamily
                }
            }

            Progress
            {
                Layout.fillWidth: true
                Layout.preferredHeight: AppMetrics.coverProgressHeight
            }
        }
    }

    Item
    {
        anchors.fill: parent
        visible: root.compact

        Rectangle
        {
            anchors.fill: parent
            color: AppTheme.colourFaint
            opacity: root.pressed ? 1.0 : 0.0

            Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
        }

        RowLayout
        {
            anchors.fill: parent
            spacing: AppMetrics.gapCompact

            Rectangle
            {
                Layout.preferredWidth: AppMetrics.readerRowCoverWidth
                Layout.preferredHeight: Math.round(AppMetrics.readerRowCoverWidth / AppMetrics.readerTileAspect)
                radius: AppMetrics.radiusSmall
                color: root.tint

                CoverStripe {}

                DesktopIcon
                {
                    anchors.centerIn: parent

                    name: root.iconName
                    color: AppTheme.colourCoverInk
                }

                Progress
                {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: AppMetrics.gapTight + AppMetrics.borderWidth
                    height: AppMetrics.coverProgressHeight - AppMetrics.borderWidth
                }
            }

            ColumnLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.borderWidth * 3

                Text
                {
                    Layout.fillWidth: true

                    text: root.title
                    color: AppTheme.colourText
                    font.pixelSize: AppMetrics.fontLabel
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    elide: Text.ElideRight
                }

                Text
                {
                    Layout.fillWidth: true

                    text: root.detail
                    color: AppTheme.colourMuted
                    font.pixelSize: AppMetrics.fontSmall
                    font.family: AppMetrics.fontFamily
                    elide: Text.ElideRight
                }
            }
        }

        Rectangle
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: AppMetrics.borderWidth
            color: AppTheme.colourBorder
        }
    }
}
