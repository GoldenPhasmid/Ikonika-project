import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Item
{
    id: root

    required property string word
    required property string translation
    required property int stage

    property bool completed: false

    property bool editing: false

    signal clicked()
    signal editRequested()
    signal deleteRequested()

    Layout.fillWidth: true
    Layout.preferredHeight: 69

    RowLayout
    {
        anchors.fill: parent
        anchors.leftMargin: AppMetrics.contentPadding
        anchors.rightMargin: AppMetrics.gapCompact

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 2

            Text
            {
                Layout.fillWidth: true
                Layout.minimumWidth: 0

                text: root.word
                font.pixelSize: 16
                font.weight: Font.DemiBold
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
                elide: Text.ElideRight
            }

            Text
            {
                Layout.fillWidth: true
                Layout.minimumWidth: 0

                text: root.translation
                font.pixelSize: AppMetrics.fontBody
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
                elide: Text.ElideRight
            }
        }

        LearnedMark
        {
            Layout.alignment: Qt.AlignVCenter

            visible: !root.editing
            stage: root.stage
            completed: root.completed
            size: 18
        }

        RowLayout
        {
            visible: root.editing
            spacing: AppMetrics.gapSmall

            MobileActionIconButton
            {
                iconSvg: Icons.Icons.edit.path
                onClicked: root.editRequested()
            }

            MobileActionIconButton
            {
                iconSvg: Icons.Icons.trash.path
                onClicked: root.deleteRequested()
            }
        }
    }

    TapHandler
    {
        enabled: !root.editing
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.clicked()
    }
}
