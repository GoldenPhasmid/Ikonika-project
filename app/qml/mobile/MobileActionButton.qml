import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property string label

    property string icon: ""
    property string iconSvg: ""
    property string caption: ""
    property string trailing: ""
    property string trailingIconSvg: ""
    property bool filled: true
    property bool interactive: true

    signal clicked()
    signal blockedClicked()

    readonly property bool solid: root.filled && root.interactive

    readonly property color contentColour: !root.interactive
        ? AppTheme.colourMuted
        : (root.filled ? AppTheme.colourPanel : AppTheme.colourText)

    Layout.fillWidth: true
    Layout.preferredHeight: AppMetrics.sessionTileHeightCompact

    radius: AppMetrics.radiusMedium
    color: root.solid ? AppTheme.colourText : (press.pressed && root.interactive ? AppTheme.colourFaint : "transparent")
    border.width: root.solid ? 0 : AppMetrics.borderWidth
    border.color: AppTheme.colourBorder
    scale: press.pressed && root.interactive ? 0.98 : 1

    Behavior on scale
    {
        NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
    }

    RowLayout
    {
        anchors.fill: parent
        anchors.leftMargin: AppMetrics.contentPadding
        anchors.rightMargin: AppMetrics.contentPadding
        spacing: AppMetrics.gapCompact

        Text
        {
            text: root.icon
            visible: root.iconSvg.length === 0 && text.length > 0
            font.pixelSize: 16
        }

        MobileVectorIcon
        {
            visible: root.iconSvg.length > 0
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            svgPath: root.iconSvg
            color: root.contentColour
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: 2

            Text
            {
                Layout.fillWidth: true

                text: root.label
                font.pixelSize: AppMetrics.fontTitle
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: root.contentColour
                elide: Text.ElideRight
            }

            Text
            {
                Layout.fillWidth: true
                visible: root.caption.length > 0

                text: root.caption
                font.pixelSize: AppMetrics.fontMeta
                font.family: AppMetrics.fontFamily
                color: root.contentColour
                opacity: root.interactive ? AppTheme.opacitySecondary : 1.0
                elide: Text.ElideRight
            }
        }

        Text
        {
            text: root.trailing
            visible: text.length > 0
            font.pixelSize: AppMetrics.fontTitle
            font.weight: Font.Medium
            font.family: AppMetrics.fontFamily
            color: root.contentColour
        }

        MobileVectorIcon
        {
            visible: root.trailingIconSvg.length > 0
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            svgPath: root.trailingIconSvg
            color: root.contentColour
        }
    }

    MouseArea
    {
        id: press

        anchors.fill: parent
        onClicked: root.interactive ? root.clicked() : root.blockedClicked()
    }
}
