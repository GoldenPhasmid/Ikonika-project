import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Popup
{
    id: root

    property string title: ""
    property string message: ""
    property bool dismissable: true
    property int titleAlignment: Text.AlignLeft
    property var afterClose: null

    default property alias body: bodyColumn.data
    property alias footer: footerRow.data
    property alias status: statusRow.data

    parent: Overlay.overlay
    anchors.centerIn: parent

    width: Math.min(AppMetrics.dialogWidth, parent ? parent.width - AppMetrics.contentPadding * 2 : AppMetrics.dialogWidth)
    padding: AppMetrics.cardPadding

    modal: true
    focus: true
    closePolicy: root.dismissable ? Popup.CloseOnEscape | Popup.CloseOnPressOutside : Popup.CloseOnEscape

    function closeThen(action)
    {
        root.afterClose = action
        root.close()
    }

    onClosed:
    {
        const action = root.afterClose

        root.afterClose = null

        if (action)
        {
            action()
        }
    }

    Overlay.modal: Rectangle
    {
        color: AppTheme.colourScrim

        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionBase } }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusLarge
        color: AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder
    }

    contentItem: ColumnLayout
    {
        spacing: AppMetrics.gapLarge

        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapSmall

                DesktopLabel
                {
                    Layout.fillWidth: true
                    text: root.title
                    horizontalAlignment: root.titleAlignment
                    font.pixelSize: AppMetrics.fontSubtitle
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap

                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                RowLayout
                {
                    id: statusRow

                    spacing: AppMetrics.gapSmall
                    visible: children.length > 0
                }
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: root.message.length > 0
                text: root.message
                color: AppTheme.colourSubtle
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout
        {
            id: bodyColumn

            Layout.fillWidth: true
            spacing: AppMetrics.gapMedium
            visible: children.length > 0
        }

        RowLayout
        {
            id: footerRow

            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: AppMetrics.gapSmall
            spacing: AppMetrics.gapSmall
        }
    }

    enter: Transition
    {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AppMetrics.motionBase }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
    }

    exit: Transition
    {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: AppMetrics.motionBase }
        NumberAnimation { property: "scale"; from: 1; to: 0.96; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
    }
}
