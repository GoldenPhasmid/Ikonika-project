import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property string text
    property real relative: 0
    property bool hasProbability: true
    property int learnedStage: -1
    property bool inUse: false

    signal picked()

    readonly property bool learned: root.learnedStage >= 0
    readonly property int level: root.hasProbability ? Math.max(1, Math.round(root.relative * 4)) : 0
    readonly property real naturalWidth: chipRow.implicitWidth + chipRow.anchors.leftMargin + chipRow.anchors.rightMargin

    Layout.fillWidth: true
    Layout.preferredHeight: 44
    radius: AppMetrics.radiusMedium
    color:
    {
        if (root.inUse)
        {
            return AppTheme.colourText
        }

        if (root.learned)
        {
            return Qt.tint(AppTheme.colourPanel, Qt.rgba(AppTheme.colourAccent.r,
                                                        AppTheme.colourAccent.g,
                                                        AppTheme.colourAccent.b, 0.12))
        }

        return AppTheme.colourPanel
    }

    border.color:
    {
        if (root.inUse)
        {
            return AppTheme.colourAccent
        }

        if (root.learned)
        {
            return Qt.rgba(AppTheme.colourAccent.r, AppTheme.colourAccent.g,
                           AppTheme.colourAccent.b, 0.5)
        }

        return AppTheme.colourBorder
    }
    border.width: AppMetrics.borderWidth

    RowLayout
    {
        id: chipRow

        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 10

        Text
        {
            text: root.text
            font.pixelSize: AppMetrics.fontBody
            font.weight: Font.Medium
            font.family: AppMetrics.fontFamily
            color: root.inUse ? AppTheme.colourPanel : AppTheme.colourText
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        LearnedMark
        {
            stage: root.learnedStage
        }

        MobileSignalBars
        {
            visible: root.hasProbability
            level: root.level
            activeColor: root.inUse ? AppTheme.colourPanel : AppTheme.colourText
            inactiveColor: root.inUse ? AppTheme.colourSubtle : AppTheme.colourBorder
        }
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: root.picked()
    }
}
