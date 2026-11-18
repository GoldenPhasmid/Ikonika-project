import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T

TextField
{
    id: root

    property bool showsFocus: root.activeFocus

    leftPadding: AppMetrics.contentPadding
    rightPadding: AppMetrics.contentPadding

    font.pixelSize: AppMetrics.fontLabel
    font.family: AppMetrics.fontFamily
    color: root.enabled ? AppTheme.colourText : AppTheme.colourMuted

    placeholderTextColor: AppTheme.colourMuted
    inputMethodHints: Qt.ImhNoPredictiveText

    T.ContextMenu.menu: null

    MobileKeyboardTap { editor: root }
    MobileKeyboardRelease { editor: root }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: "transparent"

        border.width: AppMetrics.borderWidth
        border.color: root.showsFocus ? AppTheme.colourAccent : AppTheme.colourBorder
    }
}
