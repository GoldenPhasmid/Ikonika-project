import QtQuick
import QtQuick.Layouts
import "../ReaderShelf.js" as Shelf

MobileBottomSheet
{
    id: root

    property string order: Shelf.libraryOrders[0]
    property bool reversed: false
    property string groupMode: Shelf.libraryGroups[0]
    property var directions: ({})

    signal orderChosen(string order)
    signal reversedChosen(bool reversed)
    signal groupChosen(string group)

    heading: qsTr("Sort books")
    visible: false

    onDismissed: root.close()

    function open()
    {
        root.present()
    }

    function close()
    {
        root.hide()
    }

    component Choice: MobileSettingsRow
    {
        id: choice

        property bool chosen: false

        DesktopIcon
        {
            visible: choice.chosen
            name: "check"
            width: AppMetrics.fontLabel
            height: width
            color: AppTheme.colourAccent
        }
    }

    Flickable
    {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(sortColumn.implicitHeight, root.maximumBodyHeight)
        contentWidth: width
        contentHeight: sortColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ColumnLayout
        {
            id: sortColumn

            width: parent.width
            spacing: AppMetrics.gapMedium

            MobileOptionListPanel
            {
                Repeater
                {
                    model: Shelf.libraryOrders

                    delegate: Choice
                    {
                        required property string modelData
                        required property int index

                        objectName: "reader_sort_" + modelData
                        title: Shelf.libraryOrderLabels()[index]
                        chosen: root.order === modelData
                        lastRow: index === Shelf.libraryOrders.length - 1

                        onClicked: root.orderChosen(modelData)
                    }
                }
            }

            MobileOptionListPanel
            {
                Repeater
                {
                    model: 2

                    delegate: Choice
                    {
                        required property int index

                        objectName: "reader_sort_direction_" + index
                        title: (root.directions[root.order] || ["", ""])[index]
                        chosen: root.reversed === (index === 1)
                        lastRow: index === 1

                        onClicked: root.reversedChosen(index === 1)
                    }
                }
            }

            Text
            {
                text: qsTr("Group by")
                font.pixelSize: AppMetrics.fontMicro
                font.weight: Font.DemiBold
                font.family: AppMetrics.fontFamily
                font.letterSpacing: AppMetrics.trackingWide
                font.capitalization: Font.AllUppercase
                color: AppTheme.colourMuted
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapSmall

                Repeater
                {
                    model: Shelf.libraryGroups

                    delegate: MobileFilterChip
                    {
                        required property string modelData
                        required property int index

                        objectName: "reader_group_" + modelData
                        Layout.fillWidth: true
                        label: Shelf.libraryGroupLabels()[index]
                        selected: root.groupMode === modelData

                        onClicked: root.groupChosen(modelData)
                    }
                }
            }
        }
    }
}
