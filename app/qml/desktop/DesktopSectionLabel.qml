import QtQuick

DesktopLabel
{
    font.pixelSize: AppMetrics.fontMicro
    font.weight: Font.Bold
    font.letterSpacing: AppMetrics.trackingWide
    font.capitalization: Font.AllUppercase
    color: AppTheme.colourMuted

    Accessible.role: Accessible.Heading
    Accessible.name: text
}
