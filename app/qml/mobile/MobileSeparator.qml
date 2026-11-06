import QtQuick
import QtQuick.Layouts

Rectangle
{
    property bool strong: false

    Layout.fillWidth: true
    Layout.preferredHeight: AppMetrics.borderWidth
    implicitHeight: AppMetrics.borderWidth
    color: AppTheme.colourBorder
    opacity: strong ? 1.0 : 0.35
}
