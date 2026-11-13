import QtQuick
import QtQuick.Layouts

DesktopButton
{
    id: root

    property var shortcutKeys: []

    Layout.fillWidth: true
    Layout.preferredWidth: AppMetrics.actionButtonWidth
    large: true

    Shortcut
    {
        sequences: root.shortcutKeys
        enabled: root.visible && root.enabled && root.shortcutKeys.length > 0
        context: Qt.WindowShortcut

        onActivated: root.clicked()
    }
}
