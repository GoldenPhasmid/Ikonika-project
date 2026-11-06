import QtQuick

MobileTextInput
{
    id: root

    placeholderText: qsTr("Search")

    font.pixelSize: AppMetrics.fontTitle
    font.weight: Font.DemiBold
    font.family: AppMetrics.fontFamily
    color: AppTheme.colourText
    verticalAlignment: TextInput.AlignVCenter
    clip: true
}
