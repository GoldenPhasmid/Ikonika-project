import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property int rank: 0
    property int maximumRank: 0
    property var levelMarks: []

    signal rankChosen(int rank)

    readonly property real curve: 0.45
    readonly property int shownRank: slider.pressed ? root.rankAt(slider.value) : root.rank
    readonly property var numberMarks: [1000, 5000, 10000, 20000, 50000].filter(Mark => Mark < root.maximumRank)

    function positionOf(value)
    {
        return root.maximumRank > 0 ? Math.pow(Math.min(value, root.maximumRank) / root.maximumRank, root.curve) : 0
    }

    function rankAt(position)
    {
        const raw = Math.round(root.maximumRank * Math.pow(position, 1 / root.curve))

        if (raw <= 0)
        {
            return 0
        }

        const step = Math.pow(10, Math.max(0, Math.floor(Math.log10(raw)) - 1))

        return Math.min(root.maximumRank, Math.round(raw / step) * step)
    }

    function shortNumber(value)
    {
        return value >= 1000 ? qsTr("%1k").arg(Math.round(value / 1000)) : String(value)
    }

    function levelColour(label)
    {
        return AppTheme.cefrColours[label] || AppTheme.colourBorder
    }

    spacing: AppMetrics.gapSmall

    DesktopLabel
    {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Words from")
        font.pixelSize: AppMetrics.fontSmall
        color: AppTheme.colourMuted
    }

    DesktopLabel
    {
        Layout.alignment: Qt.AlignHCenter
        text: root.shownRank > 0 ? qsTr("#%1").arg(Qt.locale().toString(root.shownRank, "f", 0)) : qsTr("the top")
        font.weight: Font.DemiBold
    }

    Item
    {
        id: rankScale

        Layout.fillHeight: true
        Layout.alignment: Qt.AlignHCenter
        implicitWidth: numberColumn.width + track.width + levelColumn.width + AppMetrics.gapSmall * 2

        readonly property real trackTop: AppMetrics.rankHandleSize / 2
        readonly property real trackHeight: height - AppMetrics.rankHandleSize

        function yOf(value)
        {
            return rankScale.trackTop + rankScale.trackHeight * (1 - root.positionOf(value))
        }

        Item
        {
            id: numberColumn

            width: numberMetrics.advanceWidth
            height: parent.height

            TextMetrics
            {
                id: numberMetrics

                font.family: AppMetrics.fontFamily
                font.pixelSize: AppMetrics.fontSmall
                text: qsTr("50k")
            }

            Repeater
            {
                model: root.numberMarks

                delegate: DesktopLabel
                {
                    required property int modelData

                    anchors.right: parent.right
                    y: rankScale.yOf(modelData) - height / 2
                    text: root.shortNumber(modelData)
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted
                }
            }
        }

        Item
        {
            id: track

            x: numberColumn.width + AppMetrics.gapSmall
            width: AppMetrics.rankTrackWidth + AppMetrics.gapSmall * 2
            height: parent.height

            Rectangle
            {
                id: trackBody

                anchors.horizontalCenter: parent.horizontalCenter
                y: rankScale.trackTop
                width: AppMetrics.rankTrackWidth
                height: rankScale.trackHeight
                radius: AppMetrics.radiusSmall
                color: AppTheme.colourSunken
                border.width: AppMetrics.borderWidth
                border.color: AppTheme.colourBorder

                Item
                {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: rankScale.trackHeight * root.positionOf(root.shownRank)
                    clip: true

                    Rectangle
                    {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: rankScale.trackHeight
                        radius: AppMetrics.radiusSmall
                        color: root.levelMarks.length > 0
                            ? root.levelColour(root.levelMarks[root.levelMarks.length - 1].label) : AppTheme.colourAccent

                        Rectangle
                        {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            visible: root.levelMarks.length > 0
                            height: root.levelMarks.length > 0
                                ? rankScale.trackHeight * root.positionOf(root.levelMarks[0].rank) + radius : 0
                            radius: AppMetrics.radiusSmall
                            color: root.levelMarks.length > 0 ? root.levelColour(root.levelMarks[0].label) : "transparent"
                        }

                        Repeater
                        {
                            model: Math.max(0, root.levelMarks.length - 1)

                            delegate: Rectangle
                            {
                                required property int index

                                readonly property var lower: root.levelMarks[index]
                                readonly property var upper: root.levelMarks[index + 1]

                                anchors.left: parent.left
                                anchors.right: parent.right
                                y: rankScale.trackHeight * (1 - root.positionOf(upper.rank))
                                height: rankScale.trackHeight * (root.positionOf(upper.rank) - root.positionOf(lower.rank))

                                gradient: Gradient
                                {
                                    GradientStop { position: 0.0; color: root.levelColour(upper.label) }
                                    GradientStop { position: 1.0; color: root.levelColour(lower.label) }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item
        {
            id: levelColumn

            x: track.x + track.width + AppMetrics.gapSmall
            width: levelMetrics.advanceWidth
            height: parent.height

            TextMetrics
            {
                id: levelMetrics

                font.family: AppMetrics.fontFamily
                font.pixelSize: AppMetrics.fontSmall
                font.weight: Font.DemiBold
                text: qsTr("C2")
            }

            Repeater
            {
                model: root.levelMarks

                delegate: DesktopLabel
                {
                    required property var modelData

                    y: rankScale.yOf(modelData.rank) - height / 2
                    text: modelData.label
                    font.pixelSize: AppMetrics.fontSmall
                    font.weight: Font.DemiBold
                    color: root.levelColour(modelData.label)
                }
            }
        }

        Slider
        {
            id: slider

            objectName: "triage_rank"

            x: track.x
            width: track.width
            height: parent.height
            orientation: Qt.Vertical
            from: 0
            to: 1
            value: root.positionOf(root.rank)
            padding: 0
            focusPolicy: Qt.NoFocus

            Accessible.name: qsTr("Words from rank %1").arg(root.shownRank)

            background: Item {}

            handle: Rectangle
            {
                x: 0
                y: slider.visualPosition * (slider.height - height)
                width: slider.width
                height: AppMetrics.rankHandleSize
                radius: height / 2
                color: AppTheme.colourText
                border.width: AppMetrics.borderWidth
                border.color: AppTheme.colourPanel
            }

            onPressedChanged:
            {
                if (!slider.pressed)
                {
                    root.rankChosen(root.rankAt(slider.value))
                }
            }

            HoverHandler
            {
                cursorShape: Qt.PointingHandCursor
            }
        }
    }
}
