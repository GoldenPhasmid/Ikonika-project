import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ItemDelegate
{
    id: root

    property string description: ""
    property string iconName: ""
    property string flagCode: ""
    property bool selected: false
    property bool showsCheck: false
    property bool destructive: false
    property bool toggle: false
    property bool toggledOn: false
    property string trailingText: ""
    property real progress: -1

    Layout.fillWidth: true
    implicitHeight: Math.max(AppMetrics.buttonHeight,
                             contentColumn.implicitHeight + AppMetrics.gapSmall * 2)

    leftPadding: AppMetrics.gapCompact
    rightPadding: AppMetrics.gapCompact
    topPadding: 0
    bottomPadding: 0

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: root.enabled ? 1.0 : AppTheme.opacityDisabled

    Accessible.role: root.toggle ? Accessible.CheckBox : Accessible.MenuItem
    Accessible.name: root.text
    Accessible.description: root.description
    Accessible.checkable: root.toggle || root.showsCheck
    Accessible.checked: root.toggle ? root.toggledOn : root.selected

    contentItem: RowLayout
    {
        spacing: AppMetrics.gapCompact

        DesktopIcon
        {
            visible: root.iconName.length > 0
            name: root.iconName
            color: root.destructive ? AppTheme.colourWrong : AppTheme.colourText
        }

        LanguageFlag
        {
            visible: root.flagCode.length > 0
            code: root.flagCode
            implicitWidth: AppMetrics.flagWidthSmall
            implicitHeight: AppMetrics.flagWidthSmall * 3 / 4
        }

        ColumnLayout
        {
            id: contentColumn

            Layout.fillWidth: true
            spacing: 0

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.text
                font.weight: root.selected ? Font.DemiBold : Font.Normal
                color: root.destructive ? AppTheme.colourWrong : AppTheme.colourText
                elide: Text.ElideRight
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: root.description.length > 0
                text: root.description
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
                wrapMode: Text.WordWrap
            }
        }

        DesktopLabel
        {
            visible: root.trailingText.length > 0
            text: root.trailingText
            font.pixelSize: AppMetrics.fontSmall
            color: AppTheme.colourSubtle
        }

        RowLayout
        {
            visible: root.progress >= 0
            spacing: AppMetrics.gapSmall

            DesktopProgressBar
            {
                Layout.preferredWidth: AppMetrics.rowProgressWidth
                progress: root.progress
                fill: AppTheme.colourAccent
            }

            DesktopLabel
            {
                text: qsTr("%1%").arg(Math.round(root.progress * 100))
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
            }
        }

        DesktopIcon
        {
            visible: root.showsCheck && root.selected
            name: "check"
            width: AppMetrics.iconSizeSmall
            height: AppMetrics.iconSizeSmall
            color: AppTheme.colourAccent
        }

        DesktopSwitch
        {
            visible: root.toggle
            on: root.toggledOn
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusSmall
        color: root.down ? AppTheme.colourPressed
                         : (root.hovered || root.visualFocus ? AppTheme.colourHover : AppTheme.faded(AppTheme.colourHover, 0))

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }
    }

    HoverHandler
    {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
