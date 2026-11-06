import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Item
{
    id: root

    property alias heading: headingLabel.text
    property bool showClose: true
    property real maxHeightFraction: 0.85

    default property alias content: body.data
    property alias overlays: overlayLayer.data

    signal dismissed()
    signal hidden()

    property real reveal: 0
    property bool leaving: false
    property bool hideAfterLeaving: false

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
            root.visible = false
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
                root.visible = false
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
            onClicked: root.dismiss()
        }
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -height * (1 - root.reveal)

        enabled: !root.leaving

        height: Math.min(parent.height * root.maxHeightFraction,
                        sheetColumn.implicitHeight + AppMetrics.contentPadding * 3)
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
                Layout.fillWidth: true
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

                MobileActionIconButton
                {
                    visible: root.showClose

                    iconSvg: Icons.Icons.close.path

                    onClicked: root.dismiss()
                }
            }

            ColumnLayout
            {
                id: body

                Layout.fillWidth: true
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
