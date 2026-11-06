import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ScrollView
{
    id: root

    property alias text: area.text
    property alias placeholderText: area.placeholderText
    property string accessibleName: ""
    property Item nextItem: null

    Layout.fillWidth: true
    Layout.preferredHeight: AppMetrics.textAreaHeight

    TextArea
    {
        id: area

        color: AppTheme.colourText
        placeholderTextColor: AppTheme.colourMuted
        selectionColor: AppTheme.colourAccent
        selectedTextColor: AppTheme.colourAccentText
        font.family: AppMetrics.fontFamily
        font.pixelSize: AppMetrics.fontBody
        wrapMode: TextEdit.Wrap
        selectByMouse: true
        hoverEnabled: true
        padding: AppMetrics.gapCompact

        Accessible.name: root.accessibleName

        KeyNavigation.priority: KeyNavigation.BeforeItem
        KeyNavigation.tab: root.nextItem

        background: Rectangle
        {
            radius: AppMetrics.radiusMedium
            color: AppTheme.colourPanel
            border.width: AppMetrics.borderWidth
            border.color: area.activeFocus ? AppTheme.colourAccent
                                           : (area.hovered ? AppTheme.colourOutline : AppTheme.colourBorder)

            Behavior on border.color { ColorAnimation { duration: AppMetrics.motionFast } }
        }
    }
}
