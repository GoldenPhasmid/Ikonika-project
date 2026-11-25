import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property string tone: "secondary"
    property string iconName: ""
    property color iconColour: root.contentColour
    property bool iconFilled: false
    property string trailingIconName: ""
    property string keyHint: ""
    property bool large: false
    property real topRadius: AppMetrics.radiusMedium
    property real bottomRadius: AppMetrics.radiusMedium
    property bool iconOnly: false
    property bool hintOnControl: false
    property bool alignHintRight: false
    property bool onCover: false
    property bool hairlineOutline: false

    readonly property color inkColour: root.onCover ? AppTheme.colourCoverInk : AppTheme.colourText
    readonly property color paperColour: root.onCover ? AppTheme.colourCoverPaper : AppTheme.colourPanel
    readonly property color edgeColour: root.onCover ? AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaHalf) : AppTheme.colourBorder
    readonly property color hoverColour: root.onCover ? AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaSoft) : AppTheme.colourHover
    readonly property color pressedColour: root.onCover ? AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaStrong) : AppTheme.colourPressed

    readonly property bool showsHint: root.keyHint.length > 0 && !root.compactContent && !AppMetrics.touchInput
                                      && (!root.hintOnControl || AppModifierKeys.controlHeld)

    readonly property bool compactContent: root.iconOnly && root.iconName.length > 0
    readonly property bool showsText: root.text.length > 0 && !root.compactContent
    readonly property real textPadding: root.text.length > 0 ? AppMetrics.gapMedium : 0

    readonly property real fullWidth:
    {
        const widths = [
            root.iconName.length > 0 ? leadingIcon.implicitWidth : -1,
            root.text.length > 0 ? label.implicitWidth : -1,
            root.trailingIconName.length > 0 ? trailingIcon.implicitWidth : -1,
            root.keyHint.length > 0 && !root.hintOnControl && !AppMetrics.touchInput ? hint.implicitWidth : -1
        ].filter(Width => Width >= 0)

        const content = widths.reduce((Sum, Width) => Sum + Width, 0) + Math.max(0, widths.length - 1) * contentRow.spacing

        return Math.max(root.implicitHeight, content + root.textPadding * 2)
    }

    readonly property bool filled: root.tone === "primary" || root.tone === "danger"

    readonly property color fillColour:
    {
        if (root.tone === "primary")
        {
            return root.inkColour
        }

        if (root.tone === "danger")
        {
            return AppTheme.colourWrong
        }

        return "transparent"
    }

    readonly property color contentColour:
    {
        if (root.filled)
        {
            return root.paperColour
        }

        return root.tone === "dangerOutline" ? AppTheme.colourWrong : root.inkColour
    }

    implicitHeight: root.large ? AppMetrics.buttonHeightLarge : AppMetrics.buttonHeight
    implicitWidth: Math.max(implicitHeight, contentItem.implicitWidth + leftPadding + rightPadding)

    leftPadding: root.showsText ? root.textPadding : 0
    rightPadding: root.showsText ? root.textPadding : 0
    topPadding: 0
    bottomPadding: 0

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: root.enabled ? 1.0 : AppTheme.opacityDisabled

    Accessible.name: root.text

    ToolTip.visible: (root.compactContent || root.hintOnControl) && root.hovered && root.text.length > 0
    ToolTip.text: root.keyHint.length > 0 ? qsTr("%1 (%2)").arg(root.text).arg(root.keyHint) : root.text
    ToolTip.delay: AppMetrics.tooltipDelay

    contentItem: Item
    {
        id: buttonContent

        readonly property real hintSpace: root.alignHintRight && root.showsHint
                                          ? hint.implicitWidth + contentRow.spacing : 0
        readonly property real inlineHintWidth: !root.alignHintRight && root.showsHint
                                                ? hint.implicitWidth + contentRow.spacing : 0

        implicitWidth: contentRow.implicitWidth + inlineHintWidth + 2 * hintSpace
        implicitHeight: Math.max(contentRow.implicitHeight, root.showsHint ? hint.implicitHeight : 0)

        RowLayout
        {
            id: contentRow

            x: (parent.width - width - parent.inlineHintWidth) / 2
            y: (parent.height - height) / 2
            width: root.alignHintRight ? Math.min(implicitWidth, Math.max(0, parent.width - 2 * parent.hintSpace)) : implicitWidth
            spacing: AppMetrics.gapSmall

            DesktopIcon
            {
                id: leadingIcon

                visible: root.iconName.length > 0
                name: root.iconName
                color: root.iconColour
                filled: root.iconFilled
                Layout.alignment: Qt.AlignVCenter
            }

            DesktopLabel
            {
                id: label

                visible: root.showsText
                text: root.text
                elide: root.alignHintRight ? Text.ElideRight : Text.ElideNone
                Layout.minimumWidth: root.alignHintRight ? 0 : implicitWidth
                font.pixelSize: root.large ? AppMetrics.fontLabel : AppMetrics.fontBody
                font.weight: Font.DemiBold
                color: root.contentColour
                Layout.alignment: Qt.AlignVCenter
            }

            DesktopIcon
            {
                id: trailingIcon

                visible: root.trailingIconName.length > 0 && !root.compactContent
                name: root.trailingIconName
                width: AppMetrics.iconSizeSmall
                height: AppMetrics.iconSizeSmall
                color: root.contentColour
                Layout.alignment: Qt.AlignVCenter
            }
        }

        DesktopKeyHint
        {
            id: hint

            x: root.alignHintRight ? buttonContent.width - width : contentRow.x + contentRow.width + contentRow.spacing
            y: (buttonContent.height - height) / 2

            visible: root.showsHint
            key: root.keyHint
            tint: root.contentColour
            opacity: AppTheme.opacitySecondary
        }
    }

    background: Rectangle
    {
        topLeftRadius: root.topRadius
        topRightRadius: root.topRadius
        bottomLeftRadius: root.bottomRadius
        bottomRightRadius: root.bottomRadius

        color:
        {
            if (root.filled)
            {
                const veil = root.down ? AppTheme.alphaStrong : (root.hovered ? AppTheme.alphaSoft : 0)

                return Qt.tint(root.fillColour, AppTheme.faded(root.paperColour, veil))
            }

            if (root.down)
            {
                return root.pressedColour
            }

            return root.hovered || root.checked ? root.hoverColour : AppTheme.faded(root.hoverColour, 0)
        }

        border.width: root.filled || root.tone === "ghost" ? 0
                    : AppMetrics.borderWidth / (root.hairlineOutline ? Screen.devicePixelRatio : 1)
        border.pixelAligned: !root.hairlineOutline
        border.color: root.tone === "dangerOutline" ? AppTheme.colourWrong : root.edgeColour

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopFocusRing
        {
            visible: root.visualFocus
            colour: root.onCover ? AppTheme.colourCoverInk : AppTheme.colourAccent
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
