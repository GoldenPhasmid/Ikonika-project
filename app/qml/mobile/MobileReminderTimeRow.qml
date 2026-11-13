import QtQuick
import QtQuick.Layouts

MobileSettingsRow
{
    id: root

    required property var timeSheet
    property bool interactive: true

    function formatReminderTime(minutes)
    {
        const when = new Date(2000, 0, 1, Math.floor(minutes / 60), minutes % 60)
        const pattern = Qt.locale().timeFormat(Locale.ShortFormat).replace(/[:.]?s+/gi, "").trim()

        return Qt.formatTime(when, pattern)
    }

    title: qsTr("Remind me at")

    MobileSettingsSelect
    {
        label: root.formatReminderTime(AppReminder.timeMinutes)
        interactive: root.interactive
        minimumWidth: 96

        onClicked: root.timeSheet.open(AppReminder.timeMinutes,
                                       function(picked)
                                       {
                                           AppReminder.timeMinutes = picked
                                       })
    }
}
