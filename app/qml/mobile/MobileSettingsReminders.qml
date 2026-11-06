import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var timeSheet

    function formatReminderTime(minutes)
    {
        const when = new Date(2000, 0, 1, Math.floor(minutes / 60), minutes % 60)
        const pattern = Qt.locale().timeFormat(Locale.ShortFormat).replace(/[:.]?s+/gi, "").trim()

        return Qt.formatTime(when, pattern)
    }

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

        MobileSettingsRow
        {
            title: qsTr("Remind me at")

            MobileSettingsSelect
            {
                label: root.formatReminderTime(AppReminder.timeMinutes)
                interactive: AppReminder.enabled
                minimumWidth: 96

                onClicked: root.timeSheet.open(AppReminder.timeMinutes,
                                               function(picked)
                                               {
                                                   AppReminder.timeMinutes = picked
                                               })
            }
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
