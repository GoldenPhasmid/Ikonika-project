import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Rectangle
{
    id: root

    property string text: ""
    property string iconName: ""
    property color tint: AppTheme.colourCoverMuted

    readonly property string iconPath: Icons.Icons[root.iconName] ? Icons.Icons[root.iconName].path : ""

    signal clicked()

    implicitWidth: content.implicitWidth + AppMetrics.gapCompact * 2
    implicitHeight: 22
    radius: AppMetrics.radiusSmall
    color: tapHandler.pressed ? Qt.tint(root.tint, AppTheme.faded(AppTheme.colourText, AppTheme.alphaFaint)) : root.tint
    clip: true

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Open deck %1").arg(root.text)

    CoverStripe {}

    RowLayout
    {
        id: content

        anchors.centerIn: parent
        spacing: AppMetrics.gapTight

        MobileVectorIcon
        {
            visible: root.iconPath.length > 0
            Layout.preferredWidth: 12
            Layout.preferredHeight: 12

            svgPath: root.iconPath
            color: AppTheme.colourCoverInk
        }

        Text
        {
            text: root.text
            font.pixelSize: AppMetrics.fontMicro
            font.bold: true
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourCoverInk
        }
    }

    TapHandler
    {
        id: tapHandler

        onTapped: root.clicked()
    }
}
