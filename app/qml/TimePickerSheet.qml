import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property int minuteStep: 5
    property var chosen: null

    readonly property int rowHeight: 44

    visible: false

    function open(current, onChosen)
    {
        root.chosen = onChosen

        hours.currentIndex = Math.floor(current / 60)
        minutes.currentIndex = Math.round((current % 60) / root.minuteStep)
            % Math.floor(60 / root.minuteStep)

        root.visible = true
    }

    function close()
    {
        root.visible = false
    }

    function commit()
    {
        const picked = (hours.currentIndex * 60) + (minutes.currentIndex * root.minuteStep)

        if (root.chosen)
        {
            root.chosen(picked)
        }

        root.close()
    }

    Rectangle
    {
        anchors.fill: parent

        color: AppTheme.colourText
        opacity: 0.4

        TapHandler { onTapped: root.close() }
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        height: Math.min(parent.height * 0.7,
                         (AppMetrics.gapLarge * 3) + heading.implicitHeight
                         + (root.rowHeight * 5) + buttons.implicitHeight)

        radius: AppMetrics.radiusLarge
        color: AppTheme.colourPanel

        MetaLabel
        {
            id: heading

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppMetrics.gapLarge

            text: qsTr("Remind me at")
            horizontalAlignment: Text.AlignHCenter
        }

        Row
        {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: heading.bottom
            anchors.topMargin: AppMetrics.gapMedium
            anchors.bottom: buttons.top
            anchors.bottomMargin: AppMetrics.gapMedium

            spacing: AppMetrics.gapSmall

            Tumbler
            {
                id: hours

                width: 88
                height: parent.height

                model: 24
                wrap: true
                visibleItemCount: 5

                delegate: TimePickerDigit {}
            }

            Text
            {
                anchors.verticalCenter: parent.verticalCenter

                text: ":"
                font.pixelSize: AppMetrics.fontSubtitle
                color: AppTheme.colourText
            }

            Tumbler
            {
                id: minutes

                width: 88
                height: parent.height

                model: Math.floor(60 / root.minuteStep)
                wrap: true
                visibleItemCount: 5

                delegate: TimePickerDigit { step: root.minuteStep }
            }
        }

        RowLayout
        {
            id: buttons

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: AppMetrics.gapLarge

            spacing: AppMetrics.gapSmall

            WireButton
            {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.preferredHeight: AppMetrics.mobileLayout
                    ? AppMetrics.buttonHeightLarge : AppMetrics.buttonHeight

                text: qsTr("Cancel")
                onClicked: root.close()
            }

            WireButton
            {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.preferredHeight: AppMetrics.mobileLayout
                    ? AppMetrics.buttonHeightLarge : AppMetrics.buttonHeight

                text: qsTr("Set")
                filled: true
                onClicked: root.commit()
            }
        }
    }
}
