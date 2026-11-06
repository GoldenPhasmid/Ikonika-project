import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Popup
{
    id: root

    property string heading: ""
    default property alias rows: rowColumn.data

    width: AppMetrics.popoverWidth
    margins: AppMetrics.gapSmall
    padding: AppMetrics.gapTight
    topPadding: root.heading.length > 0 ? 0 : AppMetrics.gapTight

    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder
    }

    contentItem: ColumnLayout
    {
        spacing: 0

        DesktopSectionLabel
        {
            visible: root.heading.length > 0
            text: root.heading
            Layout.leftMargin: AppMetrics.gapCompact
            Layout.topMargin: AppMetrics.gapCompact
            Layout.bottomMargin: AppMetrics.gapSmall
        }

        ColumnLayout
        {
            id: rowColumn

            Layout.fillWidth: true
            spacing: 0
        }
    }

    enter: Transition
    {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AppMetrics.motionFast }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
    }

    exit: Transition
    {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: AppMetrics.motionFast }
    }
}
