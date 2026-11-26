import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property alias heading: headingLabel.text
    property bool showClose: true
    property real maxHeightFraction: 0.5
    property real bottomInset: 0

    readonly property real footerSpace: footerHolder.visible ? footerHolder.implicitHeight + sheetColumn.spacing : 0
    readonly property real maximumBodyHeight: Math.max(0, height * maxHeightFraction
        - (headingRow.visible ? headingRow.implicitHeight + sheetColumn.spacing : 0)
        - AppMetrics.contentPadding * 3 - bottomInset - root.footerSpace)

    default property alias content: body.data
    property alias footer: footerHolder.data
    property alias overlays: overlayLayer.data

    signal dismissed()
    signal hidden()

    property real reveal: 0
    property bool leaving: false
    property bool hideAfterLeaving: false
    property bool closingItself: false

    function conceal()
    {
        root.closingItself = true
        root.visible = false
        root.closingItself = false
    }

    function present()
    {
        exitAnimation.stop()
        root.leaving = false

        if (root.visible)
        {
            enterAnimation.restart()
        }

        root.visible = true
    }

    function dismiss()
    {
        if (root.leaving)
        {
            return
        }

        root.leave(false)
    }

    function hide()
    {
        if (root.leaving)
        {
            root.hideAfterLeaving = true

            return
        }

        if (root.reveal === 0)
        {
            root.conceal()
            root.hidden()

            return
        }

        root.leave(true)
    }

    function leave(hideAfterwards)
    {
        root.hideAfterLeaving = hideAfterwards
        root.leaving = true

        enterAnimation.stop()
        exitAnimation.start()
    }

    anchors.fill: parent

    Component.onCompleted:
    {
        if (!root.visible)
        {
            return
        }

        root.forceActiveFocus()
        Qt.inputMethod.hide()
        enterAnimation.restart()
    }

    onVisibleChanged:
    {
        if (root.visible)
        {
            root.forceActiveFocus()
            Qt.inputMethod.hide()

            root.leaving = false
            enterAnimation.restart()

            return
        }

        root.reveal = 0

        if (!root.closingItself)
        {
            exitAnimation.stop()
            root.leaving = false
            root.dismissed()
        }
    }

    NumberAnimation
    {
        id: enterAnimation

        target: root
        property: "reveal"
        from: 0
        to: 1
        duration: AppMetrics.motionSlow
        easing.type: AppMetrics.motionEasing
    }

    NumberAnimation
    {
        id: exitAnimation

        target: root
        property: "reveal"
        to: 0
        duration: AppMetrics.motionBase
        easing.type: AppMetrics.motionEasing

        onFinished:
        {
            root.leaving = false

            if (root.hideAfterLeaving)
            {
                root.conceal()
                root.hidden()

                return
            }

            root.dismissed()
        }
    }

    Rectangle
    {
        anchors.fill: parent
        color: AppTheme.colourText
        opacity: 0.4 * root.reveal

        MouseArea
        {
            anchors.fill: parent
            enabled: root.reveal > 0
            onClicked: root.dismiss()
        }
    }

    Rectangle
    {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -height * (1 - root.reveal)

        width: Math.min(parent.width, AppMetrics.shellMaximumWidth)
        enabled: !root.leaving

        height: Math.min(parent.height * root.maxHeightFraction,
                        sheetColumn.implicitHeight + AppMetrics.contentPadding * 3 + root.bottomInset)
        topLeftRadius: AppMetrics.radiusLarge
        topRightRadius: AppMetrics.radiusLarge
        bottomLeftRadius: 0
        bottomRightRadius: 0
        color: AppTheme.colourPanel
        clip: true

        MouseArea
        {
            anchors.fill: parent
        }

        ColumnLayout
        {
            id: sheetColumn

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppMetrics.contentPadding * 1.5
            spacing: AppMetrics.gapWide

            RowLayout
            {
                id: headingRow

                Layout.fillWidth: true
                visible: headingLabel.text.length > 0 || root.showClose
                spacing: AppMetrics.gapSmall

                Text
                {
                    id: headingLabel

                    Layout.fillWidth: true
                    font.pixelSize: AppMetrics.fontTitle
                    font.weight: Font.DemiBold
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourText
                    elide: Text.ElideRight
                }

                MobileIconButton
                {
                    visible: root.showClose

                    iconName: "close"
                    text: qsTr("Close")

                    onClicked: root.dismiss()
                }
            }

            Flickable
            {
                id: bodyScroll

                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(body.implicitHeight, root.maximumBodyHeight)

                contentWidth: width
                contentHeight: body.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout
                {
                    id: body

                    width: bodyScroll.width
                    spacing: AppMetrics.gapWide
                }
            }

            ColumnLayout
            {
                id: footerHolder

                Layout.fillWidth: true
                visible: footerHolder.children.length > 0
                spacing: AppMetrics.gapWide
            }
        }
    }

    Item
    {
        id: overlayLayer

        anchors.fill: parent
    }
}
