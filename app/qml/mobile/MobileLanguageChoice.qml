import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    property string code: ""
    property string label: ""
    property string detail: ""
    property bool selected: false
    property bool interactive: true

    signal clicked()

    property real tileWidth: 0

    Layout.preferredWidth: root.tileWidth
    implicitHeight: contentColumn.implicitHeight + AppMetrics.gapLarge + AppMetrics.gapMedium

    radius: AppMetrics.radiusLarge
    color: root.selected ? Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaSoft))
                         : (tapArea.pressed ? AppTheme.colourSunken : AppTheme.colourPanel)
    border.width: root.selected ? AppMetrics.selectedBorderWidth : AppMetrics.borderWidth
    border.color: root.selected ? AppTheme.colourAccent : AppTheme.colourBorder
    opacity: root.interactive ? 1.0 : AppTheme.opacityDisabled

    Accessible.role: Accessible.RadioButton
    Accessible.name: root.label
    Accessible.description: root.detail
    Accessible.checkable: true
    Accessible.checked: root.selected
    Accessible.onPressAction: if (root.interactive) root.clicked()

    ColumnLayout
    {
        id: contentColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: AppMetrics.gapSmall
        anchors.rightMargin: AppMetrics.gapSmall
        anchors.topMargin: AppMetrics.gapLarge
        spacing: AppMetrics.gapSmall

        LanguageFlag
        {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: AppMetrics.gapTight
            code: root.code
        }

        Text
        {
            Layout.fillWidth: true

            text: root.label
            font.pixelSize: AppMetrics.fontLabel
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text
        {
            Layout.fillWidth: true

            visible: root.detail.length > 0
            text: root.detail
            font.pixelSize: AppMetrics.fontSmall
            font.family: AppMetrics.fontFamily
            color: root.selected ? AppTheme.colourAccent : AppTheme.colourMuted
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }

    MouseArea
    {
        id: tapArea

        anchors.fill: parent
        enabled: root.interactive

        onClicked: root.clicked()
    }
}
