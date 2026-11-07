import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property var tags: []
    property string counterText: ""
    property int masteryTarget: 0
    property int masteryFilled: 0
    property bool showPrimary: true
    property bool fillSecondary: false
    property bool primarySpeaks: false
    readonly property real primaryHeight: primaryArea.height
    readonly property real secondaryHeight: secondaryArea.height

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
    readonly property real zoneGap: root.compact ? AppMetrics.gapMedium : AppMetrics.gapLarge

    Rectangle
    {
        id: card

        x: (root.width - width) / 2
        y: Math.max(AppMetrics.gapLarge, root.cardBottom - height)
        width: Math.min(AppMetrics.sessionCardWidth, root.width - AppMetrics.contentPadding * 2)
        height: Math.max(0, Math.min(root.designHeight, root.cardBottom - AppMetrics.gapLarge))

        radius: AppMetrics.radiusLarge
        color: AppTheme.colourCard
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder

        Item
        {
            id: overlayLayer

            anchors.fill: parent
        }

        Item
        {
            id: body

            anchors.fill: parent
            anchors.topMargin: root.compact ? AppMetrics.gapMedium : AppMetrics.gapLarge
            anchors.bottomMargin: root.compact ? AppMetrics.gapMedium : AppMetrics.contentPadding
            readonly property real splitHeight: Math.max(0, (height - header.height - AppMetrics.borderWidth) / 2)

            Item
            {
                id: header

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: AppMetrics.contentPadding
                height: AppMetrics.buttonHeight

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

            Flickable
            {
                id: primaryArea

                visible: root.showPrimary
                anchors.top: header.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: AppMetrics.contentPadding
                anchors.rightMargin: AppMetrics.contentPadding
                height: Math.max(0, body.splitHeight - root.zoneGap)
                contentWidth: width
                contentHeight: Math.max(height, primaryColumn.implicitHeight)
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                onContentHeightChanged: contentY = Math.max(0, contentHeight - height)
                onHeightChanged: contentY = Math.max(0, contentHeight - height)

                ScrollBar.vertical: DesktopScrollBar {}

                MouseArea
                {
                    objectName: "exercise_prompt_area"

                    width: primaryArea.width
                    height: primaryArea.contentHeight
                    visible: root.primarySpeaks
                    cursorShape: Qt.PointingHandCursor

                    onClicked: root.primaryClicked()
                }

                ColumnLayout
                {
                    id: primaryColumn

                    width: primaryArea.width
                    height: primaryArea.contentHeight
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
                id: divider

                visible: root.showPrimary
                anchors.left: parent.left
                anchors.right: parent.right
                y: header.height + body.splitHeight
                height: AppMetrics.borderWidth
                color: AppTheme.colourBorder
            }

            Flickable
            {
                id: secondaryArea

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: AppMetrics.contentPadding
                anchors.rightMargin: AppMetrics.contentPadding
                y: root.showPrimary ? divider.y + divider.height + root.zoneGap : header.height
                height: Math.max(0, body.height - y)
                contentWidth: width
                contentHeight: Math.max(height, secondaryColumn.implicitHeight)
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                ScrollBar.vertical: DesktopScrollBar {}

                ColumnLayout
                {
                    id: secondaryColumn

                    width: secondaryArea.width
                    height: root.fillSecondary ? secondaryArea.contentHeight : implicitHeight
                    y: root.showPrimary ? 0 : Math.max(0, (secondaryArea.height - height) / 2)
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
