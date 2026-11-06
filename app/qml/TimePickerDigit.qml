import QtQuick
import QtQuick.Controls.Basic

Text
{
    id: root

    required property int index

    property int step: 1

    readonly property int shown: root.index * root.step

    text: root.shown < 10 ? "0" + root.shown : root.shown

    font.pixelSize: AppMetrics.fontTitle
    font.weight: Font.Medium
    color: AppTheme.colourText

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter

    opacity: 0.35 + Math.max(0, 1 - Math.abs(Tumbler.displacement)) * 0.65
}
