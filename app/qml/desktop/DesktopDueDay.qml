import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property string name: ""
    property string dayName: ""
    property int count: 0
    property real heat: 0

    signal clicked()

    spacing: AppMetrics.todayNameGap

    DesktopLabel
    {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredHeight: AppMetrics.todayLabelHeight
        verticalAlignment: Text.AlignVCenter
        text: root.name
        font.pixelSize: AppMetrics.fontMeta
        font.weight: Font.Medium
        color: AppTheme.colourMuted
    }

    Button
    {
        id: mark

        readonly property bool hasWords: root.count > 0

        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: AppMetrics.weekRingSize
        Layout.preferredHeight: AppMetrics.weekRingSize
        padding: 0

        enabled: mark.hasWords
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        scale: mark.hovered ? AppMetrics.todayMarkHoverScale : 1

        Behavior on scale { NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing } }

        Accessible.name: qsTr("%1, %n words due", "", root.count).arg(root.dayName)

        onClicked: root.clicked()

        contentItem: DesktopLabel
        {
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: root.count
            font.pixelSize: AppMetrics.fontMicro
            font.weight: mark.hasWords ? Font.Bold : Font.Medium
            font.features: { "tnum": 1 }
            color: mark.hasWords ? AppTheme.colourText : AppTheme.colourMuted
        }

        background: Rectangle
        {
            radius: width / 2
            color: mark.hasWords
                ? Qt.tint(AppTheme.colourSunken, AppTheme.faded(AppTheme.colourAccent,
                                                                 mark.hovered ? AppTheme.alphaHalf
                                                                              : AppTheme.alphaFaint + root.heat * AppTheme.alphaStrong))
                : AppTheme.colourSunken
            border.width: mark.hasWords ? AppMetrics.borderWidth : 0
            border.color: AppTheme.faded(AppTheme.colourAccent, 1 - AppTheme.alphaStrong)

            Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

            DesktopDashedRing
            {
                anchors.fill: parent
                visible: !mark.hasWords
            }

            DesktopFocusRing
            {
                visible: mark.visualFocus
            }
        }

        HoverHandler
        {
            cursorShape: mark.hasWords ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }
}
