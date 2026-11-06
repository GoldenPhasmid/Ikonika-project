import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property var tags: []
    property string counterText: ""
    property int masteryTarget: 0
    property int masteryFilled: 0
    property bool showPrimary: true
    property bool compactPrompt: false
    property bool centreSecondary: false
    property bool fillSecondary: false
    property bool primarySpeaks: false
    property real promptHeight: AppMetrics.exercisePromptHeight

    property alias primary: primaryColumn.data
    property alias secondary: secondaryColumn.data
    property alias controls: actionRow.data
    property alias overlay: overlayLayer.data
    readonly property Item surface: card

    readonly property var visibleTags: root.tags.filter(Tag => !!Tag && Tag.length > 0)

    signal primaryClicked()

    function wordTags(controller)
    {
        if (!controller)
        {
            return []
        }

        return [controller.partOfSpeech, controller.partOfSpeech === "noun" ? controller.gender : ""]
    }

    function swap(change)
    {
        change()
    }

    function armMasteryPop()
    {
        dots.armPop()
    }

    readonly property real designHeight: AppMetrics.sessionCardHeight
    readonly property real actionsHeight: AppMetrics.buttonHeightLarge + AppMetrics.gapLarge
    readonly property real designTop: Math.max(AppMetrics.gapLarge, (root.height - root.designHeight - root.actionsHeight) / 2)
    readonly property real cardBottom: Math.min(root.designTop + root.designHeight, root.height - root.actionsHeight - AppMetrics.gapLarge)
    readonly property bool compact: root.cardBottom - AppMetrics.gapLarge < root.designHeight
    readonly property real zoneGap: root.compact || root.compactPrompt ? AppMetrics.gapMedium : AppMetrics.gapLarge

    Rectangle
    {
        id: card

        x: (root.width - width) / 2
        y: Math.max(AppMetrics.gapLarge, root.cardBottom - height)
        width: Math.min(AppMetrics.sessionCardWidth, root.width - AppMetrics.contentPadding * 2)
        height: Math.min(Math.max(root.designHeight, body.implicitHeight + body.anchors.topMargin + body.anchors.bottomMargin),
                         root.cardBottom - AppMetrics.gapLarge)

        radius: AppMetrics.radiusLarge
        color: AppTheme.colourCard
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder

        Item
        {
            id: overlayLayer

            anchors.fill: parent
        }

        ColumnLayout
        {
            id: body

            anchors.fill: parent
            anchors.topMargin: root.compact ? AppMetrics.gapMedium : AppMetrics.gapLarge
            anchors.bottomMargin: root.compact ? AppMetrics.gapMedium : AppMetrics.contentPadding
            spacing: 0

            Item
            {
                Layout.fillWidth: true
                Layout.leftMargin: AppMetrics.contentPadding
                Layout.rightMargin: AppMetrics.contentPadding
                Layout.preferredHeight: AppMetrics.buttonHeight

                Row
                {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: AppMetrics.gapSmall

                    Repeater
                    {
                        model: root.visibleTags

                        delegate: DesktopLevelBadge
                        {
                            required property string modelData

                            label: modelData
                            fill: AppTheme.colourSubtle
                        }
                    }
                }

                DesktopMasteryDots
                {
                    id: dots

                    anchors.centerIn: parent
                    visible: root.masteryTarget > 0
                    target: root.masteryTarget
                    filled: root.masteryFilled
                }

                DesktopLabel
                {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.counterText
                    font.pixelSize: AppMetrics.fontSmall
                    font.weight: Font.Medium
                    color: AppTheme.colourMuted

                    Accessible.role: Accessible.StaticText
                    Accessible.name: qsTr("Card %1").arg(text)
                }
            }

            Item
            {
                visible: root.showPrimary
                Layout.fillWidth: true
                Layout.leftMargin: AppMetrics.contentPadding
                Layout.rightMargin: AppMetrics.contentPadding
                Layout.preferredHeight: root.compactPrompt ? AppMetrics.exercisePromptHeightCompact : root.promptHeight
                Layout.minimumHeight: primaryColumn.implicitHeight
                Layout.fillHeight: root.compact

                MouseArea
                {
                    objectName: "exercise_prompt_area"

                    anchors.fill: parent
                    visible: root.primarySpeaks
                    cursorShape: Qt.PointingHandCursor

                    onClicked: root.primaryClicked()
                }

                ColumnLayout
                {
                    id: primaryColumn

                    anchors.fill: parent
                    spacing: AppMetrics.gapSmall

                    Item
                    {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                }
            }

            Rectangle
            {
                visible: root.showPrimary
                Layout.fillWidth: true
                Layout.topMargin: root.zoneGap
                Layout.preferredHeight: AppMetrics.borderWidth
                color: AppTheme.colourBorder
            }

            Item
            {
                id: secondaryArea

                Layout.fillWidth: true
                Layout.fillHeight: root.fillSecondary || !root.compact || !root.showPrimary
                Layout.minimumHeight: root.fillSecondary ? secondaryColumn.implicitHeight : 0
                Layout.preferredHeight: secondaryColumn.implicitHeight
                Layout.leftMargin: AppMetrics.contentPadding
                Layout.rightMargin: AppMetrics.contentPadding
                Layout.topMargin: root.showPrimary ? root.zoneGap : 0

                ColumnLayout
                {
                    id: secondaryColumn

                    readonly property real centredY: (card.height - height) / 2 - body.anchors.topMargin - secondaryArea.y

                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: root.fillSecondary ? parent.height : implicitHeight
                    y:
                    {
                        if (!root.showPrimary)
                        {
                            return Math.max(0, secondaryColumn.centredY)
                        }

                        return root.centreSecondary ? Math.max(0, (secondaryArea.height - height) / 2) : 0
                    }
                    spacing: AppMetrics.gapMedium
                }
            }
        }
    }

    RowLayout
    {
        id: actionRow

        anchors.horizontalCenter: card.horizontalCenter
        y: card.y + card.height + AppMetrics.gapLarge
        height: AppMetrics.buttonHeightLarge
        spacing: AppMetrics.gapMedium
    }
}
