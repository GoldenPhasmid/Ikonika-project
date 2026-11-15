import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T

TextField
{
    id: root

    leftPadding: AppMetrics.contentPadding
    rightPadding: AppMetrics.contentPadding

    font.pixelSize: AppMetrics.fontLabel
    font.family: AppMetrics.fontFamily
    color: root.enabled ? AppTheme.colourText : AppTheme.colourMuted

    placeholderTextColor: AppTheme.colourMuted
    inputMethodHints: Qt.ImhNoPredictiveText

    T.ContextMenu.menu: null

    MobileKeyboardTap { editor: root }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: "transparent"

        border.width: AppMetrics.borderWidth
        border.color: root.activeFocus ? AppTheme.colourAccent : AppTheme.colourBorder
    }

    Connections
    {
        target: Qt.inputMethod

        function onVisibleChanged()
        {
            if (!Qt.inputMethod.visible && root.activeFocus)
            {
                Qt.inputMethod.reset()
                root.focus = false
            }
        }
    }
}
