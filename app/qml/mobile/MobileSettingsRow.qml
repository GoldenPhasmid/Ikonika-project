import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property string title

    property string description: ""
    property bool lastRow: false
    property bool destructive: false

    property bool holdToConfirm: false
    property int holdDuration: 600
    property real fillProgress: 0

    default property alias control: controlHolder.data

    signal clicked()

    Layout.fillWidth: true
    implicitHeight: Math.max(60, label.implicitHeight + 28)

    ColumnLayout
    {
        id: label

        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.contentPadding
        anchors.right: controlHolder.left
        anchors.rightMargin: AppMetrics.gapCompact
        anchors.verticalCenter: parent.verticalCenter

        spacing: 2

        Text
        {
            Layout.fillWidth: true
            Layout.minimumWidth: 0

            text: root.title
            font.pixelSize: AppMetrics.fontLabel
            font.weight: Font.Medium
            font.family: AppMetrics.fontFamily
            color: root.destructive ? AppTheme.colourWrong : AppTheme.colourText
            elide: Text.ElideRight
        }

        Text
        {
            Layout.fillWidth: true
            Layout.minimumWidth: 0

            visible: root.description.length > 0
            text: root.description
            font.pixelSize: AppMetrics.fontMeta
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourSubtle
            wrapMode: Text.WordWrap
        }
    }

    Row
    {
        id: controlHolder

        anchors.right: parent.right
        anchors.rightMargin: AppMetrics.contentPadding
        anchors.verticalCenter: parent.verticalCenter

        spacing: AppMetrics.gapSmall
    }

    Rectangle
    {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.contentPadding

        width: parent.width - AppMetrics.contentPadding
        height: AppMetrics.borderWidth

        visible: !root.lastRow
        color: AppTheme.colourBorder
    }

    Rectangle
    {
        anchors.fill: parent

        color: AppTheme.colourFaint
        opacity: !root.holdToConfirm && rowPress.pressed ? 1.0 : 0.0
        z: -2

        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        width: parent.width * root.fillProgress
        color: AppTheme.colourFaint
        visible: root.holdToConfirm
        z: -2
    }

    NumberAnimation
    {
        id: fillUp
        target: root
        property: "fillProgress"
        to: 1.0
        duration: root.holdDuration

        onFinished: { root.fillProgress = 0; root.clicked() }
    }

    NumberAnimation
    {
        id: fillDown
        target: root
        property: "fillProgress"
        to: 0.0
        duration: AppMetrics.motionFast
    }

    MouseArea
    {
        id: rowPress

        anchors.fill: parent
        z: -1

        onClicked: if (!root.holdToConfirm) { root.clicked() }

        onPressedChanged:
        {
            if (!root.holdToConfirm)
            {
                return
            }

            if (pressed)
            {
                fillDown.stop()
                fillUp.start()
            }
            else
            {
                fillUp.stop()
                fillDown.start()
            }
        }
    }
}
