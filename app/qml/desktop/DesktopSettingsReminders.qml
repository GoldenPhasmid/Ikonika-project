import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    readonly property int hour: Math.floor(AppReminder.timeMinutes / 60)
    readonly property int minute: AppReminder.timeMinutes % 60

    readonly property var hourNames: Array.from({ length: 24 }, (Unused, Index) => (Index < 10 ? "0" : "") + Index)
    readonly property var minuteNames: Array.from({ length: 12 }, (Unused, Index) => (Index < 2 ? "0" : "") + Index * 5)

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

        DesktopSettingsRow
        {
            text: qsTr("Remind me at")
            enabled: AppReminder.enabled
            opacity: AppReminder.enabled ? 1.0 : AppTheme.opacityDisabled

            DesktopSelect
            {
                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth / 2 - AppMetrics.gapTight
                heading: qsTr("Hour")
                options: root.hourNames
                currentIndex: root.hour

                onChosen: function(index) { AppReminder.timeMinutes = index * 60 + root.minute }
            }

            DesktopLabel
            {
                text: ":"
                font.weight: Font.Bold
            }

            DesktopSelect
            {
                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth / 2 - AppMetrics.gapTight
                heading: qsTr("Minute")
                options: root.minuteNames
                currentIndex: Math.round(root.minute / 5) % 12

                onChosen: function(index) { AppReminder.timeMinutes = root.hour * 60 + index * 5 }
            }
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
