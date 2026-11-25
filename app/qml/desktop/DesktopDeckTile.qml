import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property string title: ""
    property string iconName: ""
    property color tint: AppTheme.colourCoverMuted
    property int wordCount: 0
    property int learningCount: 0
    property int learnedCount: 0
    property bool pickable: false
    property bool picked: false

    signal openRequested()
    signal pickRequested()

    readonly property bool lifted: root.enabled && (root.hovered || root.picked)
    readonly property Item face: cover

    readonly property color inkColour: AppTheme.colourCoverInk

    hoverEnabled: true
    focusPolicy: root.pickable ? Qt.TabFocus : Qt.StrongFocus
    padding: 0

    z: root.lifted ? 1 : 0
    scale: root.lifted ? AppMetrics.coverHoverScale : 1.0
    opacity: root.enabled ? 1 : AppTheme.opacityDisabled

    Behavior on scale { NumberAnimation { duration: AppMetrics.motionBase; easing.type: Easing.OutCubic } }

    text: root.title

    onClicked:
    {
        if (root.pickable)
        {
            root.pickRequested()

            return
        }

        root.openRequested()
    }

    Accessible.role: root.pickable ? Accessible.CheckBox : Accessible.Button
    Accessible.checkable: root.pickable
    Accessible.checked: root.picked
    Accessible.name: root.pickable ? root.title : qsTr("Open %1").arg(root.title)
    Accessible.description: qsTr("%1 of %2 learned, %3 learning")
        .arg(root.learnedCount).arg(root.wordCount).arg(root.learningCount)

    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()

    contentItem: ColumnLayout
    {
        spacing: AppMetrics.gapSmall

        DesktopDeckCover
        {
            id: cover

            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(width / AppMetrics.readerTileAspect)
            title: root.title
            iconName: root.iconName
            wordCount: root.wordCount
            learningCount: root.learningCount
            learnedCount: root.learnedCount
            color: !root.enabled ? AppTheme.colourCoverMuted
                 : root.hovered ? Qt.tint(root.tint, AppTheme.faded(AppTheme.colourText, AppTheme.alphaFaint))
                 : root.tint

            Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

            DesktopIcon
            {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: AppMetrics.gapMedium
                width: AppMetrics.deckIconSize
                height: AppMetrics.deckIconSize
                visible: root.picked
                name: "check"
                color: root.inkColour
            }

            Rectangle
            {
                anchors.fill: parent
                visible: root.picked
                radius: AppMetrics.coverRadius
                color: "transparent"
                border.color: root.inkColour
                border.width: AppMetrics.focusRingWidth
            }

            DesktopFocusRing
            {
                visible: root.visualFocus
                baseRadius: AppMetrics.coverRadius
            }
        }
    }

    background: Item {}

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
