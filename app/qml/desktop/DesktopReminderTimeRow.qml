import QtQuick
import QtQuick.Layouts

DesktopSettingsRow
{
    id: root

    readonly property int hour: Math.floor(AppReminder.timeMinutes / 60)
    readonly property int minute: AppReminder.timeMinutes % 60

    readonly property var hourNames: Array.from({ length: 24 }, (Unused, Index) => (Index < 10 ? "0" : "") + Index)
    readonly property var minuteNames: Array.from({ length: 12 }, (Unused, Index) => (Index < 2 ? "0" : "") + Index * 5)

    text: qsTr("Remind me at")

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
