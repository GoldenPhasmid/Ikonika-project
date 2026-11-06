import QtQuick
import QtQuick.Layouts

DesktopCard
{
    id: root

    property string title
    property string detail
    property real progress: -1
    property string error
    property bool dismissible: false

    signal dismissed()

    default property alias actions: actionRow.data

    Layout.fillWidth: true
    padding: AppMetrics.gapMedium
    spacing: AppMetrics.gapSmall
    border.color: AppTheme.colourAccent

    Accessible.role: Accessible.AlertMessage
    Accessible.name: root.title

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        DesktopIcon
        {
            Layout.alignment: Qt.AlignTop
            name: "arrow_down"
            width: AppMetrics.iconSizeSmall
            height: AppMetrics.iconSizeSmall
            color: AppTheme.colourAccent
        }

        DesktopLabel
        {
            id: titleLabel

            Layout.fillWidth: true
            text: root.title
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
        }

        DesktopIconButton
        {
            objectName: root.objectName + "_dismiss"
            visible: root.dismissible
            Layout.alignment: Qt.AlignTop
            iconName: "close"
            text: qsTr("Dismiss update")

            onClicked: root.dismissed()
        }
    }

    DesktopLabel
    {
        Layout.fillWidth: true
        Layout.leftMargin: titleLabel.x
        visible: text.length > 0
        text: root.detail
        font.pixelSize: AppMetrics.fontSmall
        color: AppTheme.colourSubtle
        wrapMode: Text.WordWrap
    }

    ColumnLayout
    {
        id: actionRow

        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall
    }

    DesktopProgressBar
    {
        Layout.fillWidth: true
        visible: root.progress >= 0
        fill: AppTheme.colourAccent
        progress: Math.max(root.progress, 0)
    }

    DesktopLabel
    {
        Layout.fillWidth: true
        visible: root.error.length > 0
        text: root.error
        font.pixelSize: AppMetrics.fontSmall
        color: AppTheme.colourWrong
        wrapMode: Text.WordWrap
    }
}
