import QtQuick

Rectangle
{
    id: root

    required property string message

    color: AppTheme.colourPanel

    MouseArea
    {
        anchors.fill: parent
    }

    Column
    {
        anchors.centerIn: parent
        width: parent.width - AppMetrics.gapLarge * 2
        spacing: AppMetrics.gapLarge

        Item
        {
            anchors.horizontalCenter: parent.horizontalCenter
            width: AppMetrics.switchGlyphSize
            height: AppMetrics.switchGlyphSize

            Rectangle
            {
                anchors.fill: parent
                radius: width / 2
                color: AppTheme.faded(AppTheme.colourPanel, 0)
                border.width: AppMetrics.borderWidth
                border.color: AppTheme.colourBorder
            }

            Item
            {
                id: orbit

                anchors.fill: parent

                Rectangle
                {
                    x: (orbit.width - width) / 2
                    y: -height / 2
                    width: AppMetrics.switchGlyphSize / 4
                    height: width
                    radius: width / 2
                    color: AppTheme.colourAccent
                }

                Rectangle
                {
                    x: (orbit.width - width) / 2
                    y: orbit.height - height / 2
                    width: AppMetrics.switchGlyphSize / 4
                    height: width
                    radius: width / 2
                    color: AppTheme.colourText
                }

                SequentialAnimation
                {
                    running: root.visible && AppMetrics.motionReveal > 0
                    loops: Animation.Infinite

                    RotationAnimator { target: orbit; from: 0; to: 180; duration: AppMetrics.motionReveal; easing.type: Easing.InOutCubic }
                    RotationAnimator { target: orbit; from: 180; to: 360; duration: AppMetrics.motionReveal; easing.type: Easing.InOutCubic }
                }
            }
        }

        Text
        {
            width: parent.width

            text: root.message
            color: AppTheme.colourText
            font.pixelSize: AppMetrics.fontTitle
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
