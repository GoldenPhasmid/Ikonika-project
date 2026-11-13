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
    property int untriagedCount: 0
    property bool pickable: false
    property bool picked: false

    signal trainRequested()
    signal openRequested()
    signal pickRequested()

    readonly property bool trainable: root.untriagedCount > 0
    readonly property bool lifted: root.enabled && (root.hovered || root.picked)
    readonly property Item face: cover

    readonly property real wordCountSafe: Math.max(root.wordCount, 1)
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

        root.trainable ? root.trainRequested() : root.openRequested()
    }

    Accessible.role: root.pickable ? Accessible.CheckBox : Accessible.Button
    Accessible.checkable: root.pickable
    Accessible.checked: root.picked
    Accessible.name: root.pickable ? root.title
        : (root.trainable ? qsTr("Train %1").arg(root.title) : qsTr("Open %1").arg(root.title))
    Accessible.description: qsTr("%1 of %2 learned, %3 learning")
        .arg(root.learnedCount).arg(root.wordCount).arg(root.learningCount)

    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()

    contentItem: ColumnLayout
    {
        spacing: AppMetrics.gapSmall

        Rectangle
        {
            id: cover

            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(width / AppMetrics.readerTileAspect)
            radius: AppMetrics.coverRadius
            color: !root.enabled ? AppTheme.colourCoverMuted
                 : root.hovered ? Qt.tint(root.tint, AppTheme.faded(AppTheme.colourText, AppTheme.alphaFaint))
                 : root.tint
            clip: true

            Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

            CoverStripe {}

            DesktopIcon
            {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: AppMetrics.gapMedium
                width: AppMetrics.deckIconSize
                height: AppMetrics.deckIconSize
                name: root.iconName
                color: root.inkColour
            }

            DesktopCoverBar
            {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: AppMetrics.gapSmall
                visible: !root.pickable
                shown: root.hovered || root.visualFocus

                DesktopCoverButton
                {
                    objectName: "deck_open"

                    iconName: "contents"
                    text: qsTr("Words")

                    Accessible.name: qsTr("Words of %1").arg(root.title)

                    onClicked: root.openRequested()
                }
            }

            ColumnLayout
            {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: AppMetrics.gapMedium
                spacing: AppMetrics.gapSmall

                RowLayout
                {
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapSmall

                    DesktopLabel
                    {
                        Layout.fillWidth: true
                        text: root.title
                        font.pixelSize: AppMetrics.fontTitle
                        font.weight: Font.DemiBold
                        color: root.inkColour
                        elide: Text.ElideRight
                    }

                    DesktopLabel
                    {
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        text: root.wordCount
                        font.pixelSize: AppMetrics.fontSmall
                        color: root.inkColour
                        opacity: AppTheme.opacitySecondary
                    }
                }

                Rectangle
                {
                    Layout.fillWidth: true
                    implicitHeight: AppMetrics.coverProgressHeight
                    radius: height / 2
                    color: AppTheme.faded(root.inkColour, AppTheme.alphaStrong)
                    clip: true

                    Row
                    {
                        anchors.fill: parent

                        Rectangle
                        {
                            width: parent.width * root.learnedCount / root.wordCountSafe
                            height: parent.height
                            color: root.inkColour
                        }

                        Rectangle
                        {
                            width: parent.width * root.learningCount / root.wordCountSafe
                            height: parent.height
                            color: AppTheme.faded(root.inkColour, AppTheme.alphaHalf)
                        }
                    }
                }
            }

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
