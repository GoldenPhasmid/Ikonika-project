import QtQuick
import QtQuick.Layouts

Text
{
    Layout.fillWidth: true
    Layout.leftMargin: AppMetrics.contentPadding
    Layout.topMargin: AppMetrics.gapSmall
    Layout.bottomMargin: AppMetrics.gapTight
    font.pixelSize: AppMetrics.fontMicro
    font.weight: Font.DemiBold
    font.family: AppMetrics.fontFamily
    font.letterSpacing: AppMetrics.trackingWide
    font.capitalization: Font.AllUppercase
    color: AppTheme.colourMuted
}
