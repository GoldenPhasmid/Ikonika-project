import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var timeSheet

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    MobileSettingsGroup
    {
        title: qsTr("REMINDERS")

        MobileSettingsRow
        {
            title: qsTr("Daily reminder")
            description: (AppReminder.enabled && !AppReminder.hasNotificationPermission())
                ? qsTr("Blocked in system notification settings")
                : ""

            MobileSettingsToggle
            {
                checked: AppReminder.enabled
                interactive: AppReminder.trayAvailable

                onToggled: AppReminder.enabled = !AppReminder.enabled
            }
        }

        MobileReminderTimeRow
        {
            timeSheet: root.timeSheet
            interactive: AppReminder.enabled
        }

        MobileSettingsTextField
        {
            title: qsTr("Message")
            lastRow: true

            text: AppReminder.message
            placeholderText: AppReminder.messageOrDefault

            onEdited: function(newText) { AppReminder.message = newText }
        }
    }
}
