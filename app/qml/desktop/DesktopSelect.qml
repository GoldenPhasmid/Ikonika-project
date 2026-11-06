import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

DesktopDropdownButton
{
    id: root

    property var options: []
    property var descriptions: []
    property var trailingTexts: []
    property int visibleRows: 0
    property int currentIndex: 0
    property string heading: ""

    signal chosen(int index)

    popover: optionsPopover
    text: root.currentIndex >= 0 && root.currentIndex < root.options.length ? root.options[root.currentIndex] : ""

    Accessible.name: root.heading.length > 0 ? qsTr("%1: %2").arg(root.heading).arg(root.text) : root.text

    DesktopPopover
    {
        id: optionsPopover

        y: root.height + AppMetrics.gapTight
        width: Math.max(root.width, root.descriptions.length > 0
                                    ? AppMetrics.popoverWidth
                                    : Math.ceil(optionColumn.implicitWidth) + optionsPopover.leftPadding + optionsPopover.rightPadding)

        contentItem: Flickable
        {
            implicitHeight: root.visibleRows > 0 && optionRepeater.count > root.visibleRows
                ? (optionRepeater.itemAt(root.visibleRows) || optionColumn).y
                : Math.min(optionColumn.implicitHeight, AppMetrics.popoverListHeight)
            contentHeight: optionColumn.implicitHeight
            contentWidth: width
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: DesktopScrollBar {}

            ColumnLayout
            {
                id: optionColumn

                width: parent.width
                spacing: 0

                Repeater
                {
                    id: optionRepeater

                    model: root.options

                    delegate: DesktopPopoverRow
                    {
                        required property string modelData
                        required property int index

                        objectName: root.objectName.length > 0 ? root.objectName + "_option_" + index : ""

                        text: modelData
                        description: root.descriptions[index] || ""
                        trailingText: root.trailingTexts[index] || ""
                        showsCheck: true
                        selected: index === root.currentIndex

                        onClicked:
                        {
                            optionsPopover.close()
                            root.chosen(index)
                        }
                    }
                }
            }
        }
    }
}
