import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    property alias posText: meta.posText
    property alias posVisible: meta.posVisible
    property alias counterText: meta.counterText
    property alias masteryTarget: meta.masteryTarget
    property alias masteryFilled: meta.masteryFilled

    property alias primaryContent: primaryColumn.data
    property alias secondaryContent: secondaryColumn.data

    property bool showPrimary: true
    property int primaryShare: 40
    property bool primaryCentred: false
    property bool primaryFillsArea: false
    property bool primaryFitsContent: false
    property bool secondaryFillsArea: false
    property bool reserveActionsSpace: false
    property color swipeColour: "transparent"
    property real swipeStrength: 0

    readonly property real secondaryHeight: secondaryArea.height
    readonly property real stretchHeight: layout.height - meta.height - AppMetrics.borderWidth - layout.spacing * 3
                                          - (root.reserveActionsSpace ? AppMetrics.buttonHeightLarge + layout.spacing : 0)
    readonly property real secondaryVisibleHeight: root.keyboardEdge() - AppMetrics.gapMedium - layout.y - secondaryArea.y

    function focusInSecondary()
    {
        for (let ancestor = root.Window.activeFocusItem; ancestor; ancestor = ancestor.parent)
        {
            if (ancestor === secondaryColumn)
            {
                return true
            }
        }

        return false
    }

    function keyboardEdge()
    {
        const keyboard = Qt.inputMethod.keyboardRectangle

        if (!Qt.inputMethod.visible || keyboard.height <= 0 || !root.focusInSecondary())
        {
            return root.height + AppMetrics.gapMedium
        }

        const scale = Qt.platform.os === "android" ? root.Screen.devicePixelRatio : 1

        return root.mapFromItem(null, 0, keyboard.y / scale).y
    }

    function armMasteryPop()
    {
        meta.armMasteryPop()
    }

    function revealSecondary(top, bottom)
    {
        if (top < secondaryFlickable.contentY)
        {
            secondaryFlickable.contentY = top

            return
        }

        if (bottom > secondaryFlickable.contentY + secondaryArea.height)
        {
            secondaryFlickable.contentY = Math.min(top, bottom - secondaryArea.height)
        }
    }

    Layout.fillWidth: true
    Layout.fillHeight: true
    radius: AppMetrics.radiusMedium
    color: AppTheme.colourPanel
    border.color: root.swipeStrength > 0
        ? Qt.tint(AppTheme.colourBorder, Qt.rgba(root.swipeColour.r, root.swipeColour.g, root.swipeColour.b, root.swipeStrength))
        : AppTheme.colourBorder
    border.width: root.swipeStrength > 0 ? AppMetrics.borderWidth * 3 : AppMetrics.borderWidth

    ColumnLayout
    {
        id: layout

        anchors.fill: parent
        anchors.margins: AppMetrics.cardPadding
        spacing: AppMetrics.gapLarge

        MobileExerciseMeta
        {
            id: meta
        }

        Item
        {
            visible: root.showPrimary
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 1
            Layout.minimumHeight: root.primaryFitsContent ? primaryColumn.implicitHeight : 0
            Layout.maximumHeight: root.primaryFitsContent
                                  ? Math.max(primaryColumn.implicitHeight, root.stretchHeight * root.primaryShare / 100)
                                  : Number.POSITIVE_INFINITY
            Layout.verticalStretchFactor: root.primaryShare

            ColumnLayout
            {
                id: primaryColumn
                anchors.left: parent.left
                anchors.right: parent.right
                height: root.primaryFillsArea ? parent.height : implicitHeight
                y: root.primaryCentred ? (parent.height - height) / 2 : parent.height - height
                spacing: AppMetrics.gapMedium
            }
        }

        MobileSeparator
        {
            visible: root.showPrimary
        }

        Item
        {
            id: secondaryArea

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 1
            Layout.verticalStretchFactor: root.showPrimary ? 100 - root.primaryShare : 100

            Flickable
            {
                id: secondaryFlickable

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: -AppMetrics.motionShake
                anchors.rightMargin: -AppMetrics.motionShake
                anchors.top: parent.top
                anchors.topMargin: root.showPrimary ? 0 : Math.max(0, (parent.height - height) / 2)
                height: Math.max(0, Math.min(secondaryColumn.height, parent.height, root.secondaryVisibleHeight))
                contentWidth: width
                contentHeight: secondaryColumn.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout
                {
                    id: secondaryColumn
                    x: AppMetrics.motionShake
                    width: parent.width - AppMetrics.motionShake * 2
                    height: root.secondaryFillsArea ? Math.max(implicitHeight, secondaryArea.height) : implicitHeight
                    spacing: AppMetrics.gapMedium
                }
            }
        }

        Item
        {
            visible: root.reserveActionsSpace
            Layout.fillWidth: true
            Layout.preferredHeight: AppMetrics.buttonHeightLarge
        }
    }
}
