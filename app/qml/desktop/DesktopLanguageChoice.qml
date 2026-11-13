import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property string code: ""
    property string detail: ""
    property bool selected: false

    implicitWidth: AppMetrics.languageChoiceWidth
    implicitHeight: contentColumn.implicitHeight + topPadding + bottomPadding

    padding: AppMetrics.gapMedium
    topPadding: AppMetrics.gapLarge
    bottomPadding: AppMetrics.gapMedium

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: root.enabled ? 1.0 : AppTheme.opacityDisabled

    Accessible.role: Accessible.RadioButton
    Accessible.name: root.text
    Accessible.description: root.detail
    Accessible.checkable: true
    Accessible.checked: root.selected

    contentItem: ColumnLayout
    {
        id: contentColumn

        spacing: AppMetrics.gapSmall

        LanguageFlag
        {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: AppMetrics.gapTight
            code: root.code
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.text
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            visible: root.detail.length > 0
            text: root.detail
            font.pixelSize: AppMetrics.fontSmall
            color: root.selected ? AppTheme.colourAccent : AppTheme.colourMuted
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusLarge

        color:
        {
            if (root.selected)
            {
                return Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaSoft))
            }

            return root.down ? AppTheme.colourSunken : (root.hovered ? AppTheme.colourFaint : AppTheme.colourPanel)
        }

        border.width: root.selected ? AppMetrics.focusRingWidth : AppMetrics.borderWidth
        border.color: root.selected ? AppTheme.colourAccent : (root.hovered ? AppTheme.colourOutline : AppTheme.colourBorder)

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: AppMetrics.radiusLarge
        }
    }

    HoverHandler
    {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
