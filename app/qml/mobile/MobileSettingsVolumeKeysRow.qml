import QtQuick

MobileSettingsRow
{
    title: qsTr("Volume keys turn pages")

    MobileSettingsToggle
    {
        checked: AppReaderSettings.volumeKeysTurnPages

        onToggled: AppReaderSettings.volumeKeysTurnPages = !AppReaderSettings.volumeKeysTurnPages
    }
}
