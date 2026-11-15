import QtQuick
import QtQuick.Controls.Basic

AbstractButton
{
    id: root

    property string iconName: ""
    property color tint: AppTheme.colourCoverMuted

    hoverEnabled: true
    padding: 0
    leftPadding: AppMetrics.gapSmall
    rightPadding: AppMetrics.gapSmall

    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: AppMetrics.badgeHeight

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Open deck %1").arg(root.text)

    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusSmall
        color: root.hovered ? Qt.tint(root.tint, AppTheme.faded(AppTheme.colourText, AppTheme.alphaFaint)) : root.tint
        border.width: root.visualFocus ? AppMetrics.focusRingWidth : 0
        border.color: AppTheme.colourAccent
        clip: true

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        CoverStripe {}
    }

    contentItem: Row
    {
        spacing: AppMetrics.gapTight

        DesktopIcon
        {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.iconName.length > 0
            name: root.iconName
            width: AppMetrics.iconSizeSmall
            height: AppMetrics.iconSizeSmall
            color: AppTheme.colourCoverInk
        }

        DesktopLabel
        {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font.pixelSize: AppMetrics.fontMicro
            font.weight: Font.Bold
            color: AppTheme.colourCoverInk
        }
    }
}
