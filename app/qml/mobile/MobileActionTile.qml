import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    property string iconName: ""
    property string label: ""
    property string holdLabel: ""
    property bool destructive: false
    property bool holdToConfirm: false
    property real held: 0

    signal clicked()
    signal confirmed()

    readonly property color contentColour: root.destructive ? AppTheme.colourWrong : AppTheme.colourText

    Layout.fillWidth: true
    Layout.preferredWidth: 1
    implicitHeight: AppMetrics.buttonHeightLarge

    radius: AppMetrics.radiusMedium
    color: tap.pressed && !root.holdToConfirm ? AppTheme.colourPressed : AppTheme.colourFaint
    clip: true

    Accessible.role: Accessible.Button
    Accessible.name: root.held > 0 && root.holdLabel.length > 0 ? root.holdLabel : root.label
    Accessible.onPressAction: root.holdToConfirm ? root.confirmed() : root.clicked()

    Rectangle
    {
        width: parent.width * root.held
        height: parent.height
        color: AppTheme.faded(AppTheme.colourWrong, AppTheme.alphaStrong)
    }

    Row
    {
        id: content

        anchors.centerIn: parent
        spacing: AppMetrics.gapSmall

        DesktopIcon
        {
            id: tileIcon

            anchors.verticalCenter: parent.verticalCenter
            name: root.iconName
            width: AppMetrics.headerActionIconSize
            height: AppMetrics.headerActionIconSize
            color: root.contentColour
        }

        Text
        {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, root.width - AppMetrics.gapSmall * 2 - tileIcon.width - content.spacing)
            text: root.held > 0 && root.holdLabel.length > 0 ? root.holdLabel : root.label
            font.pixelSize: AppMetrics.fontLabel
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            color: root.contentColour
            elide: Text.ElideRight
        }
    }

    NumberAnimation
    {
        id: holdUp

        target: root
        property: "held"
        to: 1.0
        duration: AppMetrics.holdDuration * (1 - root.held)

        onFinished:
        {
            root.held = 0
            root.confirmed()
        }
    }

    NumberAnimation
    {
        id: holdDown

        target: root
        property: "held"
        to: 0.0
        duration: AppMetrics.motionFast
    }

    TapHandler
    {
        id: tap

        gesturePolicy: TapHandler.ReleaseWithinBounds

        onTapped: if (!root.holdToConfirm) root.clicked()

        onPressedChanged:
        {
            if (!root.holdToConfirm)
            {
                return
            }

            if (tap.pressed)
            {
                holdDown.stop()
                holdUp.restart()

                return
            }

            holdUp.stop()

            if (root.held > 0)
            {
                holdDown.restart()
            }
        }
    }
}
