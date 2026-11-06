import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property real relative: 0
    property bool hasProbability: true
    property int learnedStage: -1
    property bool inUse: false
    property bool accentInUse: false
    property bool keyHighlighted: false

    readonly property bool learned: root.learnedStage >= 0
    readonly property int level: root.hasProbability ? Math.max(1, Math.round(root.relative * 4)) : 0
    readonly property bool filledInUse: root.inUse && !root.accentInUse
    readonly property color contentColour: root.filledInUse ? AppTheme.colourPanel : AppTheme.colourText

    Layout.fillWidth: true
    implicitHeight: AppMetrics.buttonHeightLarge

    leftPadding: AppMetrics.gapMedium
    rightPadding: AppMetrics.gapMedium
    topPadding: 0
    bottomPadding: 0

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.name: root.learned ? qsTr("%1, already in My Vocabulary").arg(root.text) : root.text
    Accessible.checkable: true
    Accessible.checked: root.inUse

    ToolTip.visible: root.hovered && !root.inUse
    ToolTip.text: root.learned ? qsTr("Edit this card") : qsTr("Add with this translation")
    ToolTip.delay: AppMetrics.tooltipDelay

    contentItem: RowLayout
    {
        spacing: AppMetrics.gapSmall

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.text
            font.weight: Font.Medium
            color: root.contentColour
            elide: Text.ElideRight
        }

        DesktopLearnedMark
        {
            stage: root.learnedStage
        }

        DesktopSignalBars
        {
            visible: root.hasProbability
            level: root.level
            activeColour: root.contentColour
            inactiveColour: root.filledInUse ? AppTheme.colourSubtle : AppTheme.colourBorder
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium

        color:
        {
            if (root.filledInUse)
            {
                return AppTheme.colourText
            }

            if (root.inUse)
            {
                return Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaMedium))
            }

            const base = root.learned
                ? Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaSoft))
                : AppTheme.colourPanel

            return root.down ? AppTheme.colourFaint : (root.hovered ? AppTheme.colourSunken : base)
        }

        border.width: AppMetrics.borderWidth
        border.color: root.inUse ? (root.accentInUse ? AppTheme.colourAccent : AppTheme.colourText)
                                 : (root.learned ? AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaHalf)
                                                 : (root.hovered ? AppTheme.colourOutline : AppTheme.colourBorder))

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopFocusRing
        {
            visible: root.visualFocus || root.keyHighlighted
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
