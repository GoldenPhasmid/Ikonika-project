import QtQuick

// The wireframes' .meta and .kicker: small uppercase mono text with wide letter spacing, used for
// counts, captions and section headings. `kicker` is the bolder heading form of the same thing.
Text
{
    property bool kicker: false

    color: AppTheme.colourMuted
    font.pixelSize: AppMetrics.fontMeta
    font.letterSpacing: AppMetrics.trackingWide
    font.bold: kicker
    font.capitalization: Font.AllUppercase
}
