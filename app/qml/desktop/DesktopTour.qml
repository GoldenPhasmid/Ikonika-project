import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes

FocusScope
{
    id: root

    property var stops: []
    property bool running: false
    property int index: 0
    property rect targetRect: Qt.rect(root.width / 2, root.height / 2, 0, 0)
    property Item targetItem: null

    signal finished()

    readonly property var stop: root.index < root.stops.length ? root.stops[root.index] : null
    readonly property bool lastStop: root.index >= root.stops.length - 1
    readonly property real spotPadding: AppMetrics.gapSmall
    readonly property real spotRadius: AppMetrics.radiusLarge
    readonly property real scrimBorder: Math.max(root.width, root.height) * 2
    readonly property int followInterval: 200

    visible: root.running
    focus: root.running

    function start()
    {
        root.index = 0
        root.running = true
        root.locate()
        root.forceActiveFocus()

        console.info("Tour started,", root.stops.length, "stops")
    }

    function stopRunning()
    {
        root.running = false
    }

    function finish()
    {
        if (!root.running)
        {
            return
        }

        console.info("Tour finished at stop", root.index + 1, "of", root.stops.length)

        root.running = false
        root.finished()
    }

    function advance()
    {
        if (root.lastStop)
        {
            root.finish()

            return
        }

        root.index++
        root.locate()
    }

    function findItem(item, name)
    {
        for (const child of item.children)
        {
            if (!child.visible || child === root)
            {
                continue
            }

            if (child.objectName === name)
            {
                return child
            }

            const found = root.findItem(child, name)

            if (found)
            {
                return found
            }
        }

        return null
    }

    function locate()
    {
        if (!root.running || !root.stop)
        {
            return
        }

        root.targetItem = root.findItem(root.parent, root.stop.target)

        if (!root.targetItem)
        {
            console.warn("Tour stop has no visible target:", root.stop.target)
            root.advance()

            return
        }

        root.measure()
    }

    function measure()
    {
        if (!root.running || !root.targetItem)
        {
            return
        }

        const bounds = root.targetItem.mapToItem(root, 0, 0, root.targetItem.width, root.targetItem.height)
        const left = bounds.x - root.spotPadding
        const top = bounds.y - root.spotPadding
        const width = bounds.width + root.spotPadding * 2
        const height = bounds.height + root.spotPadding * 2
        const current = root.targetRect

        if (left === current.x && top === current.y && width === current.width && height === current.height)
        {
            return
        }

        root.targetRect = Qt.rect(left, top, width, height)
    }

    onWidthChanged: Qt.callLater(root.measure)
    onHeightChanged: Qt.callLater(root.measure)

    Timer
    {
        interval: root.followInterval
        repeat: true
        running: root.running

        onTriggered: root.measure()
    }

    Keys.onReturnPressed: root.advance()
    Keys.onEnterPressed: root.advance()
    Keys.onEscapePressed: root.finish()

    MouseArea
    {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: root.advance()
        onWheel: function(wheel) { wheel.accepted = true }
    }

    Rectangle
    {
        x: root.targetRect.x - root.scrimBorder
        y: root.targetRect.y - root.scrimBorder
        width: root.targetRect.width + root.scrimBorder * 2
        height: root.targetRect.height + root.scrimBorder * 2
        radius: root.scrimBorder + root.spotRadius
        color: "transparent"
        border.width: root.scrimBorder
        border.color: AppTheme.colourScrim

        Behavior on x { NumberAnimation { duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing } }
        Behavior on y { NumberAnimation { duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing } }
        Behavior on width { NumberAnimation { duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing } }
        Behavior on height { NumberAnimation { duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing } }
    }

    Rectangle
    {
        x: root.targetRect.x
        y: root.targetRect.y
        width: root.targetRect.width
        height: root.targetRect.height
        radius: root.spotRadius
        color: "transparent"
        border.width: AppMetrics.selectedBorderWidth
        border.color: AppTheme.colourAccent

        Behavior on x { NumberAnimation { duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing } }
        Behavior on y { NumberAnimation { duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing } }
        Behavior on width { NumberAnimation { duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing } }
        Behavior on height { NumberAnimation { duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing } }
    }

    Item
    {
        id: bubble

        objectName: "tour_bubble"

        readonly property real gap: AppMetrics.gapMedium + caret.height
        readonly property bool below: root.targetRect.y + root.targetRect.height + bubble.gap + bubble.height
                                      <= root.height - AppMetrics.gapLarge
        readonly property real targetCentre: root.targetRect.x + root.targetRect.width / 2

        x: Math.max(AppMetrics.gapLarge, Math.min(root.width - bubble.width - AppMetrics.gapLarge,
                                                  bubble.targetCentre - bubble.width / 2))
        y: bubble.below ? root.targetRect.y + root.targetRect.height + bubble.gap
                        : root.targetRect.y - bubble.height - bubble.gap
        width: AppMetrics.tourCardWidth
        height: card.height

        Accessible.role: Accessible.Dialog
        Accessible.name: root.stop ? root.stop.title : ""
        Accessible.description: root.stop ? root.stop.text : ""

        Behavior on x { NumberAnimation { duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing } }
        Behavior on y { NumberAnimation { duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing } }

        Rectangle
        {
            id: card

            width: parent.width
            height: content.implicitHeight + AppMetrics.cardPadding * 2
            radius: AppMetrics.radiusLarge
            color: AppTheme.colourPanel
            border.width: AppMetrics.borderWidth
            border.color: AppTheme.colourBorder
        }

        Shape
        {
            id: caret

            readonly property real halfWidth: AppMetrics.gapSmall + AppMetrics.gapTight

            x: Math.max(AppMetrics.radiusLarge, Math.min(bubble.width - AppMetrics.radiusLarge - caret.width,
                                                         bubble.targetCentre - bubble.x - caret.halfWidth))
            y: bubble.below ? -caret.height + AppMetrics.borderWidth : card.height - AppMetrics.borderWidth
            width: caret.halfWidth * 2
            height: AppMetrics.gapSmall
            preferredRendererType: Shape.CurveRenderer

            ShapePath
            {
                fillColor: AppTheme.colourPanel
                strokeColor: AppTheme.colourBorder
                strokeWidth: AppMetrics.borderWidth

                startX: 0
                startY: bubble.below ? caret.height : 0

                PathLine { x: caret.halfWidth; y: bubble.below ? 0 : caret.height }
                PathLine { x: caret.width; y: bubble.below ? caret.height : 0 }
            }
        }

        ColumnLayout
        {
            id: content

            x: AppMetrics.cardPadding
            y: AppMetrics.cardPadding
            width: bubble.width - AppMetrics.cardPadding * 2
            spacing: AppMetrics.gapMedium

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapMedium

                Rectangle
                {
                    Layout.preferredWidth: AppMetrics.buttonHeight
                    Layout.preferredHeight: AppMetrics.buttonHeight
                    radius: AppMetrics.radiusMedium
                    color: AppTheme.faded(AppTheme.colourAccent, 0.16)

                    DesktopIcon
                    {
                        anchors.centerIn: parent
                        name: root.stop ? root.stop.icon : ""
                        color: AppTheme.colourAccent
                    }
                }

                ColumnLayout
                {
                    Layout.fillWidth: true
                    spacing: 0

                    DesktopLabel
                    {
                        Layout.fillWidth: true
                        text: qsTr("Step %1 of %2").arg(root.index + 1).arg(root.stops.length)
                        font.pixelSize: AppMetrics.fontSmall
                        color: AppTheme.colourMuted
                    }

                    DesktopLabel
                    {
                        Layout.fillWidth: true
                        text: root.stop ? root.stop.title : ""
                        font.pixelSize: AppMetrics.fontSubtitle
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                }
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.stop ? root.stop.text : ""
                color: AppTheme.colourSubtle
                wrapMode: Text.WordWrap
                lineHeight: 1.2
            }

            DesktopDivider
            {
                Layout.fillWidth: true
                Layout.topMargin: AppMetrics.gapTight
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapSmall

                Row
                {
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapTight

                    Repeater
                    {
                        model: root.stops.length

                        delegate: Rectangle
                        {
                            required property int index

                            anchors.verticalCenter: parent.verticalCenter
                            width: index === root.index ? AppMetrics.gapLarge : AppMetrics.gapSmall - AppMetrics.gapTight
                            height: AppMetrics.gapSmall - AppMetrics.gapTight
                            radius: height / 2
                            color: index <= root.index ? AppTheme.colourAccent : AppTheme.colourBorder

                            Behavior on width { NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing } }
                            Behavior on color { ColorAnimation { duration: AppMetrics.motionBase } }
                        }
                    }
                }

                DesktopButton
                {
                    objectName: "tour_skip"

                    visible: !root.lastStop
                    tone: "ghost"
                    text: qsTr("Skip")
                    keyHint: qsTr("Esc")

                    onClicked: root.finish()
                }

                DesktopButton
                {
                    objectName: "tour_next"

                    tone: "primary"
                    text: root.lastStop ? qsTr("Done") : qsTr("Next")
                    keyHint: qsTr("Enter")

                    onClicked: root.advance()
                }
            }
        }
    }
}
