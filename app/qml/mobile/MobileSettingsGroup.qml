import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property string title

    property string flagCode: ""
    property color outlineColour: AppTheme.colourBorder
    property real outlineWidth: AppMetrics.borderWidth
    property bool squareTopRight: false

    default property alias rows: card.data

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    RowLayout
    {
        visible: root.title.length > 0
        spacing: AppMetrics.gapSmall

        LanguageFlag
        {
            visible: root.flagCode.length > 0
            code: root.flagCode
            implicitWidth: AppMetrics.flagWidth / 2
            implicitHeight: AppMetrics.flagWidth * 3 / 8
        }

        MobileSectionLabel
        {
            text: root.title
        }
    }

    Rectangle
    {
        Layout.fillWidth: true
        implicitHeight: card.implicitHeight

        radius: AppMetrics.radiusMedium
        topRightRadius: root.squareTopRight ? 0 : AppMetrics.radiusMedium
        color: AppTheme.colourPanel
        border.color: root.outlineColour
        border.width: root.outlineWidth
        clip: true

        ColumnLayout
        {
            id: card

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0
        }
    }
}
