import QtQuick

Rectangle
{
    id: root

    required property string text

    property bool interactive: true
    property bool used: false

    signal clicked()
    signal held()

    implicitWidth: label.implicitWidth + AppMetrics.gapCompact * 2
    implicitHeight: label.implicitHeight + AppMetrics.gapSmall * 2

    radius: AppMetrics.radiusMedium
    color: AppTheme.colourPanel
    border.color: AppTheme.colourBorder
    border.width: AppMetrics.borderWidth

    opacity: root.used ? 0.25 : 1.0
    scale: tap.pressed ? 0.95 : 1

    Behavior on opacity { NumberAnimation { duration: AppMetrics.motionBase } }
    Behavior on scale { NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing } }

    Text
    {
        id: label

        anchors.centerIn: parent

        text: root.text
        font.pixelSize: AppMetrics.sessionOptionSize
        font.weight: Font.Medium
        font.family: AppMetrics.fontFamily
        color: AppTheme.colourText
    }

    TapHandler
    {
        id: tap

        onTapped: if (root.interactive && !root.used) root.clicked()
        onLongPressed: root.held()
    }
}
