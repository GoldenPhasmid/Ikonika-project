import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property string title: ""
    property string hint: ""
    property string blockedHint: ""
    property var options: []
    property int minimumEnabled: 1
    property bool lastRow: false

    property bool blocked: false

    signal toggled(var option)

    readonly property int enabledCount:
    {
        let count = 0

        for (const option of root.options)
        {
            if (option.enabled)
            {
                count++
            }
        }

        return count
    }

    function toggle(option)
    {
        if (option.enabled && root.enabledCount <= root.minimumEnabled)
        {
            root.blocked = true
            resetTimer.restart()

            return
        }

        root.toggled(option)
    }

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + (AppMetrics.gapMedium * 2)

    Timer
    {
        id: resetTimer

        interval: 1600
        onTriggered: root.blocked = false
    }

    ColumnLayout
    {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: AppMetrics.contentPadding
        anchors.rightMargin: AppMetrics.contentPadding
        anchors.verticalCenter: parent.verticalCenter

        spacing: AppMetrics.gapSmall

        Text
        {
            Layout.fillWidth: true

            visible: text.length > 0
            text: root.title
            font.pixelSize: AppMetrics.fontLabel
            font.weight: Font.Medium
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
        }

        Text
        {
            Layout.fillWidth: true

            visible: text.length > 0
            text: root.blocked ? root.blockedHint : root.hint
            font.pixelSize: AppMetrics.fontMeta
            font.family: AppMetrics.fontFamily
            color: root.blocked ? AppTheme.colourWrong : AppTheme.colourSubtle
            wrapMode: Text.WordWrap
        }

        RowLayout
        {
            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapTight
            Layout.bottomMargin: AppMetrics.gapSmall

            spacing: AppMetrics.gapSmall

            Repeater
            {
                model: root.options

                delegate: MobileToggleChip
                {
                    required property var modelData

                    label: modelData.label
                    iconSvg: modelData.icon || ""
                    signalLevel: modelData.signalLevel || 0
                    checked: modelData.enabled
                    enabled: modelData.available !== false
                    opacity: enabled ? 1.0 : AppTheme.opacityDisabled

                    onClicked: root.toggle(modelData)
                }
            }
        }
    }

    Rectangle
    {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.contentPadding

        width: parent.width - AppMetrics.contentPadding
        height: AppMetrics.borderWidth

        visible: !root.lastRow
        color: AppTheme.colourBorder
    }
}
