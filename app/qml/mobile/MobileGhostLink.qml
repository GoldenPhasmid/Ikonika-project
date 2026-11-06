import QtQuick
import QtQuick.Layouts

Text
{
    id: root

    property string label: qsTr("Don't know")
    property bool interactive: true

    signal clicked()

    text: root.label
    font.pixelSize: AppMetrics.fontBody
    font.weight: Font.DemiBold
    font.family: AppMetrics.fontFamily
    font.underline: true
    color: AppTheme.colourSubtle
    opacity: root.interactive ? 1.0 : 0.5

    Layout.alignment: Qt.AlignHCenter

    MouseArea
    {
        anchors.fill: parent
        enabled: root.interactive
        onClicked: root.clicked()
    }
}
