import QtQuick
import QtQuick.Layouts

MobileSectionsDrawer
{
    id: root

    property alias heading: headingLabel.text
    default property alias options: body.data
    property alias footer: footerColumn.data

    accessibleName: root.heading
    accessibleRole: Accessible.Dialog
    rightSide: true
    panelWidth: Math.min(root.width - AppMetrics.buttonHeightLarge, AppMetrics.categoryPaneWidth * 1.8)

    content: ColumnLayout
    {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: AppMetrics.gapSmall
        Layout.rightMargin: AppMetrics.gapSmall
        spacing: AppMetrics.gapWide

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            Text
            {
                id: headingLabel

                Layout.fillWidth: true
                font.pixelSize: AppMetrics.fontTitle
                font.weight: Font.DemiBold
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
                elide: Text.ElideRight
            }

            MobileIconButton
            {
                iconName: "close"
                text: qsTr("Close")

                onClicked: root.close()
            }
        }

        Flickable
        {
            id: scroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: body.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout
            {
                id: body

                width: scroll.width
                spacing: AppMetrics.gapWide
            }
        }

        ColumnLayout
        {
            id: footerColumn

            Layout.fillWidth: true
            visible: children.length > 0
            spacing: AppMetrics.gapSmall
        }
    }
}
