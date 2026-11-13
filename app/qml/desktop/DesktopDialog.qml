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
    height: parent ? Math.min(implicitHeight, parent.height - AppMetrics.contentPadding * 2) : implicitHeight

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

        Flickable
        {
            id: bodyFlick

            readonly property real ringMargin: AppMetrics.focusRingWidth * 2
            readonly property real scrollGutter: AppMetrics.scrollBarWidth + AppMetrics.gapSmall / 2

            function reveal(item)
            {
                if (!item || !bodyFlick.interactive)
                {
                    return
                }

                let owner = item.parent

                while (owner && owner !== bodyColumn)
                {
                    owner = owner.parent
                }

                if (!owner)
                {
                    return
                }

                const top = item.mapToItem(bodyFlick.contentItem, 0, 0).y - bodyFlick.ringMargin
                const bottom = top + item.height + bodyFlick.ringMargin * 2
                const limit = bodyFlick.contentHeight - bodyFlick.height

                if (top < bodyFlick.contentY)
                {
                    bodyFlick.contentY = Math.max(0, top)
                }
                else if (bottom > bodyFlick.contentY + bodyFlick.height)
                {
                    bodyFlick.contentY = Math.min(limit, bottom - bodyFlick.height)
                }
            }

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: -bodyFlick.ringMargin
            Layout.rightMargin: -bodyFlick.ringMargin - bodyFlick.scrollGutter
            Layout.topMargin: -bodyFlick.ringMargin
            Layout.bottomMargin: -bodyFlick.ringMargin
            Layout.preferredHeight: bodyFlick.contentHeight
            visible: bodyColumn.children.length > 0
            contentWidth: width
            contentHeight: bodyColumn.implicitHeight + bodyFlick.ringMargin * 2
            interactive: bodyFlick.contentHeight > bodyFlick.height
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ScrollBar.vertical: DesktopScrollBar {}

            Connections
            {
                target: bodyFlick.Window.window

                function onActiveFocusItemChanged()
                {
                    bodyFlick.reveal(bodyFlick.Window.window.activeFocusItem)
                }
            }

            ColumnLayout
            {
                id: bodyColumn

                x: bodyFlick.ringMargin
                y: bodyFlick.ringMargin
                width: bodyFlick.width - bodyFlick.ringMargin * 2 - bodyFlick.scrollGutter
                spacing: AppMetrics.gapMedium
            }
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
