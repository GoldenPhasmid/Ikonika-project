import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    property var options: []
    property var descriptions: []
    property var trailingTexts: []
    property int currentIndex: -1
    property var chosen: null

    readonly property int rowHeight: root.descriptions.length > 0 ? 64 : 52

    visible: false

    onDismissed: root.close()

    function open(title, model, selected, onChosen, details, trailing)
    {
        root.heading = title
        root.options = model
        root.descriptions = details || []
        root.trailingTexts = trailing || []
        root.currentIndex = selected
        root.chosen = onChosen
        root.present()
    }

    function close()
    {
        root.hide()
    }

    Flickable
    {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(choices.implicitHeight, root.options.length * root.rowHeight)

        contentWidth: width
        contentHeight: choices.implicitHeight
        clip: true

        ColumnLayout
        {
            id: choices

            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 0

            Repeater
            {
                model: root.options

                delegate: Item
                {
                    id: choice

                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    Layout.preferredHeight: root.rowHeight

                    Column
                    {
                        anchors.left: parent.left
                        anchors.right: trailingLabel.left
                        anchors.rightMargin: AppMetrics.gapSmall
                        anchors.verticalCenter: parent.verticalCenter

                        Text
                        {
                            width: parent.width
                            text: choice.modelData
                            font.pixelSize: AppMetrics.fontLabel
                            font.family: AppMetrics.fontFamily
                            color: AppTheme.colourText
                            elide: Text.ElideRight
                        }

                        Text
                        {
                            width: parent.width
                            visible: text.length > 0
                            text: root.descriptions[choice.index] || ""
                            font.pixelSize: AppMetrics.fontSmall
                            font.family: AppMetrics.fontFamily
                            color: AppTheme.colourSubtle
                            elide: Text.ElideRight
                        }
                    }

                    Text
                    {
                        id: trailingLabel

                        anchors.right: tick.left
                        anchors.rightMargin: tick.visible ? AppMetrics.gapSmall : 0
                        anchors.verticalCenter: parent.verticalCenter

                        text: root.trailingTexts[choice.index] || ""
                        font.pixelSize: AppMetrics.fontSmall
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourSubtle
                    }

                    DesktopIcon
                    {
                        id: tick

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter

                        name: "check"
                        visible: choice.index === root.currentIndex
                        width: AppMetrics.iconSizeSmall
                        height: width
                        color: AppTheme.colourText
                    }

                    Rectangle
                    {
                        anchors.bottom: parent.bottom

                        width: parent.width
                        height: AppMetrics.borderWidth
                        color: AppTheme.colourBorder
                        visible: choice.index < root.options.length - 1
                    }

                    Rectangle
                    {
                        anchors.fill: parent
                        anchors.leftMargin: -AppMetrics.gapSmall
                        anchors.rightMargin: -AppMetrics.gapSmall

                        color: AppTheme.colourFaint
                        opacity: rowPress.pressed ? 1.0 : 0.0
                        z: -1

                        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
                    }

                    MouseArea
                    {
                        id: rowPress

                        anchors.fill: parent

                        onClicked:
                        {
                            if (root.chosen)
                            {
                                root.chosen(choice.index)
                            }

                            root.close()
                        }
                    }
                }
            }
        }
    }
}
