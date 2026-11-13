import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    DesktopSettingsGroup
    {
        title: qsTr("Daily notification")

        DesktopSettingsRow
        {
            text: qsTr("Daily reminder")
            description: !AppReminder.trayAvailable
                ? qsTr("Needs the system tray, which is not available")
                : (AppReminder.enabled && !AppReminder.hasNotificationPermission()
                   ? qsTr("Blocked in system notification settings") : "")

            DesktopToggle
            {
                accessibleName: qsTr("Daily reminder")
                on: AppReminder.enabled
                enabled: AppReminder.trayAvailable

                onClicked: AppReminder.enabled = !AppReminder.enabled
            }
        }

        DesktopReminderTimeRow
        {
            enabled: AppReminder.enabled
            opacity: AppReminder.enabled ? 1.0 : AppTheme.opacityDisabled
        }

        DesktopSettingsRow
        {
            text: qsTr("Message")
            lastRow: true
            enabled: AppReminder.enabled
            opacity: AppReminder.enabled ? 1.0 : AppTheme.opacityDisabled

            DesktopTextField
            {
                Layout.preferredWidth: AppMetrics.settingsControlWidth + AppMetrics.gapLarge * 3
                text: AppReminder.message
                placeholderText: AppReminder.messageOrDefault
                maximumLength: AppMetrics.maximumCardFieldLength

                onEditingFinished: if (text !== AppReminder.message) AppReminder.message = text
            }
        }
    }
}
